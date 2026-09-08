#define _POSIX_C_SOURCE 200809L
#include "hhy/compiler.h"
#include "hhy/common.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const char *const hhy_compiler_pass_names[HHY_COMPILER_PASS_COUNT] = {
    "fold", "constant-propagation", "copy-propagation", "peephole", "unreachable", "dce"
};
static HhyBytecodeResult status(bool ok, size_t at, const char *message) {
    HhyBytecodeResult r = {.ok = ok, .instruction = at};
    snprintf(r.message, sizeof(r.message), "%s", message);
    return r;
}
static uint64_t now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * UINT64_C(1000000000) + (uint64_t)t.tv_nsec;
}
void hhy_compiler_ir_free(HhyCompilerIR *ir) {
    if (!ir) return;
    for (size_t i = 0; i < ir->count; i++) free((void *)ir->nodes[i].token.start);
    free(ir->nodes); free(ir->blocks); memset(ir, 0, sizeof(*ir));
}
static bool count_nodes(const HhyNode *n, size_t depth, size_t *count) {
    if (!n || depth > HHY_BYTECODE_MAX_NESTING ||
        ++*count >= HHY_BYTECODE_MAX_INSTRUCTIONS ||
        n->kind < HHY_N_PROGRAM || n->kind > HHY_N_LITERAL ||
        n->child_count > HHY_BYTECODE_MAX_INSTRUCTIONS ||
        (n->child_count && !n->children) || (n->token.length && !n->token.start)) return false;
    for (size_t i = 0; i < n->child_count; i++)
        if (!count_nodes(n->children[i], depth + 1, count)) return false;
    return true;
}
static uint32_t lower_node(const HhyNode *n, HhyCompilerIR *ir, uint32_t parent) {
    uint32_t id = (uint32_t)ir->count++;
    HhyCompilerNode *x = &ir->nodes[id];
    *x = (HhyCompilerNode){.op = (unsigned)n->kind, .token = n->token,
        .parent = parent, .first_child = HHY_COMPILER_NONE, .next = HHY_COMPILER_NONE,
        .child_count = (uint32_t)n->child_count, .origin = HHY_COMPILER_NONE, .frame_slots = n->frame_slot_count,
        .local_depth = n->local_env_depth, .local_slot = n->local_binding_slot,
        .resolved = n->local_slot_resolved};
    x->token.start = hhy_strndup(n->token.start ? n->token.start : "", n->token.length);
    uint32_t previous = HHY_COMPILER_NONE;
    for (size_t i = 0; i < n->child_count; i++) {
        uint32_t child = lower_node(n->children[i], ir, id);
        if (previous == HHY_COMPILER_NONE) x->first_child = child;
        else ir->nodes[previous].next = child;
        previous = child;
    }
    return id;
}
static bool integer_token(HhyToken t, int64_t *out) {
    if (t.kind != HHY_T_INT || !t.length || t.length >= 128) return false;
    unsigned base = 10;
    size_t i = 0;
    if (t.length > 2 && t.start[0] == '0') {
        if (t.start[1] == 'x' || t.start[1] == 'X') {base = 16; i = 2;}
        if (t.start[1] == 'b' || t.start[1] == 'B') {base = 2; i = 2;}
    }
    uint64_t value = 0; bool digit_seen = false;
    for (; i < t.length; i++) {
        unsigned char c = (unsigned char)t.start[i];
        if (c == '_') continue;
        unsigned d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return false;
        if (d >= base || value > ((uint64_t)INT64_MAX - d) / base) return false;
        digit_seen = true; value = value * base + d;
    }
    *out = (int64_t)value; return digit_seen;
}
/* Fold only allocation-free scalar operations. Failed operations remain runtime
 * operations, with their original evaluation order and source locations. */
static bool scalar(const HhyCompilerIR *ir, uint32_t id,
                    HhyCompilerType *type, int64_t *value) {
    const HhyCompilerNode *x = &ir->nodes[id];
    if (x->op == HHY_COMPILER_CONST) {*type = x->type; *value = x->value; return true;}
    if (x->op == HHY_N_LITERAL) {
        *type = HHY_C_INT;
        if (integer_token(x->token, value)) return true;
        if (x->token.kind == HHY_T_TRUE || x->token.kind == HHY_T_FALSE) {
            *type = HHY_C_BOOL; *value = x->token.kind == HHY_T_TRUE; return true;
        }
        if (x->token.kind == HHY_T_NULL) {*type = HHY_C_NULL; *value = 0; return true;}
        return false;
    }
    if (x->op != HHY_N_UNARY && x->op != HHY_N_BINARY) return false;
    if (x->child_count != (x->op == HHY_N_UNARY ? 1u : 2u)) return false;
    uint32_t a = x->first_child;
    HhyCompilerType at = ir->nodes[a].type;
    int64_t av = ir->nodes[a].value;
    if (at == HHY_C_DYNAMIC || ir->nodes[a].effects) return false;
    *type = at;
    if (x->op == HHY_N_UNARY) {
        if (x->token.kind == HHY_T_NOT && at == HHY_C_BOOL) {*value = !av; return true;}
        if (at != HHY_C_INT) return false;
        if (x->token.kind == HHY_T_PLUS) {*value = av; return true;}
        if (x->token.kind == HHY_T_MINUS && av != INT64_MIN) {*value = -av; return true;}
        return false;
    }
    uint32_t b = ir->nodes[a].next;
    HhyCompilerType bt = ir->nodes[b].type;
    int64_t bv = ir->nodes[b].value;
    if (bt == HHY_C_DYNAMIC || ir->nodes[b].effects) return false;
    if (at == HHY_C_BOOL && bt == HHY_C_BOOL) {
        if (x->token.kind == HHY_T_AND) {*value = av && bv; return true;}
        if (x->token.kind == HHY_T_OR) {*value = av || bv; return true;}
    }
    if (at != HHY_C_INT || bt != HHY_C_INT) return false;
    switch (x->token.kind) {
        case HHY_T_PLUS: return !__builtin_add_overflow(av, bv, value);
        case HHY_T_MINUS: return !__builtin_sub_overflow(av, bv, value);
        case HHY_T_STAR: return !__builtin_mul_overflow(av, bv, value);
        case HHY_T_MOD:
            if (!bv) return false;
            *value = av == INT64_MIN && bv == -1 ? 0 : av % bv; return true;
        case HHY_T_EQUAL_EQUAL:
            if (av > INT64_C(9007199254740992) || av < -INT64_C(9007199254740992) ||
                bv > INT64_C(9007199254740992) || bv < -INT64_C(9007199254740992)) return false;
            *type = HHY_C_BOOL; *value = av == bv; return true;
        case HHY_T_BANG_EQUAL:
            if (av > INT64_C(9007199254740992) || av < -INT64_C(9007199254740992) ||
                bv > INT64_C(9007199254740992) || bv < -INT64_C(9007199254740992)) return false;
            *type = HHY_C_BOOL; *value = av != bv; return true;
        case HHY_T_LT: *type = HHY_C_BOOL; *value = av < bv; return true;
        case HHY_T_LTE: *type = HHY_C_BOOL; *value = av <= bv; return true;
        case HHY_T_GT: *type = HHY_C_BOOL; *value = av > bv; return true;
        case HHY_T_GTE: *type = HHY_C_BOOL; *value = av >= bv; return true;
        default: return false;
    }
}
static void metadata(HhyCompilerIR *ir) {
    for (size_t i = ir->count; i-- > 0;) {
        HhyCompilerNode *x = &ir->nodes[i];
        HhyCompilerType type = HHY_C_DYNAMIC; int64_t value = 0;
        if (scalar(ir, (uint32_t)i, &type, &value)) {
            x->type = type; x->value = value; x->effects = 0;
        } else {
            x->type = HHY_C_DYNAMIC; x->value = 0;
            /* Unknown names/calls/containers may allocate and throw. Treat all
             * non-proven regions as effect/cancellation barriers. */
            x->effects = HHY_C_THROW | HHY_C_CANCEL | HHY_C_ALLOC | HHY_C_EFFECT;
        }
    }
}
static uint32_t child_at(const HhyCompilerIR *ir, uint32_t id, size_t at) {
    uint32_t child = ir->nodes[id].first_child;
    while (at-- && child != HHY_COMPILER_NONE) child = ir->nodes[child].next;
    return child;
}
static void cfg_node(const HhyCompilerIR *ir, HhyCompilerBlock *blocks, uint32_t id,
                     uint32_t next, uint32_t owner, uint32_t handler,
                     uint32_t break_to, uint32_t continue_to) {
    const HhyCompilerNode *x = &ir->nodes[id];
    HhyCompilerBlock *b = &blocks[id];
    *b = (HhyCompilerBlock){.present = true, .node = id, .owner = owner,
        .next = next, .alternate = HHY_COMPILER_NONE,
        .exception = x->effects & HHY_C_THROW ? handler : HHY_COMPILER_NONE};
    uint32_t first = x->first_child;
    if (x->op == HHY_N_BLOCK || x->op == HHY_N_PROGRAM) {
        b->next = first == HHY_COMPILER_NONE ? next : first;
        for (uint32_t c = first; c != HHY_COMPILER_NONE; c = ir->nodes[c].next)
            cfg_node(ir, blocks, c, ir->nodes[c].next == HHY_COMPILER_NONE ? next : ir->nodes[c].next,
                     owner, handler, break_to, continue_to);
    } else if (x->op == HHY_N_IF) {
        uint32_t yes = child_at(ir, id, 1), no = child_at(ir, id, 2);
        b->next = yes; b->alternate = no == HHY_COMPILER_NONE ? next : no;
        const HhyCompilerNode *condition = &ir->nodes[first];
        if (condition->type == HHY_C_BOOL && condition->effects == 0) {
            if (!condition->value) b->next = b->alternate;
            b->alternate = HHY_COMPILER_NONE;
        }
        cfg_node(ir, blocks, yes, next, owner, handler, break_to, continue_to);
        if (no != HHY_COMPILER_NONE) cfg_node(ir, blocks, no, next, owner, handler, break_to, continue_to);
    } else if (x->op == HHY_N_WHILE || x->op == HHY_N_FOR) {
        uint32_t body = child_at(ir, id, x->op == HHY_N_FOR ? 2 : 1);
        uint32_t header = id;
        if (x->op == HHY_N_FOR) {
            header = (uint32_t)ir->count + id;
            blocks[header] = *b; blocks[header].phase = 1;
            b->next = header; b = &blocks[header];
        }
        b->next = body; b->alternate = next;
        if (x->op == HHY_N_WHILE && ir->nodes[first].type == HHY_C_BOOL &&
            ir->nodes[first].effects == 0) {
            if (!ir->nodes[first].value) b->next = next;
            b->alternate = HHY_COMPILER_NONE;
        }
        cfg_node(ir, blocks, body, header, owner, handler, next, header);
    } else if (x->op == HHY_N_TRY) {
        uint32_t binding = child_at(ir, id, 1), caught = child_at(ir, id, 2);
        b->next = first;
        blocks[binding] = (HhyCompilerBlock){.present = true, .node = binding,
            .owner = owner, .phase = 2, .next = caught, .alternate = HHY_COMPILER_NONE,
            .exception = handler};
        cfg_node(ir, blocks, first, next, owner, binding, break_to, continue_to);
        cfg_node(ir, blocks, caught, next, owner, handler, break_to, continue_to);
    } else if (x->op == HHY_N_RETURN) b->next = HHY_COMPILER_NONE;
    else if (x->op == HHY_N_BREAK) b->next = break_to;
    else if (x->op == HHY_N_CONTINUE) b->next = continue_to;
    else if (x->op == HHY_N_EXPORT_DECL) {
        b->next = first;
        cfg_node(ir, blocks, first, next, owner, handler, break_to, continue_to);
    }
}
static void cfg_callables(const HhyCompilerIR *ir, HhyCompilerBlock *blocks, uint32_t id) {
    const HhyCompilerNode *x = &ir->nodes[id];
    if (x->op == HHY_N_FN_DECL || x->op == HHY_N_CLOSURE) {
        uint32_t body = child_at(ir, id, x->child_count - 1);
        /* Closure bodies can be a sequence of statements (not a BLOCK). */
        if (x->op == HHY_N_CLOSURE) {
            body = x->first_child;
            while (body != HHY_COMPILER_NONE && ir->nodes[body].op == HHY_N_IDENTIFIER)
                body = ir->nodes[body].next;
        }
        for (uint32_t c = body; c != HHY_COMPILER_NONE; c = ir->nodes[c].next)
            cfg_node(ir, blocks, c, ir->nodes[c].next, id, HHY_COMPILER_NONE,
                     HHY_COMPILER_NONE, HHY_COMPILER_NONE);
    }
    for (uint32_t c = x->first_child; c != HHY_COMPILER_NONE; c = ir->nodes[c].next)
        cfg_callables(ir, blocks, c);
}
static void cfg(const HhyCompilerIR *ir, HhyCompilerBlock *blocks) {
    memset(blocks, 0, 2 * ir->count * sizeof(*blocks));
    cfg_node(ir, blocks, 0, HHY_COMPILER_NONE, 0, HHY_COMPILER_NONE,
             HHY_COMPILER_NONE, HHY_COMPILER_NONE);
    cfg_callables(ir, blocks, 0);
}
static bool arity(const HhyCompilerNode *x) {
    switch (x->op) {
        case HHY_N_IF: return x->child_count == 2 || x->child_count == 3;
        case HHY_N_WHILE: case HHY_N_BINARY: case HHY_N_PIPE:
        case HHY_N_ASSIGN: case HHY_N_INDEX: case HHY_N_RANGE: return x->child_count == 2;
        case HHY_N_TRY: case HHY_N_FOR: return x->child_count == 3;
        case HHY_N_UNARY: case HHY_N_EXPR_STMT: case HHY_N_EXPORT_DECL:
        case HHY_N_MEMBER: case HHY_N_MAP_ENTRY: case HHY_N_ATTEMPT: return x->child_count == 1;
        case HHY_N_RETURN: return x->child_count <= 1;
        case HHY_N_LET_DECL: return x->child_count == 2 || x->child_count == 3;
        case HHY_N_FN_DECL: return x->child_count >= 2;
        case HHY_N_CALL: return x->child_count >= 1;
        case HHY_N_BREAK: case HHY_N_CONTINUE: case HHY_N_LITERAL:
        case HHY_N_IDENTIFIER: case HHY_COMPILER_CONST: return x->child_count == 0;
        default: return true;
    }
}
static bool value_node(unsigned op) {
    return op >= HHY_N_ASSIGN && op <= HHY_COMPILER_CONST && op != HHY_N_MAP_ENTRY;
}
static bool role_shape(const HhyCompilerIR *ir, uint32_t id) {
    const HhyCompilerNode *x = &ir->nodes[id];
    uint32_t a = child_at(ir, id, 0), b = child_at(ir, id, 1), c = child_at(ir, id, 2);
    switch (x->op) {
        case HHY_N_FN_DECL:
            for (uint32_t n = a; ir->nodes[n].next != HHY_COMPILER_NONE; n = ir->nodes[n].next)
                if (ir->nodes[n].op != HHY_N_IDENTIFIER) return false;
            return ir->nodes[child_at(ir, id, x->child_count - 1)].op == HHY_N_BLOCK;
        case HHY_N_LET_DECL:
            return ir->nodes[x->child_count == 3 ? b : a].op == HHY_N_IDENTIFIER &&
                value_node(ir->nodes[x->child_count == 3 ? c : b].op);
        case HHY_N_IF:
            return value_node(ir->nodes[a].op) && ir->nodes[b].op == HHY_N_BLOCK &&
                (c == HHY_COMPILER_NONE || ir->nodes[c].op == HHY_N_BLOCK || ir->nodes[c].op == HHY_N_IF);
        case HHY_N_WHILE:
            return value_node(ir->nodes[a].op) && ir->nodes[b].op == HHY_N_BLOCK;
        case HHY_N_FOR:
            return ir->nodes[a].op == HHY_N_IDENTIFIER && value_node(ir->nodes[b].op) && ir->nodes[c].op == HHY_N_BLOCK;
        case HHY_N_TRY:
            return ir->nodes[a].op == HHY_N_BLOCK && ir->nodes[b].op == HHY_N_IDENTIFIER && ir->nodes[c].op == HHY_N_BLOCK;
        case HHY_N_ATTEMPT: return ir->nodes[a].op == HHY_N_BLOCK;
        case HHY_N_MAP:
            for (uint32_t n = a; n != HHY_COMPILER_NONE; n = ir->nodes[n].next)
                if (ir->nodes[n].op != HHY_N_MAP_ENTRY) return false;
            return true;
        case HHY_N_BLOCK: case HHY_N_PROGRAM: case HHY_N_CLOSURE:
            if (x->op == HHY_N_CLOSURE && a != HHY_COMPILER_NONE && ir->nodes[a].op == HHY_N_IDENTIFIER)
                a = ir->nodes[a].next;
            for (uint32_t n = a; n != HHY_COMPILER_NONE; n = ir->nodes[n].next)
                if (ir->nodes[n].op > HHY_N_EXPR_STMT || ir->nodes[n].op == HHY_N_PROGRAM) return false;
            return true;
        case HHY_N_IMPORT_DECL: case HHY_N_EXPORT_DECL: return true;
        default:
            for (uint32_t n = a; n != HHY_COMPILER_NONE; n = ir->nodes[n].next)
                if (!value_node(ir->nodes[n].op)) return false;
            return true;
    }
}
static bool verify_tree(const HhyCompilerIR *ir, uint32_t id, uint32_t parent, size_t depth,
                        unsigned char *seen) {
    if (id >= ir->count || seen[id] || depth > HHY_BYTECODE_MAX_NESTING) return false;
    seen[id] = 1;
    const HhyCompilerNode *x = &ir->nodes[id];
    if (x->parent != parent || !arity(x)) return false;
    size_t count = 0; uint32_t previous = id;
    for (uint32_t c = x->first_child; c != HHY_COMPILER_NONE; c = ir->nodes[c].next) {
        if (c <= previous || !verify_tree(ir, c, id, depth + 1, seen)) return false;
        previous = c; count++;
    }
    return count == x->child_count && role_shape(ir, id);
}
static uint32_t preceding_binding(const HhyCompilerIR *ir, uint32_t region,
                                  uint32_t stop, HhyToken name) {
    uint32_t found = HHY_COMPILER_NONE;
    for (uint32_t c = ir->nodes[region].first_child; c != HHY_COMPILER_NONE && c != stop;
         c = ir->nodes[c].next) {
        const HhyCompilerNode *x = &ir->nodes[c];
        if (x->op != HHY_N_LET_DECL) continue;
        uint32_t named = child_at(ir, c, x->child_count == 3 ? 1 : 0);
        HhyToken token = ir->nodes[named].token;
        if (name.length == token.length && !memcmp(name.start, token.start, name.length)) found = c;
    }
    return found;
}
static bool binding_value(const HhyCompilerIR *ir, uint32_t declaration,
                          HhyCompilerType type, int64_t value) {
    /* Copy chains strictly decrease instruction IDs: bounded, no phi/cycles. */
    while (declaration != HHY_COMPILER_NONE) {
        const HhyCompilerNode *d = &ir->nodes[declaration];
        if (d->op != HHY_N_LET_DECL || d->child_count != 2) return false;
        const HhyCompilerNode *v = &ir->nodes[child_at(ir, declaration, 1)];
        if (v->effects == 0 && v->type == type && v->value == value) return true;
        if (v->op != HHY_N_IDENTIFIER) return false;
        uint32_t previous = preceding_binding(ir, d->parent, declaration, v->token);
        if (previous >= declaration) return false;
        declaration = previous;
    }
    return false;
}
static bool verify_origins(const HhyCompilerIR *ir, const unsigned char *seen) {
    for (size_t i = 0; i < ir->count; i++) {
        const HhyCompilerNode *x = &ir->nodes[i];
        if (!seen[i] || x->origin == HHY_COMPILER_NONE) continue;
        if (x->op != HHY_COMPILER_CONST || x->origin >= i ||
            ir->nodes[x->origin].op != HHY_N_LET_DECL) return false;
        uint32_t region = ir->nodes[x->origin].parent;
        if (region >= ir->count || (ir->nodes[region].op != HHY_N_BLOCK &&
            ir->nodes[region].op != HHY_N_PROGRAM)) return false;
        uint32_t statement = (uint32_t)i;
        while (statement != HHY_COMPILER_NONE && ir->nodes[statement].parent != region)
            statement = ir->nodes[statement].parent;
        if (statement == HHY_COMPILER_NONE ||
            preceding_binding(ir, region, statement, x->token) != x->origin ||
            !binding_value(ir, x->origin, x->type, x->value)) return false;
    }
    return true;
}
static HhyNode *backend_node(const HhyCompilerIR *, uint32_t);
static void backend_free(HhyNode *);
static void clear_backend_slots(HhyNode *node) {
    node->local_slot_resolved = false;
    node->local_env_depth = node->local_binding_slot = node->frame_slot_count = 0;
    for (size_t i = 0; i < node->child_count; i++) clear_backend_slots(node->children[i]);
}
static bool compare_resolved_uses(const HhyCompilerIR *ir, uint32_t id, const HhyNode *view) {
    const HhyCompilerNode *x = &ir->nodes[id];
    if (x->op == HHY_COMPILER_CONST) return !x->resolved;
    if (x->resolved && (!view->local_slot_resolved || x->local_depth != view->local_env_depth ||
        x->local_slot != view->local_binding_slot)) return false;
    uint32_t child = x->first_child;
    for (size_t i = 0; i < view->child_count; i++) {
        if (!compare_resolved_uses(ir, child, view->children[i])) return false;
        child = ir->nodes[child].next;
    }
    return true;
}
HhyBytecodeResult hhy_compiler_verify(const HhyCompilerIR *ir) {
    if (!ir || ir->version != HHY_COMPILER_IR_VERSION || !ir->count ||
        ir->count > ir->capacity || ir->capacity >= HHY_BYTECODE_MAX_INSTRUCTIONS ||
        !ir->nodes || !ir->blocks || ir->nodes[0].op != HHY_N_PROGRAM ||
        ir->nodes[0].next != HHY_COMPILER_NONE)
        return status(false, 0, "invalid compiler IR header");
    for (size_t i = 0; i < ir->count; i++) {
        const HhyCompilerNode *x = &ir->nodes[i];
        if (x->op > HHY_COMPILER_CONST || x->token.kind < HHY_T_EOF || x->token.kind > HHY_T_NOT ||
            !x->token.start || x->token.length > UINT32_MAX ||
            ((x->token.line == 0) != (x->token.column == 0)) ||
            x->frame_slots > UINT32_MAX || x->local_depth > UINT32_MAX || x->local_slot > UINT32_MAX ||
            (x->first_child != HHY_COMPILER_NONE && (x->first_child <= i || x->first_child >= ir->count)) ||
            (x->next != HHY_COMPILER_NONE && (x->next <= i || x->next >= ir->count)) ||
            (x->op == HHY_COMPILER_CONST && (x->type < HHY_C_INT || x->type > HHY_C_NULL ||
              (x->type == HHY_C_BOOL && x->value != 0 && x->value != 1) ||
              (x->type == HHY_C_NULL && x->value != 0))))
            return status(false, i, "invalid compiler IR instruction");
    }
    /* Inactive nodes also remain owned by the arena. Validate their child
     * chains before metadata reconstruction; no traversal may trust cached arity. */
    for (size_t i = 0; i < ir->count; i++) {
        const HhyCompilerNode *x = &ir->nodes[i];
        size_t children = 0;
        for (uint32_t c = x->first_child; c != HHY_COMPILER_NONE; c = ir->nodes[c].next) children++;
        if (children != x->child_count || !arity(x))
            return status(false, i, "invalid IR instruction arity");
    }
    unsigned char *seen = hhy_alloc(ir->count);
    bool tree_ok = verify_tree(ir, 0, HHY_COMPILER_NONE, 0, seen);
    if (tree_ok) tree_ok = verify_origins(ir, seen);
    free(seen);
    if (!tree_ok) return status(false, 0, "invalid IR region / definition ownership");
    HhyCompilerIR scratch = *ir;
    scratch.nodes = hhy_alloc(ir->count * sizeof(*scratch.nodes));
    memcpy(scratch.nodes, ir->nodes, ir->count * sizeof(*scratch.nodes));
    metadata(&scratch);
    bool valid = true;
    for (size_t i = 0; i < ir->count; i++)
        if (ir->nodes[i].effects != scratch.nodes[i].effects ||
            ir->nodes[i].type != scratch.nodes[i].type || ir->nodes[i].value != scratch.nodes[i].value) valid = false;
    free(scratch.nodes);
    if (!valid) return status(false, 0, "invalid IR type / effect / constant metadata");
    HhyCompilerBlock *expected = hhy_alloc(2 * ir->count * sizeof(*expected));
    cfg(ir, expected);
    for (size_t i = 0; i < 2 * ir->count; i++) {
        HhyCompilerBlock a = ir->blocks[i], b = expected[i];
        if (a.present != b.present || a.node != b.node || a.owner != b.owner ||
            a.phase != b.phase || a.next != b.next || a.alternate != b.alternate ||
            a.exception != b.exception) valid = false;
    }
    free(expected);
    if (valid) {
        /* Re-resolve independently from the optimizer's facts. Dynamic/unresolved
         * names retain runtime checks; each direct slot must match a preceding
         * definition in the right lexical/function scope. Frame capacities stay
         * at their original values because quota/allocation behavior is observable. */
        HhyNode *view = backend_node(ir, 0);
        clear_backend_slots(view); hhy_resolve_slots(view);
        valid = compare_resolved_uses(ir, 0, view);
        backend_free(view);
    }
    return status(valid, 0, valid ? "verified" : "invalid IR CFG / exception / callable edge / resolved use");
}
HhyBytecodeResult hhy_compiler_lower(const HhyNode *program, HhyCompilerIR *out) {
    size_t count = 0;
    if (!out || !count_nodes(program, 0, &count)) return status(false, 0, "invalid or oversized compiler input");
    HhyCompilerIR ir = {.version = HHY_COMPILER_IR_VERSION, .capacity = count};
    ir.nodes = hhy_alloc(count * sizeof(*ir.nodes));
    ir.blocks = hhy_alloc(2 * count * sizeof(*ir.blocks));
    lower_node(program, &ir, HHY_COMPILER_NONE);
    /* Check structural shape before deriving metadata or CFG. */
    unsigned char *seen = hhy_alloc(count);
    bool valid = ir.nodes[0].op == HHY_N_PROGRAM && verify_tree(&ir, 0, HHY_COMPILER_NONE, 0, seen);
    free(seen);
    if (!valid) {hhy_compiler_ir_free(&ir); return status(false, 0, "invalid compiler input shape");}
    metadata(&ir); cfg(&ir, ir.blocks);
    HhyBytecodeResult result = hhy_compiler_verify(&ir);
    if (!result.ok) {hhy_compiler_ir_free(&ir); return result;}
    hhy_compiler_ir_free(out); *out = ir; return result;
}
static size_t live_nodes(const HhyCompilerIR *ir, uint32_t id) {
    size_t count = 1;
    for (uint32_t c = ir->nodes[id].first_child; c != HHY_COMPILER_NONE; c = ir->nodes[c].next)
        count += live_nodes(ir, c);
    return count;
}
static void make_constant(HhyCompilerNode *x, HhyCompilerType type, int64_t value) {
    x->origin = HHY_COMPILER_NONE;
    x->op = HHY_COMPILER_CONST; x->first_child = HHY_COMPILER_NONE; x->child_count = 0;
    x->type = type; x->value = value; x->effects = 0; x->resolved = false;
    x->local_depth = x->local_slot = 0;
}
static void fold_pass(HhyCompilerIR *ir, uint32_t id, bool peephole, size_t *changes) {
    HhyCompilerNode *x = &ir->nodes[id];
    for (uint32_t c = x->first_child; c != HHY_COMPILER_NONE; c = ir->nodes[c].next)
        fold_pass(ir, c, peephole, changes);
    if (x->op != HHY_N_BINARY && x->op != HHY_N_UNARY) return;
    HhyCompilerType type; int64_t value;
    if (!scalar(ir, id, &type, &value)) return;
    if (peephole) {
        uint32_t a = x->first_child, b = ir->nodes[a].next;
        bool identity = x->op == HHY_N_UNARY && x->token.kind == HHY_T_PLUS;
        if (b != HHY_COMPILER_NONE && type == HHY_C_INT) {
            int64_t right = ir->nodes[b].value;
            identity |= (x->token.kind == HHY_T_PLUS || x->token.kind == HHY_T_MINUS) && right == 0;
            identity |= x->token.kind == HHY_T_STAR && right == 1;
        }
        if (!identity) return;
    }
    /* Signed constants lower to a small existing-opcode tree; never grow code. */
    size_t emitted = type == HHY_C_INT && value < 0 ? (value == INT64_MIN ? 5 : 2) : 1;
    if (emitted >= live_nodes(ir, id)) return;
    make_constant(x, type, value); (*changes)++;
}
typedef struct Fact Fact;
struct Fact {HhyToken name; HhyCompilerType type; int64_t value; bool copy; uint32_t declaration; Fact *previous;};
static const Fact *fact_find(const Fact *facts, HhyToken name) {
    for (; facts; facts = facts->previous)
        if (facts->name.length == name.length && !memcmp(facts->name.start, name.start, name.length)) return facts;
    return NULL;
}
static void propagate_region(HhyCompilerIR *, uint32_t, bool, size_t *);
static void propagate_expression(HhyCompilerIR *ir, uint32_t id, const Fact *facts,
                                 bool copies, size_t *changes) {
    HhyCompilerNode *x = &ir->nodes[id];
    if (x->op == HHY_N_FN_DECL || x->op == HHY_N_CLOSURE ||
        x->op == HHY_N_BLOCK || x->op == HHY_N_PROGRAM) {
        propagate_region(ir, id, copies, changes); return;
    }
    if (x->op == HHY_N_IDENTIFIER) {
        const Fact *f = fact_find(facts, x->token);
        if (f && f->type != HHY_C_DYNAMIC && (!copies || f->copy) &&
            /* A load is one instruction. Avoid expanding signed constants. */
            (f->type != HHY_C_INT || f->value >= 0)) {
            make_constant(x, f->type, f->value); x->origin = f->declaration; (*changes)++;
        }
        return;
    }
    if (x->op == HHY_N_ASSIGN || x->op == HHY_N_IMPORT_DECL || x->op == HHY_N_EXPORT_DECL ||
        x->op == HHY_N_LET_DECL || x->op == HHY_N_FOR || x->op == HHY_N_TRY) {
        /* Binding/target positions are not value uses. Descend only into independent
         * BLOCK regions, never substitute a declaration or assignment target. */
        for (uint32_t c = x->first_child; c != HHY_COMPILER_NONE; c = ir->nodes[c].next)
            if (ir->nodes[c].op == HHY_N_BLOCK) propagate_region(ir, c, copies, changes);
        return;
    }
    for (uint32_t c = x->first_child; c != HHY_COMPILER_NONE; c = ir->nodes[c].next)
        propagate_expression(ir, c, facts, copies, changes);
}
static void propagate_region(HhyCompilerIR *ir, uint32_t id, bool copies, size_t *changes) {
    HhyCompilerNode *x = &ir->nodes[id];
    if (x->op != HHY_N_PROGRAM && x->op != HHY_N_BLOCK) {
        for (uint32_t c = x->first_child; c != HHY_COMPILER_NONE; c = ir->nodes[c].next)
            if (ir->nodes[c].op != HHY_N_IDENTIFIER) propagate_expression(ir, c, NULL, copies, changes);
        return;
    }
    Fact *facts = NULL;
    for (uint32_t c = x->first_child; c != HHY_COMPILER_NONE; c = ir->nodes[c].next) {
        HhyCompilerNode *s = &ir->nodes[c];
        if (s->op == HHY_N_LET_DECL) {
            bool immutable = s->child_count == 2;
            uint32_t name = child_at(ir, c, immutable ? 0 : 1), value_id = ir->nodes[name].next;
            HhyCompilerNode *v = &ir->nodes[value_id];
            const Fact *source = v->op == HHY_N_IDENTIFIER ? fact_find(facts, v->token) : NULL;
            bool is_copy = source && source->type != HHY_C_DYNAMIC;
            HhyCompilerType type = is_copy ? source->type : v->type;
            int64_t value = is_copy ? source->value : v->value;
            propagate_expression(ir, value_id, facts, copies, changes);
            Fact *f = hhy_alloc(sizeof(*f));
            *f = (Fact){.name = ir->nodes[name].token, .type = immutable ? type : HHY_C_DYNAMIC,
                .value = value, .copy = is_copy, .declaration = c, .previous = facts};
            facts = f;
        } else {
            propagate_expression(ir, c, facts, copies, changes);
            /* Conservatively discard facts after control/effect statements. A later
             * pass can add dataflow joins without relying on mutable name lookup. */
            if (s->op != HHY_N_EXPR_STMT || ir->nodes[s->first_child].effects)
                while (facts) {Fact *p = facts->previous; free(facts); facts = p;}
        }
    }
    while (facts) {Fact *p = facts->previous; free(facts); facts = p;}
}
static bool terminates(const HhyCompilerIR *ir, uint32_t id) {
    const HhyCompilerNode *x = &ir->nodes[id];
    if (x->op == HHY_N_RETURN || x->op == HHY_N_BREAK || x->op == HHY_N_CONTINUE) return true;
    if (x->op == HHY_N_IF && x->child_count == 3)
        return terminates(ir, child_at(ir, id, 1)) && terminates(ir, child_at(ir, id, 2));
    if (x->op == HHY_N_BLOCK && x->child_count)
        return terminates(ir, child_at(ir, id, x->child_count - 1));
    return false;
}
static void delete_pass(HhyCompilerIR *ir, uint32_t id, bool dce, size_t *changes) {
    HhyCompilerNode *x = &ir->nodes[id];
    for (uint32_t c = x->first_child; c != HHY_COMPILER_NONE; c = ir->nodes[c].next)
        delete_pass(ir, c, dce, changes);
    if (!dce && (x->op == HHY_N_IF || x->op == HHY_N_WHILE)) {
        const HhyCompilerNode *condition = &ir->nodes[x->first_child];
        if (condition->type == HHY_C_BOOL && condition->effects == 0) {
            uint32_t dead = HHY_COMPILER_NONE;
            if (!condition->value) dead = child_at(ir, id, 1);
            else if (x->op == HHY_N_IF && x->child_count == 3) dead = child_at(ir, id, 2);
            if (dead != HHY_COMPILER_NONE) {
                HhyCompilerNode *branch = &ir->nodes[dead];
                size_t removed = live_nodes(ir, dead) - 1;
                /* Preserve the condition/header and all scope layouts. This block
                 * cannot execute; no active scope or cancellation point is removed. */
                branch->op = HHY_N_BLOCK; branch->child_count = 0;
                branch->first_child = HHY_COMPILER_NONE; branch->origin = HHY_COMPILER_NONE;
                *changes += removed;
            }
        }
    }
    if (x->op != HHY_N_BLOCK && x->op != HHY_N_PROGRAM) return;
    uint32_t *link = &x->first_child;
    bool stopped = false;
    while (*link != HHY_COMPILER_NONE) {
        uint32_t c = *link;
        HhyCompilerNode *s = &ir->nodes[c];
        bool dead = !dce && stopped;
        if (dce && s->op == HHY_N_EXPR_STMT && s->next != HHY_COMPILER_NONE &&
            ir->nodes[s->first_child].effects == 0) dead = true;
        if (dead) {
            *changes += live_nodes(ir, c);
            *link = s->next; x->child_count--;
        } else {
            stopped |= terminates(ir, c);
            link = &s->next;
        }
    }
}
HhyCompilerOptions hhy_compiler_options(void) {
    HhyCompilerOptions o = {{true, true, true, true, true, true}};
    const char *disabled = getenv("HHY_COMPILER_DISABLE");
    if (!disabled) return o;
    if (!strcmp(disabled, "all")) {memset(&o, 0, sizeof(o)); return o;}
    for (unsigned p = 0; p < HHY_COMPILER_PASS_COUNT; p++) {
        const char *cursor = disabled;
        while (*cursor) {
            size_t n = strcspn(cursor, ",");
            if (strlen(hhy_compiler_pass_names[p]) == n && !memcmp(cursor, hhy_compiler_pass_names[p], n)) o.enabled[p] = false;
            cursor += n; if (*cursor) cursor++;
        }
    }
    return o;
}
HhyBytecodeResult hhy_compiler_optimize(HhyCompilerIR *ir, HhyCompilerOptions options,
                                        HhyCompilerReport *report) {
    HhyBytecodeResult result = hhy_compiler_verify(ir);
    if (!result.ok || !report) return result.ok ? status(false, 0, "null compiler report") : result;
    memcpy(report->enabled, options.enabled, sizeof(report->enabled));
    report->before_nodes = live_nodes(ir, 0);
    /* Propagation precedes folding; pass identity and switches remain independent. */
    const unsigned order[] = {2, 1, 0, 3, 4, 5};
    for (unsigned k = 0; k < HHY_COMPILER_PASS_COUNT; k++) {
        unsigned p = order[k]; uint64_t start = now_ns();
        if (options.enabled[p]) {
            if (p == 0 || p == 3) fold_pass(ir, 0, p == 3, &report->changed[p]);
            else if (p == 1 || p == 2) propagate_region(ir, 0, p == 2, &report->changed[p]);
            else delete_pass(ir, 0, p == 5, &report->changed[p]);
            metadata(ir); cfg(ir, ir->blocks);
            result = hhy_compiler_verify(ir);
        }
        report->pass_ns[p] = now_ns() - start;
        if (!result.ok) return result;
    }
    report->after_nodes = live_nodes(ir, 0);
    report->ir_bytes = ir->capacity * (sizeof(*ir->nodes) + 2 * sizeof(*ir->blocks));
    for (size_t i = 0; i < ir->count; i++) report->ir_bytes += ir->nodes[i].token.length + 1;
    for (size_t i = 0; i < 2 * ir->count; i++) report->blocks += ir->blocks[i].present;
    for (size_t i = 0; i < ir->count; i++) {
        unsigned op = ir->nodes[i].op;
        report->calls += op == HHY_N_CALL;
        report->aggregates += op == HHY_N_LIST || op == HHY_N_MAP;
        report->closures += op == HHY_N_CLOSURE;
        if (op == HHY_N_CLOSURE) report->captured_closures++;
        if (op == HHY_N_LIST || op == HHY_N_MAP) {
            uint32_t parent = ir->nodes[i].parent;
            bool local = parent < ir->count && ir->nodes[parent].op == HHY_N_INDEX &&
                ir->nodes[parent].first_child == i;
            if (local) report->local_aggregates++;
            else report->escaping_aggregates++;
        }
        report->rejected_arithmetic += (op == HHY_N_BINARY || op == HHY_N_UNARY) && ir->nodes[i].effects != 0;
    }
    return result;
}
/* Adapter into the existing verified structural Bytecode emitter. This creates
 * owned backend nodes from IR, never reuses or modifies the frontend AST. */
static HhyNode *backend_new(HhyNodeKind kind, HhyToken source, HhyTokenKind token_kind,
                             const char *text) {
    HhyNode *node = hhy_alloc(sizeof(*node));
    node->kind = kind; node->token = source; node->token.kind = token_kind;
    node->token.length = strlen(text); node->token.start = hhy_strndup(text, node->token.length);
    return node;
}
static void backend_add(HhyNode *p, HhyNode *c) {
    p->children = hhy_realloc(p->children, (p->child_count + 1) * sizeof(*p->children));
    p->children[p->child_count++] = c; p->child_capacity = p->child_count;
}
static HhyNode *backend_integer(HhyToken source, int64_t value) {
    char text[32];
    if (value >= 0) {
        snprintf(text, sizeof(text), "%" PRId64, value);
        return backend_new(HHY_N_LITERAL, source, HHY_T_INT, text);
    }
    if (value != INT64_MIN) {
        HhyNode *node = backend_new(HHY_N_UNARY, source, HHY_T_MINUS, "-");
        backend_add(node, backend_integer(source, -value)); return node;
    }
    HhyNode *node = backend_new(HHY_N_BINARY, source, HHY_T_MINUS, "-");
    backend_add(node, backend_integer(source, -INT64_MAX));
    backend_add(node, backend_integer(source, 1)); return node;
}
static HhyNode *backend_node(const HhyCompilerIR *ir, uint32_t id) {
    const HhyCompilerNode *x = &ir->nodes[id];
    if (x->op == HHY_COMPILER_CONST) {
        if (x->type == HHY_C_INT) return backend_integer(x->token, x->value);
        if (x->type == HHY_C_NULL) return backend_new(HHY_N_LITERAL, x->token, HHY_T_NULL, "null");
        return backend_new(HHY_N_LITERAL, x->token, x->value ? HHY_T_TRUE : HHY_T_FALSE, x->value ? "true" : "false");
    }
    HhyNode *node = hhy_alloc(sizeof(*node));
    node->kind = (HhyNodeKind)x->op; node->token = x->token;
    node->token.start = hhy_strndup(x->token.start, x->token.length);
    node->frame_slot_count = x->frame_slots; node->local_env_depth = x->local_depth;
    node->local_binding_slot = x->local_slot; node->local_slot_resolved = x->resolved;
    for (uint32_t c = x->first_child; c != HHY_COMPILER_NONE; c = ir->nodes[c].next)
        backend_add(node, backend_node(ir, c));
    return node;
}
static void backend_free(HhyNode *node) {
    for (size_t i = 0; i < node->child_count; i++) backend_free(node->children[i]);
    free((void *)node->token.start); free(node->children); free(node);
}
HhyBytecodeResult hhy_compiler_emit(const HhyCompilerIR *ir, HhyBytecodeChunk *chunk) {
    HhyBytecodeResult result = hhy_compiler_verify(ir);
    if (!result.ok) return result;
    HhyNode *program = backend_node(ir, 0);
    result = hhy_bytecode_compile_direct(program, chunk);
    backend_free(program); return result;
}
static size_t bytecode_bytes(const HhyBytecodeChunk *chunk) {
    size_t bytes = chunk->count * sizeof(*chunk->code) +
        chunk->constant_count * sizeof(*chunk->constants) +
        chunk->stream_kernel_count * sizeof(*chunk->stream_kernels) +
        chunk->call_plan_count * sizeof(*chunk->call_plans) +
        chunk->exception_region_count * sizeof(*chunk->exception_regions);
    for (size_t i = 0; i < chunk->constant_count; i++) bytes += strlen(chunk->constants[i]) + 1;
    return bytes;
}
HhyBytecodeResult hhy_compiler_compile(const HhyNode *program, HhyBytecodeChunk *chunk,
                                        HhyCompilerOptions options, HhyCompilerReport *report) {
    if (!report || !chunk) return status(false, 0, "null compiler output");
    memset(report, 0, sizeof(*report));
    HhyCompilerIR ir = {0}; uint64_t start = now_ns();
    HhyBytecodeResult result = hhy_compiler_lower(program, &ir);
    report->lower_ns = now_ns() - start;
    if (result.ok) result = hhy_compiler_optimize(&ir, options, report);
    start = now_ns();
    if (result.ok) result = hhy_compiler_emit(&ir, chunk);
    if (result.ok) {
        HhyBytecodeChunk baseline = {0};
        result = hhy_bytecode_compile_direct(program, &baseline);
        if (result.ok) {
            report->baseline_bytecode_bytes = bytecode_bytes(&baseline);
            report->optimized_bytecode_bytes = bytecode_bytes(chunk);
            if (report->optimized_bytecode_bytes > report->baseline_bytecode_bytes ||
                chunk->count > baseline.count) {
                report->size_fallback = true;
                hhy_bytecode_chunk_free(chunk); *chunk = baseline;
                memset(&baseline, 0, sizeof(baseline));
            }
        }
        hhy_bytecode_chunk_free(&baseline);
    }
    report->emit_ns = now_ns() - start;
    hhy_compiler_ir_free(&ir); return result;
}
void hhy_compiler_dump(const HhyCompilerIR *ir, FILE *file) {
    if (!hhy_compiler_verify(ir).ok) {fputs("invalid compiler IR\n", file); return;}
    fprintf(file, "structured-ir v%u nodes=%zu\n", ir->version, ir->count);
    for (size_t i = 0; i < ir->count; i++) {
        const HhyCompilerNode *x = &ir->nodes[i];
        fprintf(file, "n%zu op=%s parent=%u child=%u next=%u count=%u type=%u value=%" PRId64 " effects=%u origin=%u source=%u:%u+%zu\n",
            i, x->op == HHY_COMPILER_CONST ? "CONST" : hhy_opcode_name((HhyOpcode)x->op), x->parent, x->first_child, x->next, x->child_count,
            (unsigned)x->type, x->value, x->effects, x->origin, x->token.line, x->token.column, x->token.length);
    }
    for (size_t i = 0; i < 2 * ir->count; i++) {
        const HhyCompilerBlock *b = &ir->blocks[i];
        if (b->present) fprintf(file, "b%zu node=%u owner=%u phase=%u next=%u alternate=%u exception=%u\n",
            i, b->node, b->owner, b->phase, b->next, b->alternate, b->exception);
    }
}
void hhy_compiler_dump_json(const HhyCompilerIR *ir, FILE *file) {
    if (!hhy_compiler_verify(ir).ok) {fputs("null", file); return;}
    unsigned char *seen = hhy_alloc(ir->count);
    (void)verify_tree(ir, 0, HHY_COMPILER_NONE, 0, seen);
    fprintf(file, "{\"version\":%u,\"instructions\":[", ir->version);
    for (size_t i = 0; i < ir->count; i++) {
        const HhyCompilerNode *x = &ir->nodes[i];
        fprintf(file, "%s{\"id\":%zu,\"active\":%s,\"op\":%u,\"parent\":%u,\"first_child\":%u,\"next\":%u,\"children\":%u,\"type\":%u,\"value\":%" PRId64 ",\"effects\":%u,\"origin\":%u,\"line\":%u,\"column\":%u,\"source_length\":%zu}",
            i ? "," : "", i, seen[i] ? "true" : "false", x->op, x->parent,
            x->first_child, x->next, x->child_count, (unsigned)x->type, x->value,
            x->effects, x->origin, x->token.line, x->token.column, x->token.length);
    }
    fputs("],\"blocks\":[", file); bool first = true;
    for (size_t i = 0; i < 2 * ir->count; i++) {
        const HhyCompilerBlock *b = &ir->blocks[i];
        if (!b->present) continue;
        fprintf(file, "%s{\"id\":%zu,\"node\":%u,\"owner\":%u,\"phase\":%u,\"next\":%u,\"alternate\":%u,\"exception\":%u}",
            first ? "" : ",", i, b->node, b->owner, b->phase, b->next, b->alternate, b->exception);
        first = false;
    }
    fputs("]}", file); free(seen);
}
void hhy_compiler_report(const HhyCompilerReport *r, FILE *file) {
    fprintf(file, "{\"schema_version\":1,\"compiler\":\"structured-ir\",\"before_nodes\":%zu,\"after_nodes\":%zu,\"ir_bytes\":%zu,\"blocks\":%zu,\"lower_verify_ns\":%" PRIu64 ",\"emit_verify_ns\":%" PRIu64 ",\"baseline_bytecode_bytes\":%zu,\"optimized_bytecode_bytes\":%zu,\"size_fallback\":%s,\"passes\":{",
        r->before_nodes, r->after_nodes, r->ir_bytes, r->blocks, r->lower_ns, r->emit_ns,
        r->baseline_bytecode_bytes, r->optimized_bytecode_bytes, r->size_fallback ? "true" : "false");
    for (unsigned i = 0; i < HHY_COMPILER_PASS_COUNT; i++)
        fprintf(file, "%s\"%s\":{\"enabled\":%s,\"changed\":%zu,\"verify_inclusive_ns\":%" PRIu64 "}",
            i ? "," : "", hhy_compiler_pass_names[i], r->enabled[i] ? "true" : "false", r->changed[i], r->pass_ns[i]);
    fprintf(file, "},\"analysis\":{\"calls\":%zu,\"aggregates\":%zu,\"closures\":%zu,\"rejected_arithmetic\":%zu,\"local_aggregate_candidates\":%zu,\"escaping_or_unknown_aggregates\":%zu,\"captured_closures\":%zu,\"specialization\":\"generic_no_cross_platform_feedback_admission\",\"escape\":\"retain_gc_roots_and_allocations\"}}\n",
        r->calls, r->aggregates, r->closures, r->rejected_arithmetic,
        r->local_aggregates, r->escaping_aggregates, r->captured_closures);
}

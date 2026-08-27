#include "hhy/ast.h"

#include <stdlib.h>
#include <string.h>

typedef struct ResolveBinding ResolveBinding;
typedef struct ResolveScope ResolveScope;

struct ResolveBinding {
    HhyToken name;
    size_t slot;
    ResolveBinding *next;
};

struct ResolveScope {
    ResolveScope *parent;
    ResolveBinding *bindings;
    size_t slot_count;
    size_t function_id;
    bool dynamic_layout;
};

typedef struct {
    ResolveScope *scope;
    size_t next_function_id;
} Resolver;

static bool token_equal(HhyToken left, HhyToken right) {
    return left.length == right.length && memcmp(left.start, right.start, left.length) == 0;
}

static bool scope_has_dynamic_declaration(const HhyNode *node) {
    if (node == NULL) return false;
    for (size_t i = 0; i < node->child_count; i++) {
        HhyNodeKind kind = node->children[i]->kind;
        if (kind == HHY_N_IMPORT_DECL) return true;
        if (kind == HHY_N_EXPORT_DECL && node->children[i]->child_count > 0 &&
            node->children[i]->children[0]->kind == HHY_N_IMPORT_DECL) return true;
    }
    return false;
}

static size_t define_binding(ResolveScope *scope, HhyToken name) {
    ResolveBinding *binding = hhy_alloc(sizeof(*binding));
    binding->name = name;
    binding->slot = scope->slot_count++;
    binding->next = scope->bindings;
    scope->bindings = binding;
    return binding->slot;
}

static void free_bindings(ResolveBinding *binding) {
    while (binding != NULL) {
        ResolveBinding *next = binding->next;
        free(binding);
        binding = next;
    }
}

static void resolve_node(Resolver *resolver, HhyNode *node);

static void resolve_identifier(Resolver *resolver, HhyNode *node) {
    size_t depth = 0;
    size_t function_id = resolver->scope == NULL ? 0 : resolver->scope->function_id;
    for (ResolveScope *scope = resolver->scope; scope != NULL; scope = scope->parent, depth++) {
        for (ResolveBinding *binding = scope->bindings; binding != NULL; binding = binding->next) {
            if (!token_equal(binding->name, node->token)) continue;
            if (scope->function_id == function_id && !scope->dynamic_layout) {
                node->local_env_depth = depth;
                node->local_binding_slot = binding->slot;
                node->local_slot_resolved = true;
            }
            return;
        }
    }
}

static void resolve_in_scope(Resolver *resolver, HhyNode *owner, HhyNode *body,
                             size_t function_id, bool body_contents) {
    ResolveScope scope = {
        .parent = resolver->scope,
        .function_id = function_id,
        .dynamic_layout = scope_has_dynamic_declaration(body)
    };
    ResolveScope *previous = resolver->scope;
    resolver->scope = &scope;
    if (body_contents) {
        for (size_t i = 0; i < body->child_count; i++) resolve_node(resolver, body->children[i]);
    } else resolve_node(resolver, body);
    owner->frame_slot_count = scope.slot_count;
    resolver->scope = previous;
    free_bindings(scope.bindings);
}

static void resolve_function(Resolver *resolver, HhyNode *node) {
    size_t function_id = ++resolver->next_function_id;
    ResolveScope scope = {.parent = resolver->scope, .function_id = function_id};
    ResolveScope *previous = resolver->scope;
    resolver->scope = &scope;
    size_t body_index = node->child_count - 1;
    for (size_t i = 1; i < body_index; i++) define_binding(&scope, node->children[i]->token);
    HhyNode *body = node->children[body_index];
    scope.dynamic_layout = body->kind == HHY_N_BLOCK && scope_has_dynamic_declaration(body);
    if (body->kind == HHY_N_BLOCK) {
        for (size_t i = 0; i < body->child_count; i++) resolve_node(resolver, body->children[i]);
    } else resolve_node(resolver, body);
    node->frame_slot_count = scope.slot_count;
    resolver->scope = previous;
    free_bindings(scope.bindings);
}

static void resolve_closure(Resolver *resolver, HhyNode *node) {
    size_t function_id = ++resolver->next_function_id;
    ResolveScope scope = {.parent = resolver->scope, .function_id = function_id};
    ResolveScope *previous = resolver->scope;
    resolver->scope = &scope;
    size_t start = 0;
    if (node->child_count > 0 && node->children[0]->kind == HHY_N_IDENTIFIER) {
        define_binding(&scope, node->children[0]->token);
        start = 1;
    } else {
        HhyToken implicit = node->token;
        implicit.start = "it";
        implicit.length = 2;
        define_binding(&scope, implicit);
    }
    for (size_t i = start; i < node->child_count; i++) resolve_node(resolver, node->children[i]);
    node->frame_slot_count = scope.slot_count;
    resolver->scope = previous;
    free_bindings(scope.bindings);
}

static void resolve_node(Resolver *resolver, HhyNode *node) {
    switch (node->kind) {
        case HHY_N_PROGRAM:
            for (size_t i = 0; i < node->child_count; i++) resolve_node(resolver, node->children[i]);
            break;
        case HHY_N_BLOCK:
            resolve_in_scope(resolver, node, node, resolver->scope->function_id, true);
            break;
        case HHY_N_LET_DECL: {
            size_t name = node->child_count == 3 ? 1 : 0;
            resolve_node(resolver, node->children[name + 1]);
            define_binding(resolver->scope, node->children[name]->token);
            break;
        }
        case HHY_N_FN_DECL:
            define_binding(resolver->scope, node->children[0]->token);
            resolve_function(resolver, node);
            break;
        case HHY_N_CLOSURE: resolve_closure(resolver, node); break;
        case HHY_N_FOR: {
            resolve_node(resolver, node->children[1]);
            ResolveScope scope = {.parent = resolver->scope,
                                  .function_id = resolver->scope->function_id};
            ResolveScope *previous = resolver->scope;
            resolver->scope = &scope;
            define_binding(&scope, node->children[0]->token);
            resolve_node(resolver, node->children[2]);
            resolver->scope = previous;
            free_bindings(scope.bindings);
            break;
        }
        case HHY_N_TRY: {
            resolve_node(resolver, node->children[0]);
            ResolveScope scope = {.parent = resolver->scope,
                                  .function_id = resolver->scope->function_id};
            ResolveScope *previous = resolver->scope;
            resolver->scope = &scope;
            define_binding(&scope, node->children[1]->token);
            resolve_node(resolver, node->children[2]);
            resolver->scope = previous;
            free_bindings(scope.bindings);
            break;
        }
        case HHY_N_ASSIGN:
            resolve_identifier(resolver, node->children[0]);
            resolve_node(resolver, node->children[1]);
            break;
        case HHY_N_IDENTIFIER: resolve_identifier(resolver, node); break;
        case HHY_N_IMPORT_DECL: case HHY_N_LITERAL: case HHY_N_BREAK: case HHY_N_CONTINUE:
            break;
        default:
            for (size_t i = 0; i < node->child_count; i++) resolve_node(resolver, node->children[i]);
            break;
    }
}

void hhy_resolve_slots(HhyNode *program) {
    Resolver resolver = {0};
    ResolveScope global = {.function_id = 0, .dynamic_layout = true};
    resolver.scope = &global;
    resolve_node(&resolver, program);
    free_bindings(global.bindings);
}

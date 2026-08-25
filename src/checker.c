#define _XOPEN_SOURCE 700
#include "hhy/checker.h"
#include "hhy/contracts.h"
#include "hhy/parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct CheckBinding CheckBinding;
typedef struct CheckScope CheckScope;
typedef struct CheckModule CheckModule;
struct CheckBinding {
    HhyToken token;
    bool mutable;
    bool function;
    bool sendable;
    size_t arity;
    CheckBinding *next;
};
struct CheckScope { CheckScope *parent; CheckBinding *bindings; };
struct CheckModule { const char *path; CheckModule *parent; };

typedef struct {
    const HhySource *source;
    CheckScope *scope;
    CheckScope *parallel_boundary;
    size_t errors;
    size_t warnings;
    unsigned function_depth;
    unsigned loop_depth;
    CheckModule *module_stack;
} Checker;

static bool token_is(HhyToken token, const char *text) {
    return token.length == strlen(text) && memcmp(token.start, text, token.length) == 0;
}

static void diagnostic(Checker *checker, HhyToken token, const char *level, const char *message) {
    fprintf(stderr, "%s:%u:%u: %s: %s", checker->source->path, token.line, token.column,
            level, message);
    if (token.length) fprintf(stderr, " `%.*s`", (int)token.length, token.start);
    fputc('\n', stderr);
}

static void error(Checker *checker, HhyToken token, const char *message) {
    checker->errors++;
    diagnostic(checker, token, "check error", message);
}

static void warning(Checker *checker, HhyToken token, const char *message) {
    checker->warnings++;
    diagnostic(checker, token, "warning", message);
}

static CheckBinding *local(CheckScope *scope, HhyToken token) {
    for (CheckBinding *binding = scope->bindings; binding; binding = binding->next)
        if (binding->token.length == token.length &&
            memcmp(binding->token.start, token.start, token.length) == 0) return binding;
    return NULL;
}

static CheckBinding *find(CheckScope *scope, HhyToken token, CheckScope **owner) {
    for (CheckScope *current = scope; current; current = current->parent) {
        CheckBinding *binding = local(current, token);
        if (binding) { if (owner) *owner = current; return binding; }
    }
    return NULL;
}

static void define(Checker *checker, HhyToken token, bool mutable) {
    if (local(checker->scope, token)) {
        error(checker, token, "duplicate binding in the same scope");
        return;
    }
    if (checker->scope->parent && find(checker->scope->parent, token, NULL))
        warning(checker, token, "binding shadows an outer name");
    CheckBinding *binding = calloc(1, sizeof(*binding));
    if (!binding) { fputs("hhy: out of memory\n", stderr); exit(70); }
    binding->token = token;
    binding->mutable = mutable;
    binding->sendable = true;
    binding->next = checker->scope->bindings;
    checker->scope->bindings = binding;
}

static void define_function(Checker *checker, HhyToken token, size_t arity) {
    define(checker, token, false);
    CheckBinding *binding = local(checker->scope, token);
    if (binding) { binding->function = true; binding->arity = arity; }
}

static void free_bindings(CheckBinding *binding) {
    while (binding) { CheckBinding *next = binding->next; free(binding); binding = next; }
}

static void check_node(Checker *checker, const HhyNode *node);

static bool module_exports(const HhyNode *program, HhyToken requested) {
    for (size_t i = 0; i < program->child_count; i++) {
        const HhyNode *export_node = program->children[i];
        if (export_node->kind != HHY_N_EXPORT_DECL || export_node->child_count != 1) continue;
        const HhyNode *declaration = export_node->children[0];
        if ((declaration->kind != HHY_N_LET_DECL && declaration->kind != HHY_N_FN_DECL) ||
            !declaration->child_count) continue;
        size_t name = declaration->kind == HHY_N_LET_DECL && declaration->child_count == 3 ? 1 : 0;
        HhyToken exported = declaration->children[name]->token;
        if (exported.length == requested.length &&
            memcmp(exported.start, requested.start, requested.length) == 0) return true;
    }
    return false;
}

static char *decode_string_token(HhyToken token, size_t *out_length) {
    char *decoded = hhy_alloc(token.length > 1 ? token.length - 1 : 1);
    size_t written = 0;
    for (size_t i = 1; i + 1 < token.length; i++) {
        char c = token.start[i];
        if (c == '\\' && i + 2 < token.length) {
            c = token.start[++i];
            if (c == 'n') c = '\n'; else if (c == 'r') c = '\r'; else if (c == 't') c = '\t';
            else if (c == 'b') c = '\b'; else if (c == 'f') c = '\f'; else if (c == '0') c = '\0';
        }
        decoded[written++] = c;
    }
    decoded[written] = '\0';
    *out_length = written;
    return decoded;
}

static HhyNode *check_module_file(Checker *checker, const HhyNode *site,
                                  HhySource *source, HhyTokenList *tokens, char **resolved_out) {
    HhyToken path_token = {0};
    for (size_t i = 0; i < site->child_count; i++)
        if (site->children[i]->token.kind == HHY_T_STRING) path_token = site->children[i]->token;
    if (!path_token.length || path_token.length < 2) return NULL;
    size_t requested_length = 0;
    char *requested = decode_string_token(path_token, &requested_length);
    if (memchr(requested, '\0', requested_length) != NULL) {
        error(checker, path_token, "module path cannot contain U+0000");
        free(requested);
        return NULL;
    }
    char candidate[PATH_MAX];
    const char *slash = strrchr(checker->source->path, '/');
    size_t directory = slash ? (size_t)(slash - checker->source->path) : 0;
    int written = requested[0] == '/'
        ? snprintf(candidate, sizeof(candidate), "%s", requested)
        : (directory ? snprintf(candidate, sizeof(candidate), "%.*s/%s", (int)directory,
                                checker->source->path, requested)
                     : snprintf(candidate, sizeof(candidate), "%s", requested));
    free(requested);
    if (written < 0 || written >= (int)sizeof(candidate)) {
        error(checker, path_token, "module path is too long"); return NULL;
    }
    char resolved[PATH_MAX];
    if (realpath(candidate, resolved) == NULL) {
        error(checker, path_token, "module file was not found"); return NULL;
    }
    for (CheckModule *module = checker->module_stack; module; module = module->parent) {
        if (strcmp(module->path, resolved) == 0) {
            error(checker, path_token, "circular module import"); return NULL;
        }
    }
    *resolved_out = hhy_strndup(resolved, strlen(resolved));
    if (!hhy_source_load(*resolved_out, source)) {
        error(checker, path_token, "cannot read module"); return NULL;
    }
    source->path = *resolved_out;
    if (!hhy_lex(source, tokens)) return NULL;
    HhyNode *program = NULL;
    HhyParseResult parsed = hhy_parse(source, tokens, &program);
    if (!parsed.ok) return program;
    CheckModule frame = {.path = *resolved_out, .parent = checker->module_stack};
    CheckModule *previous_stack = checker->module_stack;
    const HhySource *previous_source = checker->source;
    CheckScope module_scope = {0};
    CheckScope *previous_scope = checker->scope;
    checker->module_stack = &frame;
    checker->source = source;
    checker->scope = &module_scope;
    check_node(checker, program);
    checker->scope = previous_scope;
    checker->source = previous_source;
    checker->module_stack = previous_stack;
    free_bindings(module_scope.bindings);
    return program;
}

static void check_scope_node(Checker *checker, const HhyNode *node) {
    CheckScope nested = {.parent = checker->scope};
    CheckScope *previous = checker->scope;
    checker->scope = &nested;
    for (size_t i = 0; i < node->child_count; i++) check_node(checker, node->children[i]);
    checker->scope = previous;
    free_bindings(nested.bindings);
}

static void check_import(Checker *checker, const HhyNode *node) {
    if (!node->child_count) return;
    HhySource module_source = {0}; HhyTokenList module_tokens = {0}; char *resolved = NULL;
    HhyNode *module_program = check_module_file(checker, node, &module_source, &module_tokens, &resolved);
    if (node->children[0]->token.kind == HHY_T_STRING) {
        if (node->child_count >= 3 && node->children[1]->token.kind == HHY_T_AS)
            define(checker, node->children[2]->token, false);
        goto cleanup;
    }
    size_t close = 0;
    for (size_t i = 0; i < node->child_count; i++)
        if (node->children[i]->token.kind == HHY_T_RBRACE) { close = i; break; }
    if (!close) {
        HhyToken module_name = node->children[0]->token;
        bool installed = node->child_count == 1 &&
            hhy_contract_namespace_installed(module_name.start, module_name.length);
        if (!installed)
            error(checker, node->children[0]->token,
                  "ModuleNotFoundError(HHY_MODULE_NOT_FOUND): standard module is not installed");
        goto cleanup;
    }
    for (size_t i = 1; i < close; i++) {
        if (node->children[i]->token.kind != HHY_T_IDENTIFIER) continue;
        HhyToken local_name = node->children[i]->token;
        HhyToken source_name = local_name;
        if (i + 2 < close && node->children[i + 1]->token.kind == HHY_T_AS) {
            local_name = node->children[i + 2]->token;
            i += 2;
        }
        if (module_program && !module_exports(module_program, source_name))
            error(checker, source_name, "module does not export requested name");
        define(checker, local_name, false);
    }
cleanup:
    hhy_node_free(module_program);
    hhy_tokens_free(&module_tokens);
    hhy_source_free(&module_source);
    free(resolved);
}

static bool scope_at_or_inside(CheckScope *scope, CheckScope *boundary) {
    for (CheckScope *current = scope; current; current = current->parent)
        if (current == boundary) return true;
    return false;
}

static void check_identifier(Checker *checker, const HhyNode *node) {
    if (hhy_contract_lookup_n(node->token.start, node->token.length) != NULL ||
        token_is(node->token, "args") || token_is(node->token, "env") ||
        token_is(node->token, "system") || token_is(node->token, "http") ||
        token_is(node->token, "datetime")) return;
    CheckScope *owner = NULL;
    CheckBinding *binding = find(checker->scope, node->token, &owner);
    if (!binding) {
        error(checker, node->token, "use of undefined name");
        return;
    }
    if (checker->parallel_boundary &&
        (owner == checker->parallel_boundary ||
         !scope_at_or_inside(owner, checker->parallel_boundary))) {
        if (binding->mutable)
            error(checker, node->token, "parallel closure cannot capture mutable binding");
        else if (!binding->sendable)
            error(checker, node->token, "parallel closure cannot capture non-sendable binding");
    }
}

static bool parallel_call(const HhyNode *node) {
    return node->kind == HHY_N_CALL && node->child_count >= 2 &&
           node->children[0]->kind == HHY_N_IDENTIFIER &&
           token_is(node->children[0]->token, "parallel");
}

static const HhyCallableContract *call_contract(const HhyNode *callee) {
    if (callee->kind == HHY_N_IDENTIFIER)
        return hhy_contract_lookup_n(callee->token.start, callee->token.length);
    if (callee->kind != HHY_N_MEMBER || callee->child_count != 1 ||
        callee->children[0]->kind != HHY_N_IDENTIFIER) return NULL;
    HhyToken owner = callee->children[0]->token;
    size_t length = owner.length + 1 + callee->token.length;
    if (length >= 128) return NULL;
    char qualified[128];
    memcpy(qualified, owner.start, owner.length);
    qualified[owner.length] = '.';
    memcpy(qualified + owner.length + 1, callee->token.start, callee->token.length);
    return hhy_contract_lookup_n(qualified, length);
}

static void check_arity(Checker *checker, const HhyNode *call, bool injected) {
    const HhyNode *callee = call->children[0];
    size_t actual = call->child_count - 1 + (injected ? 1 : 0);
    size_t minimum = 0, maximum = SIZE_MAX;
    CheckBinding *binding = callee->kind == HHY_N_IDENTIFIER
        ? find(checker->scope, callee->token, NULL) : NULL;
    if (binding && binding->function) minimum = maximum = binding->arity;
    else {
        const HhyCallableContract *contract = call_contract(callee);
        if (contract == NULL) return;
        minimum = contract->minimum_arity;
        maximum = contract->maximum_arity;
    }
    if (actual < minimum || actual > maximum) {
        char message[128];
        if (minimum == maximum)
            snprintf(message, sizeof(message), "call expects %zu argument%s but received %zu",
                     minimum, minimum == 1 ? "" : "s", actual);
        else
            snprintf(message, sizeof(message), "call expects between %zu and %zu arguments but received %zu",
                     minimum, maximum, actual);
        error(checker, call->token, message);
    }
}

static void check_call(Checker *checker, const HhyNode *node, bool injected) {
    bool parallel = parallel_call(node);
    check_node(checker, node->children[0]);
    for (size_t i = 1; i < node->child_count; i++) {
        CheckScope *previous_boundary = checker->parallel_boundary;
        if (parallel && node->children[i]->kind == HHY_N_CLOSURE)
            checker->parallel_boundary = checker->scope;
        check_node(checker, node->children[i]);
        checker->parallel_boundary = previous_boundary;
    }
    check_arity(checker, node, injected);
    if (node->children[0]->kind == HHY_N_IDENTIFIER && token_is(node->children[0]->token, "shell"))
        warning(checker, node->children[0]->token,
                "shell executes through /bin/sh; prefer run(List<String>)");
}

static bool stream_producer(HhyToken token) {
    static const char *names[] = {
        "stream", "map", "where", "take", "skip", "flat_map", "inspect", "distinct",
        "sort_by", "group_by", "parse_csv", "encode_csv", "on_error", "parallel",
        "debounce", "every", "watch", "read_lines", "files", "processes", "stdin_lines",
        "stdout_lines"
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (token_is(token, names[i])) return true;
    return false;
}

static bool expression_non_sendable(Checker *checker, const HhyNode *node) {
    if (node->kind == HHY_N_IDENTIFIER) {
        CheckBinding *binding = find(checker->scope, node->token, NULL);
        return binding && !binding->sendable;
    }
    if (node->kind == HHY_N_CALL && node->child_count &&
        node->children[0]->kind == HHY_N_IDENTIFIER)
        return stream_producer(node->children[0]->token);
    if (node->kind == HHY_N_PIPE && node->child_count == 2) {
        const HhyNode *stage = node->children[1];
        const HhyNode *callee = stage->kind == HHY_N_CALL && stage->child_count
            ? stage->children[0] : stage;
        if (callee->kind == HHY_N_IDENTIFIER) return stream_producer(callee->token);
    }
    return false;
}

static bool map_key_next(HhyToken token, size_t *index, unsigned char *out) {
    size_t end = token.kind == HHY_T_STRING && token.length >= 2 ? token.length - 1 : token.length;
    if (*index >= end) return false;
    unsigned char c = (unsigned char)token.start[(*index)++];
    if (token.kind == HHY_T_STRING && c == '\\' && *index < end) {
        c = (unsigned char)token.start[(*index)++];
        if (c == 'n') c = '\n'; else if (c == 'r') c = '\r'; else if (c == 't') c = '\t';
        else if (c == 'b') c = '\b'; else if (c == 'f') c = '\f'; else if (c == '0') c = '\0';
    }
    *out = c;
    return true;
}

static bool map_keys_equal(HhyToken left, HhyToken right) {
    size_t li = left.kind == HHY_T_STRING ? 1 : 0;
    size_t ri = right.kind == HHY_T_STRING ? 1 : 0;
    for (;;) {
        unsigned char lc = 0, rc = 0;
        bool has_left = map_key_next(left, &li, &lc);
        bool has_right = map_key_next(right, &ri, &rc);
        if (!has_left || !has_right) return has_left == has_right;
        if (lc != rc) return false;
    }
}

static void check_node(Checker *checker, const HhyNode *node) {
    switch (node->kind) {
        case HHY_N_PROGRAM:
            for (size_t i = 0; i < node->child_count; i++) check_node(checker, node->children[i]);
            break;
        case HHY_N_BLOCK: check_scope_node(checker, node); break;
        case HHY_N_LET_DECL: {
            bool mutable = node->child_count == 3;
            size_t name = mutable ? 1 : 0;
            check_node(checker, node->children[name + 1]);
            define(checker, node->children[name]->token, mutable);
            CheckBinding *binding = local(checker->scope, node->children[name]->token);
            if (binding) binding->sendable = !expression_non_sendable(checker, node->children[name + 1]);
            break;
        }
        case HHY_N_FN_DECL: {
            define_function(checker, node->children[0]->token, node->child_count - 2);
            CheckScope nested = {.parent = checker->scope};
            CheckScope *previous = checker->scope; checker->scope = &nested;
            size_t body = node->child_count - 1;
            for (size_t i = 1; i < body; i++) define(checker, node->children[i]->token, false);
            checker->function_depth++;
            check_node(checker, node->children[body]);
            checker->function_depth--;
            checker->scope = previous; free_bindings(nested.bindings);
            break;
        }
        case HHY_N_IMPORT_DECL: check_import(checker, node); break;
        case HHY_N_EXPORT_DECL:
            if (node->child_count) check_node(checker, node->children[0]);
            break;
        case HHY_N_IF:
            for (size_t i = 0; i < node->child_count; i++) check_node(checker, node->children[i]);
            break;
        case HHY_N_FOR: {
            check_node(checker, node->children[1]);
            CheckScope nested = {.parent = checker->scope};
            CheckScope *previous = checker->scope; checker->scope = &nested;
            define(checker, node->children[0]->token, false); checker->loop_depth++;
            check_node(checker, node->children[2]); checker->loop_depth--;
            checker->scope = previous; free_bindings(nested.bindings);
            break;
        }
        case HHY_N_WHILE:
            check_node(checker, node->children[0]); checker->loop_depth++;
            check_node(checker, node->children[1]); checker->loop_depth--;
            break;
        case HHY_N_TRY: {
            check_node(checker, node->children[0]);
            CheckScope nested = {.parent = checker->scope};
            CheckScope *previous = checker->scope; checker->scope = &nested;
            define(checker, node->children[1]->token, false);
            check_node(checker, node->children[2]);
            checker->scope = previous; free_bindings(nested.bindings);
            break;
        }
        case HHY_N_RETURN:
            if (!checker->function_depth) error(checker, node->token, "return outside function or closure");
            for (size_t i = 0; i < node->child_count; i++) check_node(checker, node->children[i]);
            break;
        case HHY_N_BREAK: case HHY_N_CONTINUE:
            if (!checker->loop_depth) error(checker, node->token, "loop control outside loop");
            break;
        case HHY_N_ASSIGN: {
            const HhyNode *target = node->children[0];
            if (target->kind != HHY_N_IDENTIFIER) error(checker, target->token, "assignment target must be a variable");
            else {
                CheckBinding *binding = find(checker->scope, target->token, NULL);
                if (!binding) error(checker, target->token, "assignment to undefined name");
                else if (!binding->mutable) error(checker, target->token, "assignment to immutable binding");
            }
            check_node(checker, node->children[1]);
            break;
        }
        case HHY_N_MEMBER:
            check_node(checker, node->children[0]);
            break;
        case HHY_N_MAP:
            for (size_t i = 0; i < node->child_count; i++) {
                for (size_t previous = 0; previous < i; previous++)
                    if (map_keys_equal(node->children[previous]->token, node->children[i]->token)) {
                        error(checker, node->children[i]->token, "duplicate Map key");
                        break;
                    }
                check_node(checker, node->children[i]);
            }
            break;
        case HHY_N_MAP_ENTRY:
            if (node->child_count) check_node(checker, node->children[0]);
            break;
        case HHY_N_CLOSURE: {
            CheckScope nested = {.parent = checker->scope};
            CheckScope *previous = checker->scope; checker->scope = &nested;
            size_t start = 0;
            if (node->child_count && node->children[0]->kind == HHY_N_IDENTIFIER) {
                define(checker, node->children[0]->token, false); start = 1;
            } else {
                HhyToken implicit = node->token; implicit.start = "it"; implicit.length = 2;
                define(checker, implicit, false);
            }
            checker->function_depth++;
            for (size_t i = start; i < node->child_count; i++) check_node(checker, node->children[i]);
            checker->function_depth--;
            checker->scope = previous; free_bindings(nested.bindings);
            break;
        }
        case HHY_N_CALL: check_call(checker, node, false); break;
        case HHY_N_PIPE:
            check_node(checker, node->children[0]);
            if (node->children[1]->kind == HHY_N_CALL) check_call(checker, node->children[1], true);
            else {
                check_node(checker, node->children[1]);
                if (node->children[1]->kind == HHY_N_IDENTIFIER) {
                    HhyNode synthetic = {.kind = HHY_N_CALL, .token = node->children[1]->token,
                                         .children = (HhyNode **)&node->children[1], .child_count = 1};
                    check_arity(checker, &synthetic, true);
                }
            }
            break;
        case HHY_N_IDENTIFIER: check_identifier(checker, node); break;
        case HHY_N_LITERAL: break;
        default:
            for (size_t i = 0; i < node->child_count; i++) check_node(checker, node->children[i]);
            break;
    }
}

HhyCheckResult hhy_check(const HhySource *source, const HhyNode *program) {
    CheckScope global = {0};
    CheckModule root = {.path = source->path};
    Checker checker = {.source = source, .scope = &global, .module_stack = &root};
    check_node(&checker, program);
    free_bindings(global.bindings);
    HhyCheckResult result = {
        .ok = checker.errors == 0,
        .error_count = checker.errors,
        .warning_count = checker.warnings
    };
    return result;
}

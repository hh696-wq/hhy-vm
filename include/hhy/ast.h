#ifndef HHY_AST_H
#define HHY_AST_H

#include "hhy/token.h"

typedef enum {
    HHY_N_PROGRAM,
    HHY_N_LET_DECL,
    HHY_N_FN_DECL,
    HHY_N_IMPORT_DECL,
    HHY_N_EXPORT_DECL,
    HHY_N_BLOCK,
    HHY_N_IF,
    HHY_N_FOR,
    HHY_N_WHILE,
    HHY_N_TRY,
    HHY_N_RETURN,
    HHY_N_BREAK,
    HHY_N_CONTINUE,
    HHY_N_EXPR_STMT,
    HHY_N_ASSIGN,
    HHY_N_PIPE,
    HHY_N_BINARY,
    HHY_N_UNARY,
    HHY_N_RANGE,
    HHY_N_CALL,
    HHY_N_MEMBER,
    HHY_N_INDEX,
    HHY_N_CLOSURE,
    HHY_N_ATTEMPT,
    HHY_N_LIST,
    HHY_N_MAP,
    HHY_N_MAP_ENTRY,
    HHY_N_IDENTIFIER,
    HHY_N_LITERAL
} HhyNodeKind;

typedef struct HhyNode HhyNode;

struct HhyNode {
    HhyNodeKind kind;
    HhyToken token;
    HhyNode **children;
    size_t child_count;
    size_t child_capacity;
    size_t cached_env_depth;
    size_t cached_binding_slot;
    bool binding_cache_valid;
    size_t local_env_depth;
    size_t local_binding_slot;
    bool local_slot_resolved;
    size_t frame_slot_count;
};

HhyNode *hhy_node_new(HhyNodeKind kind, HhyToken token);
void hhy_node_add(HhyNode *parent, HhyNode *child);
void hhy_node_free(HhyNode *node);
const char *hhy_node_kind_name(HhyNodeKind kind);
void hhy_ast_print(const HhyNode *node);
void hhy_resolve_slots(HhyNode *program);

#endif

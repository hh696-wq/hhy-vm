#include "hhy/ast.h"

#include <stdio.h>
#include <stdlib.h>

HhyNode *hhy_node_new(HhyNodeKind kind, HhyToken token) {
    HhyNode *node = hhy_alloc(sizeof(*node));
    node->kind = kind;
    node->token = token;
    return node;
}

void hhy_node_add(HhyNode *parent, HhyNode *child) {
    if (child == NULL) return;
    if (parent->child_count == parent->child_capacity) {
        size_t capacity = parent->child_capacity < 4 ? 4 : parent->child_capacity * 2;
        parent->children = hhy_realloc(parent->children, capacity * sizeof(HhyNode *));
        parent->child_capacity = capacity;
    }
    parent->children[parent->child_count++] = child;
}

void hhy_node_free(HhyNode *node) {
    if (node == NULL) return;
    for (size_t i = 0; i < node->child_count; i++) hhy_node_free(node->children[i]);
    free(node->children);
    free(node);
}

const char *hhy_node_kind_name(HhyNodeKind kind) {
    static const char *names[] = {
        "Program","LetDecl","FnDecl","ImportDecl","ExportDecl","Block","If","For","While","Try","Return","Break","Continue","ExprStmt",
        "Assign","Pipe","Binary","Unary","Range","Call","Member","Index","Closure","Attempt","List","Map","MapEntry","Identifier","Literal"
    };
    return names[(size_t)kind];
}

static void print_escaped(const HhyToken *token) {
    putchar('"');
    for (size_t i = 0; i < token->length; i++) {
        unsigned char c = (unsigned char)token->start[i];
        if (c == '"' || c == '\\') { putchar('\\'); putchar((char)c); }
        else if (c == '\n') fputs("\\n", stdout);
        else if (c == '\r') fputs("\\r", stdout);
        else if (c == '\t') fputs("\\t", stdout);
        else if (c >= 0x20) putchar((char)c);
    }
    putchar('"');
}

static void print_node(const HhyNode *node, int indent) {
    for (int i = 0; i < indent; i++) fputs("  ", stdout);
    fputs(hhy_node_kind_name(node->kind), stdout);
    if (node->token.length > 0) {
        putchar(' ');
        print_escaped(&node->token);
    }
    printf(" @%u:%u", node->token.line, node->token.column);
    if (node->child_count == 0) {
        putchar('\n');
        return;
    }
    putchar('\n');
    for (size_t i = 0; i < node->child_count; i++) print_node(node->children[i], indent + 1);
}

void hhy_ast_print(const HhyNode *node) { print_node(node, 0); }

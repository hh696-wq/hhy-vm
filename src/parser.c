#include "hhy/parser.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const HhySource *source;
    const HhyTokenList *tokens;
    size_t current;
    size_t errors;
    bool panic;
} Parser;

static const HhyToken *peek(Parser *p) { return &p->tokens->items[p->current]; }
static const HhyToken *previous(Parser *p) { return &p->tokens->items[p->current - 1]; }
static bool is_at_end(Parser *p) { return peek(p)->kind == HHY_T_EOF; }

static const HhyToken *advance_token(Parser *p) {
    if (!is_at_end(p)) p->current++;
    return previous(p);
}

static bool check(Parser *p, HhyTokenKind kind) { return peek(p)->kind == kind; }

static bool match_token(Parser *p, HhyTokenKind kind) {
    if (!check(p, kind)) return false;
    advance_token(p);
    return true;
}

static void show_line(Parser *p, const HhyToken *token) {
    const char *line_start = token->start;
    while (line_start > p->source->text && line_start[-1] != '\n' && line_start[-1] != '\r') line_start--;
    const char *line_end = token->start;
    const char *source_end = p->source->text + p->source->length;
    while (line_end < source_end && *line_end != '\n' && *line_end != '\r') line_end++;
    fprintf(stderr, "  %.*s\n  ", (int)(line_end - line_start), line_start);
    for (uint32_t i = 1; i < token->column; i++) fputc(' ', stderr);
    fputs("^\n", stderr);
}

static void error_at(Parser *p, const HhyToken *token, const char *message) {
    if (p->panic) return;
    p->panic = true;
    p->errors++;
    if (!hhy_suppress_diagnostics) {
        fprintf(stderr, "%s:%u:%u: error: %s; found %s",
                p->source->path, token->line, token->column, message,
                hhy_token_kind_name(token->kind));
        if (token->length > 0) fprintf(stderr, " `%.*s`", (int)token->length, token->start);
        fputc('\n', stderr);
        show_line(p, token);
    }
}

static HhyToken synthetic_token(Parser *p) {
    HhyToken token = *peek(p);
    token.length = 0;
    return token;
}

static const HhyToken *consume(Parser *p, HhyTokenKind kind, const char *message) {
    if (check(p, kind)) return advance_token(p);
    error_at(p, peek(p), message);
    return peek(p);
}

static void skip_newlines(Parser *p) {
    while (match_token(p, HHY_T_NEWLINE)) {}
}

static bool at_terminator(Parser *p) {
    return check(p, HHY_T_NEWLINE) || check(p, HHY_T_SEMICOLON) ||
           check(p, HHY_T_RBRACE) || check(p, HHY_T_EOF);
}

static void consume_terminator(Parser *p) {
    if (match_token(p, HHY_T_SEMICOLON)) {
        skip_newlines(p);
        return;
    }
    if (match_token(p, HHY_T_NEWLINE)) {
        skip_newlines(p);
        return;
    }
    if (!check(p, HHY_T_RBRACE) && !check(p, HHY_T_EOF)) error_at(p, peek(p), "expected end of statement");
}

static HhyNode *parse_declaration(Parser *p);
static HhyNode *parse_statement(Parser *p);
static HhyNode *parse_expression(Parser *p);

static void synchronize(Parser *p) {
    p->panic = false;
    while (!is_at_end(p)) {
        if (p->current > 0 && (previous(p)->kind == HHY_T_NEWLINE || previous(p)->kind == HHY_T_SEMICOLON)) return;
        switch (peek(p)->kind) {
            case HHY_T_LET: case HHY_T_FN: case HHY_T_IMPORT: case HHY_T_EXPORT:
            case HHY_T_IF: case HHY_T_FOR: case HHY_T_WHILE: case HHY_T_TRY:
            case HHY_T_RETURN: return;
            default: advance_token(p); break;
        }
    }
}

static HhyNode *parse_block_after_open(Parser *p, HhyToken open) {
    HhyNode *block = hhy_node_new(HHY_N_BLOCK, open);
    skip_newlines(p);
    while (!check(p, HHY_T_RBRACE) && !is_at_end(p)) {
        HhyNode *child = parse_declaration(p);
        hhy_node_add(block, child);
        if (p->panic) synchronize(p);
        skip_newlines(p);
    }
    consume(p, HHY_T_RBRACE, "expected `}` after block");
    return block;
}

static HhyNode *parse_block(Parser *p) {
    const HhyToken *open = consume(p, HHY_T_LBRACE, "expected `{`");
    return parse_block_after_open(p, *open);
}

static HhyNode *parse_map(Parser *p, HhyToken open) {
    HhyNode *map = hhy_node_new(HHY_N_MAP, open);
    skip_newlines(p);
    while (!check(p, HHY_T_RBRACE) && !is_at_end(p)) {
        if (!check(p, HHY_T_IDENTIFIER) && !check(p, HHY_T_STRING)) {
            error_at(p, peek(p), "expected map key");
            break;
        }
        HhyToken key = *advance_token(p);
        HhyNode *entry = hhy_node_new(HHY_N_MAP_ENTRY, key);
        consume(p, HHY_T_COLON, "expected `:` after map key");
        skip_newlines(p);
        hhy_node_add(entry, parse_expression(p));
        hhy_node_add(map, entry);
        skip_newlines(p);
        if (!match_token(p, HHY_T_COMMA)) break;
        skip_newlines(p);
    }
    consume(p, HHY_T_RBRACE, "expected `}` after map");
    return map;
}

static HhyNode *parse_list(Parser *p, HhyToken open) {
    HhyNode *list = hhy_node_new(HHY_N_LIST, open);
    skip_newlines(p);
    while (!check(p, HHY_T_RBRACKET) && !is_at_end(p)) {
        hhy_node_add(list, parse_expression(p));
        skip_newlines(p);
        if (!match_token(p, HHY_T_COMMA)) break;
        skip_newlines(p);
    }
    consume(p, HHY_T_RBRACKET, "expected `]` after list");
    return list;
}

static HhyNode *parse_primary(Parser *p) {
    const HhyToken *token = peek(p);
    switch (token->kind) {
        case HHY_T_INT: case HHY_T_FLOAT: case HHY_T_STRING: case HHY_T_REGEX:
        case HHY_T_BYTES: case HHY_T_DURATION: case HHY_T_PERCENT:
        case HHY_T_TRUE: case HHY_T_FALSE: case HHY_T_NULL:
            advance_token(p);
            return hhy_node_new(HHY_N_LITERAL, *token);
        case HHY_T_IDENTIFIER: case HHY_T_THROW:
            advance_token(p);
            return hhy_node_new(HHY_N_IDENTIFIER, *token);
        case HHY_T_LPAREN: {
            advance_token(p);
            skip_newlines(p);
            HhyNode *expr = parse_expression(p);
            skip_newlines(p);
            consume(p, HHY_T_RPAREN, "expected `)` after expression");
            return expr;
        }
        case HHY_T_LBRACKET:
            advance_token(p);
            return parse_list(p, *token);
        case HHY_T_LBRACE:
            advance_token(p);
            return parse_map(p, *token);
        case HHY_T_ATTEMPT: {
            advance_token(p);
            HhyNode *node = hhy_node_new(HHY_N_ATTEMPT, *token);
            hhy_node_add(node, parse_block(p));
            return node;
        }
        default:
            error_at(p, token, "expected expression");
            switch (token->kind) {
                case HHY_T_LET: case HHY_T_FN: case HHY_T_IMPORT: case HHY_T_EXPORT:
                case HHY_T_IF: case HHY_T_FOR: case HHY_T_WHILE: case HHY_T_TRY:
                case HHY_T_RETURN: case HHY_T_RBRACE: case HHY_T_EOF:
                    break;
                default: advance_token(p); break;
            }
            return hhy_node_new(HHY_N_LITERAL, *token);
    }
}

static HhyNode *parse_call_args(Parser *p, HhyNode *callee, HhyToken open) {
    HhyNode *call = hhy_node_new(HHY_N_CALL, open);
    hhy_node_add(call, callee);
    skip_newlines(p);
    while (!check(p, HHY_T_RPAREN) && !is_at_end(p)) {
        hhy_node_add(call, parse_expression(p));
        skip_newlines(p);
        if (!match_token(p, HHY_T_COMMA)) break;
        skip_newlines(p);
    }
    consume(p, HHY_T_RPAREN, "expected `)` after arguments");
    return call;
}

static HhyNode *parse_postfix(Parser *p) {
    HhyNode *expr = parse_primary(p);
    for (;;) {
        if (match_token(p, HHY_T_LPAREN)) {
            expr = parse_call_args(p, expr, *previous(p));
        } else if (match_token(p, HHY_T_DOT)) {
            const HhyToken *name = consume(p, HHY_T_IDENTIFIER, "expected member name after `.`");
            HhyNode *member = hhy_node_new(HHY_N_MEMBER, *name);
            hhy_node_add(member, expr);
            expr = member;
        } else if (match_token(p, HHY_T_LBRACKET)) {
            HhyNode *index = hhy_node_new(HHY_N_INDEX, *previous(p));
            hhy_node_add(index, expr);
            skip_newlines(p);
            hhy_node_add(index, parse_expression(p));
            skip_newlines(p);
            consume(p, HHY_T_RBRACKET, "expected `]` after index");
            expr = index;
        } else break;
    }
    return expr;
}

static HhyNode *parse_unary(Parser *p) {
    if (match_token(p, HHY_T_NOT) || match_token(p, HHY_T_MINUS) || match_token(p, HHY_T_PLUS)) {
        HhyToken op = *previous(p);
        HhyNode *node = hhy_node_new(HHY_N_UNARY, op);
        hhy_node_add(node, parse_unary(p));
        return node;
    }
    return parse_postfix(p);
}

static HhyNode *binary(Parser *p, HhyNode *(*next)(Parser *), const HhyTokenKind *kinds, size_t count) {
    HhyNode *left = next(p);
    for (;;) {
        HhyTokenKind kind = peek(p)->kind;
        bool found = false;
        for (size_t i = 0; i < count; i++) if (kind == kinds[i]) { found = true; break; }
        if (!found) break;
        HhyToken op = *advance_token(p);
        skip_newlines(p);
        HhyNode *node = hhy_node_new(HHY_N_BINARY, op);
        hhy_node_add(node, left);
        hhy_node_add(node, next(p));
        left = node;
    }
    return left;
}

static HhyNode *parse_factor(Parser *p) {
    static const HhyTokenKind kinds[] = {HHY_T_STAR, HHY_T_SLASH, HHY_T_MOD};
    return binary(p, parse_unary, kinds, 3);
}
static HhyNode *parse_term(Parser *p) {
    static const HhyTokenKind kinds[] = {HHY_T_PLUS, HHY_T_MINUS};
    return binary(p, parse_factor, kinds, 2);
}
static HhyNode *parse_range(Parser *p) {
    HhyNode *left = parse_term(p);
    if (match_token(p, HHY_T_RANGE)) {
        HhyNode *node = hhy_node_new(HHY_N_RANGE, *previous(p));
        hhy_node_add(node, left);
        skip_newlines(p);
        hhy_node_add(node, parse_term(p));
        return node;
    }
    return left;
}
static HhyNode *parse_comparison(Parser *p) {
    static const HhyTokenKind kinds[] = {HHY_T_LT, HHY_T_LTE, HHY_T_GT, HHY_T_GTE};
    return binary(p, parse_range, kinds, 4);
}
static HhyNode *parse_equality(Parser *p) {
    static const HhyTokenKind kinds[] = {HHY_T_EQUAL_EQUAL, HHY_T_BANG_EQUAL};
    return binary(p, parse_comparison, kinds, 2);
}
static HhyNode *parse_and(Parser *p) {
    static const HhyTokenKind kinds[] = {HHY_T_AND};
    return binary(p, parse_equality, kinds, 1);
}
static HhyNode *parse_or(Parser *p) {
    static const HhyTokenKind kinds[] = {HHY_T_OR};
    return binary(p, parse_and, kinds, 1);
}
static HhyNode *parse_coalesce(Parser *p) {
    static const HhyTokenKind kinds[] = {HHY_T_COALESCE};
    return binary(p, parse_or, kinds, 1);
}

static HhyNode *parse_closure(Parser *p) {
    HhyToken open = *consume(p, HHY_T_LBRACE, "expected closure");
    HhyNode *closure = hhy_node_new(HHY_N_CLOSURE, open);
    skip_newlines(p);
    if (check(p, HHY_T_IDENTIFIER) && p->current + 1 < p->tokens->count &&
        p->tokens->items[p->current + 1].kind == HHY_T_ARROW) {
        HhyToken param = *advance_token(p);
        advance_token(p);
        hhy_node_add(closure, hhy_node_new(HHY_N_IDENTIFIER, param));
        skip_newlines(p);
    }
    while (!check(p, HHY_T_RBRACE) && !is_at_end(p)) {
        hhy_node_add(closure, parse_declaration(p));
        if (p->panic) synchronize(p);
        skip_newlines(p);
    }
    consume(p, HHY_T_RBRACE, "expected `}` after closure");
    return closure;
}

static HhyNode *parse_pipe_stage(Parser *p) {
    if (!check(p, HHY_T_IDENTIFIER) && !check(p, HHY_T_THROW)) {
        error_at(p, peek(p), "expected callable after `|>`");
        return hhy_node_new(HHY_N_IDENTIFIER, synthetic_token(p));
    }
    HhyNode *stage = hhy_node_new(HHY_N_IDENTIFIER, *advance_token(p));
    while (match_token(p, HHY_T_DOT)) {
        const HhyToken *name = consume(p, HHY_T_IDENTIFIER, "expected name after `.`");
        HhyNode *member = hhy_node_new(HHY_N_MEMBER, *name);
        hhy_node_add(member, stage);
        stage = member;
    }
    if (match_token(p, HHY_T_LPAREN)) stage = parse_call_args(p, stage, *previous(p));
    size_t before_newlines = p->current;
    skip_newlines(p);
    if (check(p, HHY_T_LBRACE)) {
        HhyNode *call;
        if (stage->kind == HHY_N_CALL) call = stage;
        else { call = hhy_node_new(HHY_N_CALL, stage->token); hhy_node_add(call, stage); }
        hhy_node_add(call, parse_closure(p));
        stage = call;
    } else {
        /* A newline belongs to this stage only when it introduces a trailing
         * closure. Otherwise it is either the next pipe's indentation or the
         * statement terminator, so let parse_pipe/consume_terminator see it. */
        p->current = before_newlines;
    }
    return stage;
}

static HhyNode *parse_pipe(Parser *p) {
    HhyNode *left = parse_coalesce(p);
    for (;;) {
        size_t saved = p->current;
        while (check(p, HHY_T_NEWLINE)) advance_token(p);
        if (!match_token(p, HHY_T_PIPE)) {
            p->current = saved;
            break;
        }
        HhyToken op = *previous(p);
        skip_newlines(p);
        HhyNode *node = hhy_node_new(HHY_N_PIPE, op);
        hhy_node_add(node, left);
        hhy_node_add(node, parse_pipe_stage(p));
        left = node;
    }
    return left;
}

static HhyNode *parse_assignment(Parser *p) {
    HhyNode *left = parse_pipe(p);
    if (match_token(p, HHY_T_EQUAL)) {
        HhyNode *node = hhy_node_new(HHY_N_ASSIGN, *previous(p));
        hhy_node_add(node, left);
        skip_newlines(p);
        hhy_node_add(node, parse_assignment(p));
        return node;
    }
    return left;
}

static HhyNode *parse_expression(Parser *p) { return parse_assignment(p); }

static HhyNode *parse_let(Parser *p, HhyToken keyword_token) {
    HhyNode *node = hhy_node_new(HHY_N_LET_DECL, keyword_token);
    if (match_token(p, HHY_T_MUT)) hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *previous(p)));
    const HhyToken *name = consume(p, HHY_T_IDENTIFIER, "expected variable name");
    hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *name));
    consume(p, HHY_T_EQUAL, "expected `=` after variable name");
    skip_newlines(p);
    hhy_node_add(node, parse_expression(p));
    consume_terminator(p);
    return node;
}

static HhyNode *parse_fn(Parser *p, HhyToken keyword_token) {
    HhyNode *node = hhy_node_new(HHY_N_FN_DECL, keyword_token);
    const HhyToken *name = consume(p, HHY_T_IDENTIFIER, "expected function name");
    hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *name));
    consume(p, HHY_T_LPAREN, "expected `(` after function name");
    skip_newlines(p);
    while (!check(p, HHY_T_RPAREN) && !is_at_end(p)) {
        const HhyToken *param = consume(p, HHY_T_IDENTIFIER, "expected parameter name");
        hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *param));
        if (!match_token(p, HHY_T_COMMA)) break;
        skip_newlines(p);
    }
    consume(p, HHY_T_RPAREN, "expected `)` after parameters");
    skip_newlines(p);
    hhy_node_add(node, parse_block(p));
    return node;
}

static HhyNode *parse_import(Parser *p, HhyToken keyword_token) {
    HhyNode *node = hhy_node_new(HHY_N_IMPORT_DECL, keyword_token);
    if (check(p, HHY_T_STRING)) {
        HhyToken path = *advance_token(p);
        hhy_node_add(node, hhy_node_new(HHY_N_LITERAL, path));
        if (match_token(p, HHY_T_AS)) {
            hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *previous(p)));
            const HhyToken *alias = consume(p, HHY_T_IDENTIFIER, "expected alias after `as`");
            hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *alias));
        }
    } else if (match_token(p, HHY_T_LBRACE)) {
        hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *previous(p)));
        skip_newlines(p);
        if (check(p, HHY_T_RBRACE)) error_at(p, peek(p), "import list cannot be empty");
        while (!check(p, HHY_T_RBRACE) && !is_at_end(p)) {
            const HhyToken *name = consume(p, HHY_T_IDENTIFIER, "expected imported name");
            hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *name));
            if (match_token(p, HHY_T_AS)) {
                hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *previous(p)));
                const HhyToken *alias = consume(p, HHY_T_IDENTIFIER, "expected alias after `as`");
                hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *alias));
            }
            skip_newlines(p);
            if (!match_token(p, HHY_T_COMMA)) break;
            hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *previous(p)));
            skip_newlines(p);
            if (check(p, HHY_T_RBRACE)) {
                error_at(p, peek(p), "trailing comma is not allowed in import list");
                break;
            }
        }
        const HhyToken *close = consume(p, HHY_T_RBRACE, "expected `}` after import list");
        hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *close));
        const HhyToken *from = consume(p, HHY_T_FROM, "expected `from` after import list");
        hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *from));
        const HhyToken *path = consume(p, HHY_T_STRING, "expected module path after `from`");
        hhy_node_add(node, hhy_node_new(HHY_N_LITERAL, *path));
    } else if (check(p, HHY_T_IDENTIFIER)) {
        hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *advance_token(p)));
        while (match_token(p, HHY_T_DOT)) {
            hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *previous(p)));
            const HhyToken *part = consume(p, HHY_T_IDENTIFIER,
                                           "expected module name after `.`");
            hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *part));
        }
    } else {
        error_at(p, peek(p), "expected module after import");
    }
    consume_terminator(p);
    return node;
}

static HhyNode *parse_if(Parser *p, HhyToken token) {
    HhyNode *node = hhy_node_new(HHY_N_IF, token);
    hhy_node_add(node, parse_expression(p));
    skip_newlines(p);
    hhy_node_add(node, parse_block(p));
    skip_newlines(p);
    if (match_token(p, HHY_T_ELSE)) {
        skip_newlines(p);
        if (match_token(p, HHY_T_IF)) hhy_node_add(node, parse_if(p, *previous(p)));
        else hhy_node_add(node, parse_block(p));
    }
    return node;
}

static HhyNode *parse_for(Parser *p, HhyToken token) {
    HhyNode *node = hhy_node_new(HHY_N_FOR, token);
    const HhyToken *name = consume(p, HHY_T_IDENTIFIER, "expected loop variable");
    hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *name));
    consume(p, HHY_T_IN, "expected `in` after loop variable");
    hhy_node_add(node, parse_expression(p));
    skip_newlines(p);
    hhy_node_add(node, parse_block(p));
    return node;
}

static HhyNode *parse_while(Parser *p, HhyToken token) {
    HhyNode *node = hhy_node_new(HHY_N_WHILE, token);
    hhy_node_add(node, parse_expression(p));
    skip_newlines(p);
    hhy_node_add(node, parse_block(p));
    return node;
}

static HhyNode *parse_try(Parser *p, HhyToken token) {
    HhyNode *node = hhy_node_new(HHY_N_TRY, token);
    skip_newlines(p);
    hhy_node_add(node, parse_block(p));
    skip_newlines(p);
    consume(p, HHY_T_CATCH, "expected `catch` after try block");
    const HhyToken *name = consume(p, HHY_T_IDENTIFIER, "expected error name after catch");
    hhy_node_add(node, hhy_node_new(HHY_N_IDENTIFIER, *name));
    skip_newlines(p);
    hhy_node_add(node, parse_block(p));
    return node;
}

static HhyNode *parse_statement(Parser *p) {
    if (match_token(p, HHY_T_IF)) return parse_if(p, *previous(p));
    if (match_token(p, HHY_T_FOR)) return parse_for(p, *previous(p));
    if (match_token(p, HHY_T_WHILE)) return parse_while(p, *previous(p));
    if (match_token(p, HHY_T_TRY)) return parse_try(p, *previous(p));
    if (match_token(p, HHY_T_RETURN)) {
        HhyNode *node = hhy_node_new(HHY_N_RETURN, *previous(p));
        if (!at_terminator(p)) hhy_node_add(node, parse_expression(p));
        consume_terminator(p);
        return node;
    }
    if (match_token(p, HHY_T_BREAK) || match_token(p, HHY_T_CONTINUE)) {
        HhyToken token = *previous(p);
        HhyNodeKind kind = token.kind == HHY_T_BREAK ? HHY_N_BREAK : HHY_N_CONTINUE;
        HhyNode *node = hhy_node_new(kind, token);
        consume_terminator(p);
        return node;
    }
    HhyNode *node = hhy_node_new(HHY_N_EXPR_STMT, *peek(p));
    hhy_node_add(node, parse_expression(p));
    consume_terminator(p);
    return node;
}

static HhyNode *parse_declaration(Parser *p) {
    if (match_token(p, HHY_T_LET)) return parse_let(p, *previous(p));
    if (match_token(p, HHY_T_FN)) return parse_fn(p, *previous(p));
    if (match_token(p, HHY_T_IMPORT)) return parse_import(p, *previous(p));
    if (match_token(p, HHY_T_EXPORT)) {
        HhyNode *node = hhy_node_new(HHY_N_EXPORT_DECL, *previous(p));
        if (match_token(p, HHY_T_LET)) hhy_node_add(node, parse_let(p, *previous(p)));
        else if (match_token(p, HHY_T_FN)) hhy_node_add(node, parse_fn(p, *previous(p)));
        else error_at(p, peek(p), "expected `let` or `fn` after export");
        return node;
    }
    return parse_statement(p);
}

HhyParseResult hhy_parse(const HhySource *source, const HhyTokenList *tokens, HhyNode **out) {
    Parser parser = {.source = source, .tokens = tokens};
    HhyNode *program = hhy_node_new(HHY_N_PROGRAM, tokens->count > 0 ? tokens->items[0] : synthetic_token(&parser));
    skip_newlines(&parser);
    while (!is_at_end(&parser)) {
        size_t start = parser.current;
        hhy_node_add(program, parse_declaration(&parser));
        if (parser.panic) synchronize(&parser);
        skip_newlines(&parser);
        /* Error recovery must always make progress, including a stray top-level `}`
           immediately following a newline. */
        if (parser.current == start && !is_at_end(&parser)) advance_token(&parser);
    }
    *out = program;
    HhyParseResult result = {.ok = parser.errors == 0, .error_count = parser.errors};
    return result;
}

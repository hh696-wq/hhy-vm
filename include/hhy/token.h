#ifndef HHY_TOKEN_H
#define HHY_TOKEN_H

#include "hhy/common.h"

typedef enum {
    HHY_T_EOF,
    HHY_T_ERROR,
    HHY_T_NEWLINE,
    HHY_T_IDENTIFIER,
    HHY_T_INT,
    HHY_T_FLOAT,
    HHY_T_STRING,
    HHY_T_REGEX,
    HHY_T_BYTES,
    HHY_T_DURATION,
    HHY_T_PERCENT,

    HHY_T_LPAREN, HHY_T_RPAREN,
    HHY_T_LBRACKET, HHY_T_RBRACKET,
    HHY_T_LBRACE, HHY_T_RBRACE,
    HHY_T_COMMA, HHY_T_DOT, HHY_T_COLON, HHY_T_SEMICOLON,
    HHY_T_PLUS, HHY_T_MINUS, HHY_T_STAR, HHY_T_SLASH, HHY_T_MOD,
    HHY_T_EQUAL, HHY_T_EQUAL_EQUAL, HHY_T_BANG_EQUAL,
    HHY_T_LT, HHY_T_LTE, HHY_T_GT, HHY_T_GTE,
    HHY_T_PIPE, HHY_T_ARROW, HHY_T_RANGE, HHY_T_COALESCE,

    HHY_T_LET, HHY_T_MUT, HHY_T_FN,
    HHY_T_IF, HHY_T_ELSE, HHY_T_FOR, HHY_T_IN, HHY_T_WHILE,
    HHY_T_RETURN, HHY_T_BREAK, HHY_T_CONTINUE,
    HHY_T_TRY, HHY_T_CATCH, HHY_T_THROW, HHY_T_ATTEMPT,
    HHY_T_IMPORT, HHY_T_EXPORT, HHY_T_AS, HHY_T_FROM,
    HHY_T_TRUE, HHY_T_FALSE, HHY_T_NULL,
    HHY_T_AND, HHY_T_OR, HHY_T_NOT
} HhyTokenKind;

typedef struct {
    HhyTokenKind kind;
    const char *start;
    size_t length;
    uint32_t line;
    uint32_t column;
} HhyToken;

typedef struct {
    HhyToken *items;
    size_t count;
    size_t capacity;
} HhyTokenList;

bool hhy_lex(const HhySource *source, HhyTokenList *out);
void hhy_tokens_free(HhyTokenList *tokens);
const char *hhy_token_kind_name(HhyTokenKind kind);
void hhy_tokens_print(const HhyTokenList *tokens);

#endif


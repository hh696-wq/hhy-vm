#include "hhy/token.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const HhySource *source;
    const char *start;
    const char *current;
    const char *end;
    uint32_t line;
    uint32_t column;
    uint32_t token_column;
    HhyTokenList *tokens;
    HhyTokenKind previous;
    bool failed;
} Lexer;

static bool source_utf8_valid(const HhySource *source, size_t *invalid_offset) {
    const unsigned char *bytes = (const unsigned char *)source->text;
    for (size_t i = 0; i < source->length;) {
        unsigned char first = bytes[i];
        size_t width = 0;
        if (first <= 0x7f) { i++; continue; }
        if (first >= 0xc2 && first <= 0xdf) width = 2;
        else if (first >= 0xe0 && first <= 0xef) width = 3;
        else if (first >= 0xf0 && first <= 0xf4) width = 4;
        else { *invalid_offset = i; return false; }
        if (i + width > source->length) { *invalid_offset = i; return false; }
        for (size_t j = 1; j < width; j++)
            if ((bytes[i + j] & 0xc0) != 0x80) { *invalid_offset = i + j; return false; }
        if ((first == 0xe0 && bytes[i + 1] < 0xa0) ||
            (first == 0xed && bytes[i + 1] >= 0xa0) ||
            (first == 0xf0 && bytes[i + 1] < 0x90) ||
            (first == 0xf4 && bytes[i + 1] >= 0x90)) {
            *invalid_offset = i; return false;
        }
        i += width;
    }
    return true;
}

static void push_token(Lexer *lexer, HhyTokenKind kind) {
    if (lexer->tokens->count == lexer->tokens->capacity) {
        size_t capacity = lexer->tokens->capacity < 16 ? 16 : lexer->tokens->capacity * 2;
        lexer->tokens->items = hhy_realloc(lexer->tokens->items, capacity * sizeof(HhyToken));
        lexer->tokens->capacity = capacity;
    }
    HhyToken token = {
        .kind = kind,
        .start = lexer->start,
        .length = (size_t)(lexer->current - lexer->start),
        .line = lexer->line,
        .column = lexer->token_column
    };
    lexer->tokens->items[lexer->tokens->count++] = token;
    if (kind != HHY_T_NEWLINE && kind != HHY_T_ERROR) lexer->previous = kind;
}

static bool at_end(const Lexer *lexer) { return lexer->current >= lexer->end; }
static char peek(const Lexer *lexer) { return at_end(lexer) ? '\0' : *lexer->current; }
static char peek_next(const Lexer *lexer) { return lexer->current + 1 >= lexer->end ? '\0' : lexer->current[1]; }

static char advance(Lexer *lexer) {
    char c = *lexer->current++;
    lexer->column++;
    return c;
}

static bool match(Lexer *lexer, char expected) {
    if (at_end(lexer) || *lexer->current != expected) return false;
    lexer->current++;
    lexer->column++;
    return true;
}

static bool text_is(const char *start, size_t length, const char *word) {
    return strlen(word) == length && memcmp(start, word, length) == 0;
}

static HhyTokenKind keyword(const char *start, size_t length) {
    struct Keyword { const char *word; HhyTokenKind kind; };
    static const struct Keyword words[] = {
        {"let", HHY_T_LET}, {"mut", HHY_T_MUT}, {"fn", HHY_T_FN},
        {"if", HHY_T_IF}, {"else", HHY_T_ELSE}, {"for", HHY_T_FOR},
        {"in", HHY_T_IN}, {"while", HHY_T_WHILE}, {"return", HHY_T_RETURN},
        {"break", HHY_T_BREAK}, {"continue", HHY_T_CONTINUE},
        {"try", HHY_T_TRY}, {"catch", HHY_T_CATCH}, {"throw", HHY_T_THROW},
        {"attempt", HHY_T_ATTEMPT}, {"import", HHY_T_IMPORT},
        {"export", HHY_T_EXPORT}, {"as", HHY_T_AS}, {"from", HHY_T_FROM},
        {"true", HHY_T_TRUE}, {"false", HHY_T_FALSE}, {"null", HHY_T_NULL},
        {"and", HHY_T_AND}, {"or", HHY_T_OR}, {"not", HHY_T_NOT}
    };
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        if (text_is(start, length, words[i].word)) return words[i].kind;
    }
    return HHY_T_IDENTIFIER;
}

static void identifier(Lexer *lexer) {
    while (isalnum((unsigned char)peek(lexer)) || peek(lexer) == '_') advance(lexer);
    push_token(lexer, keyword(lexer->start, (size_t)(lexer->current - lexer->start)));
}

static bool suffix_is(const Lexer *lexer, const char *suffix) {
    size_t length = strlen(suffix);
    return (size_t)(lexer->end - lexer->current) >= length && memcmp(lexer->current, suffix, length) == 0;
}

static void consume_suffix(Lexer *lexer, size_t length) {
    lexer->current += length;
    lexer->column += (uint32_t)length;
}

static void number(Lexer *lexer) {
    if (lexer->start[0] == '0' && (peek(lexer) == 'x' || peek(lexer) == 'X')) {
        advance(lexer);
        while (isxdigit((unsigned char)peek(lexer)) || peek(lexer) == '_') advance(lexer);
        push_token(lexer, HHY_T_INT);
        return;
    }
    if (lexer->start[0] == '0' && (peek(lexer) == 'b' || peek(lexer) == 'B') &&
        (peek_next(lexer) == '0' || peek_next(lexer) == '1' || peek_next(lexer) == '_')) {
        advance(lexer);
        while (peek(lexer) == '0' || peek(lexer) == '1' || peek(lexer) == '_') advance(lexer);
        push_token(lexer, HHY_T_INT);
        return;
    }
    while (isdigit((unsigned char)peek(lexer)) || peek(lexer) == '_') advance(lexer);
    bool floating = false;
    if (peek(lexer) == '.' && peek_next(lexer) != '.' && isdigit((unsigned char)peek_next(lexer))) {
        floating = true;
        advance(lexer);
        while (isdigit((unsigned char)peek(lexer)) || peek(lexer) == '_') advance(lexer);
    }
    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        floating = true;
        advance(lexer);
        if (peek(lexer) == '+' || peek(lexer) == '-') advance(lexer);
        while (isdigit((unsigned char)peek(lexer)) || peek(lexer) == '_') advance(lexer);
    }
    static const char *byte_units[] = {"kib", "mib", "gib", "tib", "kb", "mb", "gb", "tb", "b"};
    for (size_t i = 0; i < sizeof(byte_units) / sizeof(byte_units[0]); i++) {
        if (suffix_is(lexer, byte_units[i])) {
            consume_suffix(lexer, strlen(byte_units[i]));
            push_token(lexer, HHY_T_BYTES);
            return;
        }
    }
    static const char *duration_units[] = {"min", "ns", "us", "ms", "s", "h", "d"};
    for (size_t i = 0; i < sizeof(duration_units) / sizeof(duration_units[0]); i++) {
        if (suffix_is(lexer, duration_units[i])) {
            consume_suffix(lexer, strlen(duration_units[i]));
            push_token(lexer, HHY_T_DURATION);
            return;
        }
    }
    if (peek(lexer) == '%') {
        advance(lexer);
        push_token(lexer, HHY_T_PERCENT);
        return;
    }
    push_token(lexer, floating ? HHY_T_FLOAT : HHY_T_INT);
}

static void string(Lexer *lexer) {
    bool escaped = false;
    while (!at_end(lexer)) {
        char c = advance(lexer);
        if (c == '\n' && !escaped) {
            if (!hhy_suppress_diagnostics)
                fprintf(stderr, "%s:%u:%u: error: unterminated string\n", lexer->source->path, lexer->line, lexer->token_column);
            lexer->failed = true;
            push_token(lexer, HHY_T_ERROR);
            return;
        }
        if (c == '"' && !escaped) {
            push_token(lexer, HHY_T_STRING);
            return;
        }
        if (escaped && strchr("nrt\"\\bf0", c) == NULL) {
            if (!hhy_suppress_diagnostics)
                fprintf(stderr, "%s:%u:%u: error: invalid string escape \\%c\n",
                        lexer->source->path, lexer->line, lexer->column - 1, c);
            lexer->failed = true;
            while (!at_end(lexer) && peek(lexer) != '"' && peek(lexer) != '\n' && peek(lexer) != '\r')
                advance(lexer);
            if (!at_end(lexer) && peek(lexer) == '"') advance(lexer);
            push_token(lexer, HHY_T_ERROR);
            return;
        }
        if (c == '\\' && !escaped) escaped = true;
        else escaped = false;
    }
    if (!hhy_suppress_diagnostics)
        fprintf(stderr, "%s:%u:%u: error: unterminated string\n", lexer->source->path, lexer->line, lexer->token_column);
    lexer->failed = true;
    push_token(lexer, HHY_T_ERROR);
}

static bool can_end_expression(HhyTokenKind kind) {
    switch (kind) {
        case HHY_T_IDENTIFIER: case HHY_T_INT: case HHY_T_FLOAT: case HHY_T_STRING:
        case HHY_T_REGEX: case HHY_T_BYTES: case HHY_T_DURATION: case HHY_T_PERCENT:
        case HHY_T_TRUE: case HHY_T_FALSE: case HHY_T_NULL:
        case HHY_T_RPAREN: case HHY_T_RBRACKET: case HHY_T_RBRACE:
            return true;
        default: return false;
    }
}

static void regex(Lexer *lexer) {
    bool escaped = false;
    bool in_class = false;
    while (!at_end(lexer)) {
        char c = advance(lexer);
        if (c == '\n' && !escaped) break;
        if (c == '[' && !escaped) in_class = true;
        if (c == ']' && !escaped) in_class = false;
        if (c == '/' && !escaped && !in_class) {
            while (!at_end(lexer) && strchr("imsu", peek(lexer)) != NULL) advance(lexer);
            push_token(lexer, HHY_T_REGEX);
            return;
        }
        if (c == '\\' && !escaped) escaped = true;
        else escaped = false;
    }
    if (!hhy_suppress_diagnostics)
        fprintf(stderr, "%s:%u:%u: error: unterminated regex\n", lexer->source->path, lexer->line, lexer->token_column);
    lexer->failed = true;
    push_token(lexer, HHY_T_ERROR);
}

static void newline(Lexer *lexer) {
    if (lexer->start[0] == '\r' && peek(lexer) == '\n') advance(lexer);
    push_token(lexer, HHY_T_NEWLINE);
    lexer->line++;
    lexer->column = 1;
}

bool hhy_lex(const HhySource *source, HhyTokenList *out) {
    memset(out, 0, sizeof(*out));
    size_t invalid_offset = 0;
    if (!source_utf8_valid(source, &invalid_offset)) {
        uint32_t line = 1, column = 1;
        for (size_t i = 0; i < invalid_offset; i++) {
            if (source->text[i] == '\n') { line++; column = 1; }
            else column++;
        }
        if (!hhy_suppress_diagnostics)
            fprintf(stderr, "%s:%u:%u: error: source is not valid UTF-8\n",
                    source->path, line, column);
        return false;
    }
    Lexer lexer = {
        .source = source, .start = source->text, .current = source->text,
        .end = source->text + source->length, .line = 1, .column = 1,
        .token_column = 1, .tokens = out, .previous = HHY_T_NEWLINE
    };
    if (source->length >= 2 && source->text[0] == '#' && source->text[1] == '!') {
        while (!at_end(&lexer) && peek(&lexer) != '\n' && peek(&lexer) != '\r') advance(&lexer);
    }
    while (!at_end(&lexer)) {
        lexer.start = lexer.current;
        lexer.token_column = lexer.column;
        char c = advance(&lexer);
        switch (c) {
            case ' ': case '\t': break;
            case '\n': case '\r': newline(&lexer); break;
            case '#': while (!at_end(&lexer) && peek(&lexer) != '\n' && peek(&lexer) != '\r') advance(&lexer); break;
            case '(': push_token(&lexer, HHY_T_LPAREN); break;
            case ')': push_token(&lexer, HHY_T_RPAREN); break;
            case '[': push_token(&lexer, HHY_T_LBRACKET); break;
            case ']': push_token(&lexer, HHY_T_RBRACKET); break;
            case '{': push_token(&lexer, HHY_T_LBRACE); break;
            case '}': push_token(&lexer, HHY_T_RBRACE); break;
            case ',': push_token(&lexer, HHY_T_COMMA); break;
            case ':': push_token(&lexer, HHY_T_COLON); break;
            case ';': push_token(&lexer, HHY_T_SEMICOLON); break;
            case '+': push_token(&lexer, HHY_T_PLUS); break;
            case '*': push_token(&lexer, HHY_T_STAR); break;
            case '%': push_token(&lexer, HHY_T_MOD); break;
            case '.': push_token(&lexer, match(&lexer, '.') ? HHY_T_RANGE : HHY_T_DOT); break;
            case '-': push_token(&lexer, match(&lexer, '>') ? HHY_T_ARROW : HHY_T_MINUS); break;
            case '=': push_token(&lexer, match(&lexer, '=') ? HHY_T_EQUAL_EQUAL : HHY_T_EQUAL); break;
            case '!':
                if (match(&lexer, '=')) push_token(&lexer, HHY_T_BANG_EQUAL);
                else { lexer.failed = true; push_token(&lexer, HHY_T_ERROR); }
                break;
            case '<': push_token(&lexer, match(&lexer, '=') ? HHY_T_LTE : HHY_T_LT); break;
            case '>': push_token(&lexer, match(&lexer, '=') ? HHY_T_GTE : HHY_T_GT); break;
            case '?':
                if (match(&lexer, '?')) push_token(&lexer, HHY_T_COALESCE);
                else { lexer.failed = true; push_token(&lexer, HHY_T_ERROR); }
                break;
            case '|':
                if (match(&lexer, '>')) push_token(&lexer, HHY_T_PIPE);
                else { lexer.failed = true; push_token(&lexer, HHY_T_ERROR); }
                break;
            case '"': string(&lexer); break;
            case '/':
                if (can_end_expression(lexer.previous)) push_token(&lexer, HHY_T_SLASH);
                else regex(&lexer);
                break;
            default:
                if (isdigit((unsigned char)c)) number(&lexer);
                else if (isalpha((unsigned char)c) || c == '_') identifier(&lexer);
                else {
                    if (!hhy_suppress_diagnostics)
                        fprintf(stderr, "%s:%u:%u: error: unexpected byte 0x%02x\n", source->path, lexer.line, lexer.token_column, (unsigned char)c);
                    lexer.failed = true;
                    push_token(&lexer, HHY_T_ERROR);
                }
                break;
        }
    }
    lexer.start = lexer.current;
    lexer.token_column = lexer.column;
    push_token(&lexer, HHY_T_EOF);
    return !lexer.failed;
}

void hhy_tokens_free(HhyTokenList *tokens) {
    free(tokens->items);
    memset(tokens, 0, sizeof(*tokens));
}

const char *hhy_token_kind_name(HhyTokenKind kind) {
    static const char *names[] = {
        "EOF","ERROR","NEWLINE","IDENTIFIER","INT","FLOAT","STRING","REGEX","BYTES","DURATION","PERCENT",
        "(",")","[","]","{","}",",",".",":",";","+","-","*","/","%","=","==","!=","<","<=",">",">=","|>","->","..","??",
        "let","mut","fn","if","else","for","in","while","return","break","continue","try","catch","throw","attempt","import","export","as","from","true","false","null","and","or","not"
    };
    return names[(size_t)kind];
}

void hhy_tokens_print(const HhyTokenList *tokens) {
    for (size_t i = 0; i < tokens->count; i++) {
        const HhyToken *token = &tokens->items[i];
        printf("%4u:%-3u %-12s %.*s\n", token->line, token->column,
               hhy_token_kind_name(token->kind), (int)token->length, token->start);
    }
}

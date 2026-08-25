#include "hhy/formatter.h"

#include <stdlib.h>
#include <string.h>

static bool opening(HhyTokenKind kind) {
    return kind == HHY_T_LBRACE || kind == HHY_T_LBRACKET || kind == HHY_T_LPAREN;
}

static bool closing(HhyTokenKind kind) {
    return kind == HHY_T_RBRACE || kind == HHY_T_RBRACKET || kind == HHY_T_RPAREN;
}

char *hhy_format_source(const HhySource *source, const HhyTokenList *tokens, size_t *out_length) {
    size_t capacity = source->length + 64;
    char *output = hhy_alloc(capacity);
    size_t length = 0, token_index = 0;
    unsigned line_number = 1;
    int depth = 0;
    const char *line = source->text, *end = source->text + source->length;
    while (line < end) {
        const char *line_end = line;
        while (line_end < end && *line_end != '\n' && *line_end != '\r') line_end++;
        const char *content = line;
        while (content < line_end && (*content == ' ' || *content == '\t')) content++;
        const char *trimmed_end = line_end;
        while (trimmed_end > content && (trimmed_end[-1] == ' ' || trimmed_end[-1] == '\t')) trimmed_end--;

        while (token_index < tokens->count && tokens->items[token_index].line < line_number) token_index++;
        size_t first = token_index, after = first;
        while (after < tokens->count && tokens->items[after].line == line_number &&
               tokens->items[after].kind != HHY_T_NEWLINE && tokens->items[after].kind != HHY_T_EOF) after++;
        int leading_closers = 0;
        for (size_t i = first; i < after && closing(tokens->items[i].kind); i++) leading_closers++;
        int line_indent = depth - leading_closers;
        if (line_indent < 0) line_indent = 0;
        if (first < after && tokens->items[first].kind == HHY_T_PIPE) line_indent++;

        bool blank = content == trimmed_end;
        bool shebang = line_number == 1 && trimmed_end - content >= 2 && content[0] == '#' && content[1] == '!';
        size_t needed = length + (blank || shebang ? 0 : (size_t)line_indent * 4) +
                        (size_t)(trimmed_end - content) + 2;
        if (needed > capacity) {
            while (capacity < needed) capacity *= 2;
            output = hhy_realloc(output, capacity);
        }
        if (!blank && !shebang) {
            for (int i = 0; i < line_indent * 4; i++) output[length++] = ' ';
        }
        if (!blank) {
            memcpy(output + length, content, (size_t)(trimmed_end - content));
            length += (size_t)(trimmed_end - content);
        }
        output[length++] = '\n';

        for (size_t i = first; i < after; i++) {
            if (opening(tokens->items[i].kind)) depth++;
            else if (closing(tokens->items[i].kind) && depth > 0) depth--;
        }
        token_index = after;
        if (line_end < end && *line_end == '\r') line_end++;
        if (line_end < end && *line_end == '\n') line_end++;
        line = line_end;
        line_number++;
    }
    if (source->length == 0) output[length++] = '\n';
    output[length] = '\0';
    *out_length = length;
    return output;
}

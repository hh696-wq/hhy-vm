#ifndef HHY_PARSER_H
#define HHY_PARSER_H

#include "hhy/ast.h"

typedef struct {
    bool ok;
    size_t error_count;
} HhyParseResult;

HhyParseResult hhy_parse(const HhySource *source, const HhyTokenList *tokens, HhyNode **out);

#endif


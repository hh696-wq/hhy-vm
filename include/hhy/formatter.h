#ifndef HHY_FORMATTER_H
#define HHY_FORMATTER_H

#include "hhy/token.h"

char *hhy_format_source(const HhySource *source, const HhyTokenList *tokens, size_t *length);

#endif

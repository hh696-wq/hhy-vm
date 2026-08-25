#ifndef HHY_CHECKER_H
#define HHY_CHECKER_H

#include "hhy/ast.h"

typedef struct {
    bool ok;
    size_t error_count;
    size_t warning_count;
} HhyCheckResult;

HhyCheckResult hhy_check(const HhySource *source, const HhyNode *program);

#endif

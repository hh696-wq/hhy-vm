#define _POSIX_C_SOURCE 200809L
#include "hhy/profiler.h"
#include "hhy/bytecode.h"

#include <assert.h>
#include <stdlib.h>

int main(void) {
    assert(setenv("HHY_PROFILE_DISPATCH", "1", 1) == 0);
    HhyProfileOptions options = {.json = true, .output = stdout, .engine = "bytecode"};
    HhyProfiler *p = hhy_profiler_start(&options, "unit", 0);
    assert(p != NULL);
    hhy_profiler_dispatch(NULL, HHY_OP_PROGRAM, HHY_PROFILE_DISPATCH_GENERIC);
    hhy_profiler_dispatch(p, HHY_OP_COUNT, HHY_PROFILE_DISPATCH_GENERIC);
    hhy_profiler_dispatch(p, HHY_OP_PROGRAM, HHY_PROFILE_DISPATCH_DOMAIN_COUNT);
    hhy_profiler_dispatch(p, HHY_OP_PROGRAM, HHY_PROFILE_DISPATCH_GENERIC);
    hhy_profiler_dispatch(p, HHY_OP_CALL, HHY_PROFILE_DISPATCH_GENERIC);
    hhy_profiler_dispatch(p, HHY_OP_IDENTIFIER, HHY_PROFILE_DISPATCH_ARGUMENT_ATTEMPT);
    hhy_profiler_dispatch(p, HHY_OP_LITERAL, HHY_PROFILE_DISPATCH_ARGUMENT_ATTEMPT);
    hhy_profiler_dispatch(p, HHY_OP_RETURN, HHY_PROFILE_DISPATCH_GENERIC);
    hhy_profiler_stop(p, 0);
    hhy_profiler_free(p);
    return 0;
}

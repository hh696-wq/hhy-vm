#include "hhy/runtime.h"

#define HHY_DEFAULT_MAX_MEMORY (512u * 1024u * 1024u)
#define HHY_DEFAULT_MAX_OPEN_FILES 256u
#define HHY_DEFAULT_MAX_PROCESSES 16u
#define HHY_DEFAULT_MAX_PARALLELISM 16u
#define HHY_DEFAULT_MAX_HTTP_BODY (16u * 1024u * 1024u)
#define HHY_DEFAULT_MAX_REGEX_STEPS 1000000u
#define HHY_DEFAULT_MAX_RECURSION 256u

HhyRuntimeLimits hhy_runtime_limits_default(void) {
    return (HhyRuntimeLimits){
        .max_memory = HHY_DEFAULT_MAX_MEMORY,
        .max_open_files = HHY_DEFAULT_MAX_OPEN_FILES,
        .max_processes = HHY_DEFAULT_MAX_PROCESSES,
        .max_parallelism = HHY_DEFAULT_MAX_PARALLELISM,
        .max_http_body = HHY_DEFAULT_MAX_HTTP_BODY,
        .max_regex_steps = HHY_DEFAULT_MAX_REGEX_STEPS,
        .max_recursion = HHY_DEFAULT_MAX_RECURSION,
        .max_runtime_ns = 0
    };
}

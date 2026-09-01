#ifndef HHY_RUNTIME_H
#define HHY_RUNTIME_H

#include "hhy/ast.h"
#include "hhy/profiler.h"

typedef struct {
    bool ok;
    int exit_code;
} HhyRunResult;

typedef enum {
    HHY_ENGINE_AST,
    HHY_ENGINE_BYTECODE
} HhyExecutionEngine;

typedef struct {
    size_t max_memory;
    size_t max_open_files;
    size_t max_processes;
    size_t max_parallelism;
    size_t max_http_body;
    uint32_t max_regex_steps;
    size_t max_recursion;
    uint64_t max_runtime_ns;
} HhyRuntimeLimits;

HhyRuntimeLimits hhy_runtime_limits_default(void);

HhyRunResult hhy_run_program(const HhySource *source, const HhyNode *program,
                             int argc, char **argv, bool dry_run,
                             const HhyRuntimeLimits *limits);
HhyRunResult hhy_run_program_engine(const HhySource *source, const HhyNode *program,
                                    int argc, char **argv, bool dry_run,
                                    const HhyRuntimeLimits *limits,
                                    HhyExecutionEngine engine);
HhyRunResult hhy_profile_program(const HhySource *source, const HhyNode *program,
                                 int argc, char **argv, bool dry_run,
                                 const HhyRuntimeLimits *limits,
                                 const HhyProfileOptions *profile);
HhyRunResult hhy_profile_program_engine(const HhySource *source, const HhyNode *program,
                                        int argc, char **argv, bool dry_run,
                                        const HhyRuntimeLimits *limits,
                                        const HhyProfileOptions *profile,
                                        HhyExecutionEngine engine);
int hhy_repl(void);

#endif

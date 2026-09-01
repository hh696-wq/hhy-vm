#ifndef HHY_BYTECODE_RUNTIME_H
#define HHY_BYTECODE_RUNTIME_H

#include "hhy/ast.h"
#include "hhy/bytecode.h"

typedef struct HhyPreparedBytecode HhyPreparedBytecode;

#define HHY_BYTECODE_RUNTIME_BOUNDARY_VERSION 3u

HhyBytecodeResult hhy_bytecode_runtime_prepare(const HhyNode *program,
                                               const char *fault_injection,
                                               HhyPreparedBytecode **out_program);
void hhy_bytecode_runtime_free(HhyPreparedBytecode *program);
const HhyBytecodeExecutionPlan *hhy_bytecode_runtime_plan(
    const HhyPreparedBytecode *program);
const HhyBytecodeChunk *hhy_bytecode_runtime_chunk(
    const HhyPreparedBytecode *program);

#endif

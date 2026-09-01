#include "bytecode_runtime.h"
#include "hhy/common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct HhyPreparedBytecode {
    HhyBytecodeChunk chunk;
    HhyBytecodeExecutionPlan plan;
};

_Static_assert(HHY_BYTECODE_RUNTIME_BOUNDARY_VERSION == 2u,
               "Bytecode Runtime boundary changes require an explicit version bump");

static HhyBytecodeResult boundary_error(const char *message) {
    HhyBytecodeResult result = {.ok = false, .instruction = 0, .message = ""};
    snprintf(result.message, sizeof(result.message), "%s", message);
    return result;
}

HhyBytecodeResult hhy_bytecode_runtime_prepare(const HhyNode *program,
                                               const char *fault_injection,
                                               HhyPreparedBytecode **out_program) {
    if (out_program == NULL) return boundary_error("Bytecode boundary received a null output");
    *out_program = NULL;
    HhyPreparedBytecode *prepared = hhy_alloc(sizeof(*prepared));
    hhy_bytecode_chunk_init(&prepared->chunk);
    memset(&prepared->plan, 0, sizeof(prepared->plan));
    HhyBytecodeResult result = hhy_bytecode_compile(program, &prepared->chunk);
    if (result.ok && fault_injection != NULL) {
        if (strcmp(fault_injection, "invalid-opcode") == 0 && prepared->chunk.count > 0)
            prepared->chunk.code[0].opcode = HHY_OP_COUNT;
        else if (strcmp(fault_injection, "missing-halt") == 0 && prepared->chunk.count > 0)
            prepared->chunk.count--;
        else if (strcmp(fault_injection, "invalid-opcode") != 0 &&
                 strcmp(fault_injection, "missing-halt") != 0)
            result = boundary_error("unknown Bytecode fault injection");
    }
    if (result.ok) {
        result = hhy_bytecode_prepare_execution(&prepared->chunk,
                                                HHY_BYTECODE_MAX_NESTING + 1u,
                                                &prepared->plan);
    }
    if (!result.ok) {
        hhy_bytecode_chunk_free(&prepared->chunk);
        free(prepared);
        return result;
    }
    *out_program = prepared;
    return result;
}

void hhy_bytecode_runtime_free(HhyPreparedBytecode *program) {
    if (program == NULL) return;
    hhy_bytecode_chunk_free(&program->chunk);
    free(program);
}

const HhyBytecodeExecutionPlan *hhy_bytecode_runtime_plan(
    const HhyPreparedBytecode *program) {
    return program == NULL ? NULL : &program->plan;
}

const HhyBytecodeChunk *hhy_bytecode_runtime_chunk(
    const HhyPreparedBytecode *program) {
    return program == NULL ? NULL : &program->chunk;
}

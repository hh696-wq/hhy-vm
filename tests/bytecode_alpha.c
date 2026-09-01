#include "hhy/bytecode.h"
#include "hhy/common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *message) {
    fprintf(stderr, "bytecode alpha test failure: %s\n", message);
    exit(1);
}

static HhyBytecodeChunk minimal_chunk(void) {
    HhyBytecodeChunk chunk;
    hhy_bytecode_chunk_init(&chunk);
    chunk.code = hhy_alloc(3 * sizeof(*chunk.code));
    chunk.capacity = 3;
    chunk.count = 2;
    chunk.code[0] = (HhyInstruction){
        .opcode = HHY_OP_PROGRAM, .token_kind = HHY_T_EOF,
        .constant = HHY_BYTECODE_NO_CONSTANT, .child_count = 0,
        .subtree_size = 1, .line = 1, .column = 1
    };
    chunk.code[1] = (HhyInstruction){
        .opcode = HHY_OP_HALT, .token_kind = HHY_T_EOF,
        .constant = HHY_BYTECODE_NO_CONSTANT, .child_count = 0,
        .subtree_size = 1, .line = 1, .column = 1
    };
    return chunk;
}

static void expect_failure(HhyBytecodeChunk *chunk, const char *fragment) {
    HhyBytecodeResult verified = hhy_bytecode_verify(chunk);
    if (verified.ok || strstr(verified.message, fragment) == NULL) fail(fragment);
}

int main(void) {
    HhyBytecodeChunk chunk = minimal_chunk();
    if (!hhy_bytecode_verify(&chunk).ok) fail("minimal chunk was rejected");

    HhyBytecodeExecutionPlan plan;
    HhyBytecodeResult prepared = hhy_bytecode_prepare_execution(&chunk, 8, &plan);
    if (!prepared.ok || plan.instruction_pointer != 1 ||
        plan.max_operand_count != 1 || plan.max_frame_count != 1)
        fail("minimal execution plan was incorrect");
    prepared = hhy_bytecode_prepare_execution(&chunk, 0, &plan);
    if (prepared.ok || strstr(prepared.message, "positive") == NULL)
        fail("zero frame limit was accepted");

    chunk.code[0].opcode = HHY_OP_COUNT;
    expect_failure(&chunk, "invalid node opcode");
    chunk.code[0].opcode = HHY_OP_PROGRAM;

    chunk.code[0].constant = 0;
    expect_failure(&chunk, "constant index");
    chunk.code[0].constant = HHY_BYTECODE_NO_CONSTANT;

    chunk.code[0].subtree_size = 0;
    expect_failure(&chunk, "subtree size");
    chunk.code[0].subtree_size = 1;

    chunk.code[0].child_count = 1;
    expect_failure(&chunk, "invalid node opcode");
    chunk.code[0].child_count = 0;

    chunk.count = 1;
    expect_failure(&chunk, "missing HALT");
    chunk.count = 2;

    chunk.code[2] = chunk.code[1];
    chunk.count = 3;
    expect_failure(&chunk, "instructions follow HALT");

    chunk.count = 2;
    chunk.code[1].child_count = 1;
    expect_failure(&chunk, "canonical HALT");
    hhy_bytecode_chunk_free(&chunk);

    puts("bytecode verifier and execution planner tests passed");
    return 0;
}

#include "hhy/ast.h"
#include "hhy/bytecode.h"
#include "hhy/common.h"
#include "hhy/fuzz.h"
#include "hhy/parser.h"
#include "hhy/token.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    bool previous_suppression = hhy_suppress_diagnostics;
    hhy_suppress_diagnostics = true;
    bool trace = getenv("HHY_FUZZ_TRACE") != NULL;
    if (trace) { fputs("  lex\n", stderr); fflush(stderr); }
    char *text = hhy_alloc(size + 1);
    if (size > 0) memcpy(text, data, size);
    text[size] = '\0';
    HhySource source = {.path = "<fuzz>", .text = text, .length = size};
    HhyTokenList tokens = {0};
    HhyNode *program = NULL;
    if (hhy_lex(&source, &tokens)) {
        if (trace) { fputs("  parse\n", stderr); fflush(stderr); }
        HhyParseResult parsed = hhy_parse(&source, &tokens, &program);
        if (parsed.ok) {
            if (trace) { fputs("  bytecode compile\n", stderr); fflush(stderr); }
            HhyBytecodeChunk chunk;
            hhy_bytecode_chunk_init(&chunk);
            HhyBytecodeResult compiled = hhy_bytecode_compile(program, &chunk);
            if (compiled.ok) (void)hhy_bytecode_verify(&chunk);
            hhy_bytecode_chunk_free(&chunk);
        }
    }
    hhy_node_free(program); hhy_tokens_free(&tokens); free(text);

    if (trace) { fputs("  bytecode verify\n", stderr); fflush(stderr); }
    size_t instruction_count = size / 5;
    if (instruction_count > 256) instruction_count = 256;
    HhyBytecodeChunk raw;
    hhy_bytecode_chunk_init(&raw);
    if (instruction_count > 0) {
        raw.code = hhy_alloc(instruction_count * sizeof(*raw.code));
        raw.count = raw.capacity = instruction_count;
        for (size_t i = 0; i < instruction_count; i++) {
            size_t offset = i * 5;
            raw.code[i] = (HhyInstruction){
                .opcode = (HhyOpcode)data[offset],
                .constant = data[offset + 1] == 0xff ? HHY_BYTECODE_NO_CONSTANT : data[offset + 1],
                .child_count = data[offset + 2],
                .line = data[offset + 3],
                .column = data[offset + 4]
            };
        }
    }
    (void)hhy_bytecode_verify(&raw);
    hhy_bytecode_chunk_free(&raw);

    if (size >= 8) {
        HhyBytecodeChunk kernel_raw;
        hhy_bytecode_chunk_init(&kernel_raw);
        kernel_raw.code = hhy_alloc(3 * sizeof(*kernel_raw.code));
        kernel_raw.count = kernel_raw.capacity = 3;
        kernel_raw.code[0] = (HhyInstruction){
            .opcode = HHY_OP_PROGRAM, .token_kind = HHY_T_EOF,
            .constant = HHY_BYTECODE_NO_CONSTANT, .child_count = 1,
            .subtree_size = 2, .line = 1, .column = 1
        };
        kernel_raw.code[1] = (HhyInstruction){
            .opcode = HHY_OP_CLOSURE, .token_kind = HHY_T_PIPE,
            .constant = HHY_BYTECODE_NO_CONSTANT, .subtree_size = 1,
            .line = 1, .column = 1
        };
        kernel_raw.code[2] = (HhyInstruction){
            .opcode = HHY_OP_HALT, .token_kind = HHY_T_EOF,
            .constant = HHY_BYTECODE_NO_CONSTANT, .subtree_size = 1,
            .line = 1, .column = 1
        };
        kernel_raw.stream_kernels = hhy_alloc(sizeof(*kernel_raw.stream_kernels));
        kernel_raw.stream_kernel_count = kernel_raw.stream_kernel_capacity = 1;
        HhyStreamKernel *kernel = &kernel_raw.stream_kernels[0];
        memset(kernel, 0, sizeof(*kernel));
        kernel->version = data[0];
        kernel->source_instruction = data[1];
        kernel->instruction_count = data[2] % (HHY_STREAM_KERNEL_MAX_INSTRUCTIONS + 2);
        kernel->max_stack = data[3];
        kernel->result = (HhyStreamKernelResult)data[4];
        for (uint32_t i = 0; i < kernel->instruction_count &&
                             i < HHY_STREAM_KERNEL_MAX_INSTRUCTIONS; i++) {
            kernel->instructions[i].opcode =
                (HhyStreamKernelOpcode)data[(5 + i) % size];
            kernel->instructions[i].immediate = (int8_t)data[(6 + i) % size];
        }
        (void)hhy_bytecode_verify(&kernel_raw);
        hhy_bytecode_chunk_free(&kernel_raw);
    }

    unsigned mode = size == 0 ? 0 : data[0] % 3;
    if (trace) { fprintf(stderr, "  runtime %u\n", mode); fflush(stderr); }
    hhy_fuzz_runtime_input(size == 0 ? data : data + 1, size == 0 ? 0 : size - 1, mode);
    if (trace) { fputs("  done\n", stderr); fflush(stderr); }
    hhy_suppress_diagnostics = previous_suppression;
    return 0;
}

#ifdef HHY_STANDALONE_FUZZ
int main(int argc, char **argv) {
    size_t iterations = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 5000;
    uint64_t state = 0x48585950495045ULL;
    hhy_suppress_diagnostics = true;
    for (size_t iteration = 0; iteration < iterations; iteration++) {
        if (getenv("HHY_FUZZ_TRACE") != NULL) {
            fprintf(stderr, "fuzz input %zu\n", iteration);
            fflush(stderr);
        }
        state ^= state << 13; state ^= state >> 7; state ^= state << 17;
        size_t length = (size_t)(state % 4097);
        uint8_t *input = hhy_alloc(length == 0 ? 1 : length);
        for (size_t i = 0; i < length; i++) {
            state ^= state << 13; state ^= state >> 7; state ^= state << 17;
            input[i] = (uint8_t)state;
        }
        LLVMFuzzerTestOneInput(input, length); free(input);
    }
    hhy_suppress_diagnostics = false;
    printf("fuzz smoke completed: %zu inputs\n", iterations);
    return 0;
}
#endif

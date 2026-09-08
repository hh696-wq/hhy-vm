#include "hhy/bytecode.h"
#include "hhy/parser.h"
#include <stdio.h>
#include <stdlib.h>

static void require(bool ok, const char *message) {
    if (!ok) { fprintf(stderr, "exception table: %s\n", message); exit(1); }
}
int main(void) {
    HhySource source = {0}; HhyTokenList tokens = {0}; HhyNode *program = NULL;
    require(hhy_source_load("tests/valid/exception-regions.hhy", &source), "load");
    require(hhy_lex(&source, &tokens), "lex");
    require(hhy_parse(&source, &tokens, &program).ok, "parse");
    HhyBytecodeChunk chunk; hhy_bytecode_chunk_init(&chunk);
    require(hhy_bytecode_compile(program, &chunk).ok, "compile");
    require(chunk.exception_region_count >= 8, "missing regions");
    bool nested = false, closure = false, top = false, attempt = false;
    for (size_t i = 0; i < chunk.exception_region_count; i++) {
        HhyBytecodeExceptionRegion saved = chunk.exception_regions[i];
        require(hhy_bytecode_exception_region(&chunk, saved.source_instruction) == &chunk.exception_regions[i], "lookup");
        uint32_t expected_parent = HHY_BYTECODE_NO_INSTRUCTION;
        uint32_t closest_begin = 0;
        for (size_t j = 0; j < chunk.exception_region_count; j++) {
            HhyBytecodeExceptionRegion outer = chunk.exception_regions[j];
            if (outer.owner_instruction == saved.owner_instruction &&
                outer.protected_begin <= saved.source_instruction &&
                saved.source_instruction < outer.protected_end &&
                outer.protected_begin >= closest_begin) {
                closest_begin = outer.protected_begin;
                expected_parent = outer.source_instruction;
            }
        }
        require(saved.parent_source == expected_parent, "parent differs from innermost same-function protection");
        if (saved.parent_source != HHY_BYTECODE_NO_INSTRUCTION) nested = true;
        if (chunk.code[saved.owner_instruction].opcode == HHY_OP_CLOSURE) {
            closure = true;
            require(saved.parent_source == HHY_BYTECODE_NO_INSTRUCTION, "closure inherited defining TRY");
        }
        if (saved.owner_instruction == 0) top = true;
        if (chunk.code[saved.source_instruction].opcode == HHY_OP_ATTEMPT) attempt = true;
#define CORRUPT(field) do { chunk.exception_regions[i].field ^= 1u; \
    require(!hhy_bytecode_verify(&chunk).ok, "accepted corrupt " #field); \
    chunk.exception_regions[i] = saved; } while (0)
        CORRUPT(version); CORRUPT(source_instruction); CORRUPT(owner_instruction);
        CORRUPT(parent_source); CORRUPT(protected_begin); CORRUPT(protected_end);
        CORRUPT(catch_binding); CORRUPT(handler_begin); CORRUPT(handler_end);
#undef CORRUPT
    }
    require(nested && closure && top && attempt, "scope corpus incomplete");
    require(hhy_bytecode_exception_region(&chunk, chunk.count) == NULL, "out of range lookup");
    chunk.exception_region_count--;
    require(!hhy_bytecode_verify(&chunk).ok, "accepted missing entry");
    chunk.exception_region_count++;
    HhyBytecodeExceptionRegion *storage = chunk.exception_regions;
    chunk.exception_regions = NULL;
    require(!hhy_bytecode_verify(&chunk).ok, "accepted null storage");
    chunk.exception_regions = storage;
    HhyBytecodeExceptionRegion saved = storage[1]; storage[1] = storage[0];
    require(!hhy_bytecode_verify(&chunk).ok, "accepted duplicate entry"); storage[1] = saved;
    require(hhy_bytecode_verify(&chunk).ok, "restored table rejected");
    hhy_bytecode_chunk_free(&chunk); hhy_node_free(program);
    hhy_tokens_free(&tokens); hhy_source_free(&source);
    puts("exception table ownership, ranges and mutation tests passed");
    return 0;
}

#ifndef HHY_BYTECODE_H
#define HHY_BYTECODE_H

#include "hhy/ast.h"

#include <stdio.h>

#define HHY_BYTECODE_NO_CONSTANT UINT32_MAX
#define HHY_BYTECODE_MAX_INSTRUCTIONS 1000000u
#define HHY_BYTECODE_MAX_CONSTANTS 262144u
#define HHY_BYTECODE_MAX_NESTING 4096u

typedef enum {
    HHY_OP_PROGRAM,
    HHY_OP_LET_DECL,
    HHY_OP_FN_DECL,
    HHY_OP_IMPORT_DECL,
    HHY_OP_EXPORT_DECL,
    HHY_OP_BLOCK,
    HHY_OP_IF,
    HHY_OP_FOR,
    HHY_OP_WHILE,
    HHY_OP_TRY,
    HHY_OP_RETURN,
    HHY_OP_BREAK,
    HHY_OP_CONTINUE,
    HHY_OP_EXPR_STMT,
    HHY_OP_ASSIGN,
    HHY_OP_PIPE,
    HHY_OP_BINARY,
    HHY_OP_UNARY,
    HHY_OP_RANGE,
    HHY_OP_CALL,
    HHY_OP_MEMBER,
    HHY_OP_INDEX,
    HHY_OP_CLOSURE,
    HHY_OP_ATTEMPT,
    HHY_OP_LIST,
    HHY_OP_MAP,
    HHY_OP_MAP_ENTRY,
    HHY_OP_IDENTIFIER,
    HHY_OP_LITERAL,
    HHY_OP_HALT,
    HHY_OP_COUNT
} HhyOpcode;

typedef struct {
    HhyOpcode opcode;
    HhyTokenKind token_kind;
    uint32_t constant;
    uint32_t token_length;
    uint32_t child_count;
    uint32_t subtree_size;
    uint32_t frame_slot_count;
    uint32_t local_env_depth;
    uint32_t local_binding_slot;
    bool local_slot_resolved;
    uint32_t line;
    uint32_t column;
} HhyInstruction;

typedef struct {
    HhyInstruction *code;
    size_t count;
    size_t capacity;
    char **constants;
    size_t constant_count;
    size_t constant_capacity;
} HhyBytecodeChunk;

typedef struct {
    bool ok;
    size_t instruction;
    char message[160];
} HhyBytecodeResult;

typedef struct {
    size_t instruction_pointer;
    size_t operand_count;
    size_t max_operand_count;
    size_t frame_count;
    size_t max_frame_count;
} HhyBytecodeExecutionPlan;

void hhy_bytecode_chunk_init(HhyBytecodeChunk *chunk);
void hhy_bytecode_chunk_free(HhyBytecodeChunk *chunk);
HhyBytecodeResult hhy_bytecode_compile(const HhyNode *program, HhyBytecodeChunk *chunk);
HhyBytecodeResult hhy_bytecode_verify(const HhyBytecodeChunk *chunk);
HhyBytecodeResult hhy_bytecode_prepare_execution(const HhyBytecodeChunk *chunk,
                                                 size_t frame_limit,
                                                 HhyBytecodeExecutionPlan *plan);
void hhy_bytecode_disassemble(const HhyBytecodeChunk *chunk, FILE *output);
const char *hhy_opcode_name(HhyOpcode opcode);
bool hhy_bytecode_child(const HhyBytecodeChunk *chunk, size_t parent,
                        uint32_t child_index, size_t *child);

#endif

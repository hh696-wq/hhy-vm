#include "hhy/bytecode.h"
#include "hhy/common.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(HHY_N_LITERAL + 1 == HHY_OP_HALT,
               "AST node kinds and structural Bytecode opcodes must stay aligned");

static HhyBytecodeResult result(bool ok, size_t instruction, const char *format, ...) {
    HhyBytecodeResult value = {.ok = ok, .instruction = instruction, .message = ""};
    if (format != NULL) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(value.message, sizeof(value.message), format, arguments);
        va_end(arguments);
    }
    return value;
}

void hhy_bytecode_chunk_init(HhyBytecodeChunk *chunk) {
    memset(chunk, 0, sizeof(*chunk));
}

void hhy_bytecode_chunk_free(HhyBytecodeChunk *chunk) {
    if (chunk == NULL) return;
    for (size_t i = 0; i < chunk->constant_count; i++) free(chunk->constants[i]);
    free(chunk->constants);
    free(chunk->code);
    hhy_bytecode_chunk_init(chunk);
}

const char *hhy_opcode_name(HhyOpcode opcode) {
    static const char *names[] = {
        "PROGRAM", "LET_DECL", "FN_DECL", "IMPORT_DECL", "EXPORT_DECL",
        "BLOCK", "IF", "FOR", "WHILE", "TRY", "RETURN", "BREAK",
        "CONTINUE", "EXPR_STMT", "ASSIGN", "PIPE", "BINARY", "UNARY",
        "RANGE", "CALL", "MEMBER", "INDEX", "CLOSURE", "ATTEMPT", "LIST",
        "MAP", "MAP_ENTRY", "IDENTIFIER", "LITERAL", "HALT"
    };
    return opcode >= 0 && opcode < HHY_OP_COUNT ? names[(size_t)opcode] : "INVALID";
}

static HhyOpcode opcode_for_node(HhyNodeKind kind) {
    return kind >= HHY_N_PROGRAM && kind <= HHY_N_LITERAL
        ? (HhyOpcode)((int)HHY_OP_PROGRAM + (int)kind)
        : HHY_OP_COUNT;
}

static bool reserve_code(HhyBytecodeChunk *chunk) {
    if (chunk->count >= HHY_BYTECODE_MAX_INSTRUCTIONS) return false;
    if (chunk->count < chunk->capacity) return true;
    size_t capacity = chunk->capacity < 64 ? 64 : chunk->capacity * 2;
    if (capacity > HHY_BYTECODE_MAX_INSTRUCTIONS) capacity = HHY_BYTECODE_MAX_INSTRUCTIONS;
    chunk->code = hhy_realloc(chunk->code, capacity * sizeof(*chunk->code));
    chunk->capacity = capacity;
    return true;
}

static bool add_constant(HhyBytecodeChunk *chunk, const HhyToken *token, uint32_t *index) {
    if (token->length == 0) { *index = HHY_BYTECODE_NO_CONSTANT; return true; }
    for (size_t i = 0; i < chunk->constant_count; i++) {
        if (strlen(chunk->constants[i]) == token->length &&
            memcmp(chunk->constants[i], token->start, token->length) == 0) {
            *index = (uint32_t)i;
            return true;
        }
    }
    if (chunk->constant_count >= HHY_BYTECODE_MAX_CONSTANTS) return false;
    if (chunk->constant_count == chunk->constant_capacity) {
        size_t capacity = chunk->constant_capacity < 32 ? 32 : chunk->constant_capacity * 2;
        if (capacity > HHY_BYTECODE_MAX_CONSTANTS) capacity = HHY_BYTECODE_MAX_CONSTANTS;
        chunk->constants = hhy_realloc(chunk->constants, capacity * sizeof(*chunk->constants));
        chunk->constant_capacity = capacity;
    }
    chunk->constants[chunk->constant_count] = hhy_strndup(token->start, token->length);
    *index = (uint32_t)chunk->constant_count++;
    return true;
}

static HhyBytecodeResult compile_node(const HhyNode *node, HhyBytecodeChunk *chunk,
                                      size_t depth) {
    if (node == NULL) return result(false, chunk->count, "compiler received a null AST node");
    if (depth > HHY_BYTECODE_MAX_NESTING)
        return result(false, chunk->count, "AST nesting exceeds %u", HHY_BYTECODE_MAX_NESTING);
    if (node->child_count > UINT32_MAX)
        return result(false, chunk->count, "node has too many children");
    HhyOpcode opcode = opcode_for_node(node->kind);
    if (opcode == HHY_OP_COUNT)
        return result(false, chunk->count, "unsupported AST node kind %d", (int)node->kind);
    uint32_t constant;
    if (!add_constant(chunk, &node->token, &constant))
        return result(false, chunk->count, "constant pool exceeds %u", HHY_BYTECODE_MAX_CONSTANTS);
    if (!reserve_code(chunk))
        return result(false, chunk->count, "instruction count exceeds %u", HHY_BYTECODE_MAX_INSTRUCTIONS);
    chunk->code[chunk->count++] = (HhyInstruction){
        .opcode = opcode,
        .constant = constant,
        .child_count = (uint32_t)node->child_count,
        .line = node->token.line,
        .column = node->token.column
    };
    for (size_t i = 0; i < node->child_count; i++) {
        HhyBytecodeResult child = compile_node(node->children[i], chunk, depth + 1);
        if (!child.ok) return child;
    }
    return result(true, chunk->count, NULL);
}

HhyBytecodeResult hhy_bytecode_compile(const HhyNode *program, HhyBytecodeChunk *chunk) {
    if (chunk == NULL) return result(false, 0, "compiler received a null chunk");
    hhy_bytecode_chunk_free(chunk);
    HhyBytecodeResult compiled = compile_node(program, chunk, 0);
    if (!compiled.ok) return compiled;
    if (!reserve_code(chunk))
        return result(false, chunk->count, "instruction count exceeds %u", HHY_BYTECODE_MAX_INSTRUCTIONS);
    uint32_t line = program == NULL ? 0 : program->token.line;
    uint32_t column = program == NULL ? 0 : program->token.column;
    chunk->code[chunk->count++] = (HhyInstruction){
        .opcode = HHY_OP_HALT, .constant = HHY_BYTECODE_NO_CONSTANT,
        .child_count = 0, .line = line, .column = column
    };
    return hhy_bytecode_verify(chunk);
}

static HhyBytecodeResult verify_node(const HhyBytecodeChunk *chunk, size_t *cursor,
                                     size_t depth) {
    if (depth > HHY_BYTECODE_MAX_NESTING)
        return result(false, *cursor, "bytecode nesting exceeds %u", HHY_BYTECODE_MAX_NESTING);
    if (*cursor >= chunk->count) return result(false, *cursor, "truncated node instruction");
    size_t current = (*cursor)++;
    HhyInstruction instruction = chunk->code[current];
    if (instruction.opcode < HHY_OP_PROGRAM || instruction.opcode > HHY_OP_LITERAL)
        return result(false, current, "invalid node opcode %d", (int)instruction.opcode);
    if (instruction.constant != HHY_BYTECODE_NO_CONSTANT &&
        instruction.constant >= chunk->constant_count)
        return result(false, current, "constant index %u is out of range", instruction.constant);
    if ((instruction.line == 0) != (instruction.column == 0))
        return result(false, current, "incomplete source location");
    for (uint32_t i = 0; i < instruction.child_count; i++) {
        HhyBytecodeResult child = verify_node(chunk, cursor, depth + 1);
        if (!child.ok) return child;
    }
    return result(true, current, NULL);
}

HhyBytecodeResult hhy_bytecode_verify(const HhyBytecodeChunk *chunk) {
    if (chunk == NULL) return result(false, 0, "verifier received a null chunk");
    if (chunk->count == 0) return result(false, 0, "chunk is empty");
    if (chunk->count > HHY_BYTECODE_MAX_INSTRUCTIONS)
        return result(false, 0, "instruction count exceeds %u", HHY_BYTECODE_MAX_INSTRUCTIONS);
    if (chunk->constant_count > HHY_BYTECODE_MAX_CONSTANTS)
        return result(false, 0, "constant count exceeds %u", HHY_BYTECODE_MAX_CONSTANTS);
    if (chunk->code == NULL) return result(false, 0, "chunk has no instruction storage");
    if (chunk->constant_count > 0 && chunk->constants == NULL)
        return result(false, 0, "chunk has no constant storage");
    for (size_t i = 0; i < chunk->constant_count; i++)
        if (chunk->constants[i] == NULL) return result(false, 0, "constant %zu is null", i);
    size_t cursor = 0;
    HhyBytecodeResult root = verify_node(chunk, &cursor, 0);
    if (!root.ok) return root;
    if (chunk->code[0].opcode != HHY_OP_PROGRAM)
        return result(false, 0, "root instruction must be PROGRAM");
    if (cursor >= chunk->count) return result(false, cursor, "chunk is missing HALT");
    HhyInstruction halt = chunk->code[cursor];
    if (halt.opcode != HHY_OP_HALT || halt.constant != HHY_BYTECODE_NO_CONSTANT ||
        halt.child_count != 0)
        return result(false, cursor, "root must be followed by a canonical HALT");
    if (cursor + 1 != chunk->count)
        return result(false, cursor + 1, "instructions follow HALT");
    return result(true, cursor, NULL);
}

static void print_constant(FILE *output, const char *text) {
    fputc('"', output);
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor; cursor++) {
        if (*cursor == '"' || *cursor == '\\') { fputc('\\', output); fputc(*cursor, output); }
        else if (*cursor == '\n') fputs("\\n", output);
        else if (*cursor == '\r') fputs("\\r", output);
        else if (*cursor == '\t') fputs("\\t", output);
        else if (*cursor >= 0x20) fputc(*cursor, output);
    }
    fputc('"', output);
}

void hhy_bytecode_disassemble(const HhyBytecodeChunk *chunk, FILE *output) {
    fprintf(output, "HHY Bytecode v1 (experimental)\nconstants %zu\ninstructions %zu\n",
            chunk->constant_count, chunk->count);
    for (size_t i = 0; i < chunk->count; i++) {
        HhyInstruction instruction = chunk->code[i];
        fprintf(output, "%04zu %-13s children=%u", i, hhy_opcode_name(instruction.opcode),
                instruction.child_count);
        if (instruction.constant != HHY_BYTECODE_NO_CONSTANT &&
            instruction.constant < chunk->constant_count) {
            fprintf(output, " constant=%u ", instruction.constant);
            print_constant(output, chunk->constants[instruction.constant]);
        }
        fprintf(output, " @%u:%u\n", instruction.line, instruction.column);
    }
}

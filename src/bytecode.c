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

bool hhy_bytecode_child(const HhyBytecodeChunk *chunk, size_t parent,
                        uint32_t child_index, size_t *child) {
    if (chunk == NULL || child == NULL || parent >= chunk->count ||
        child_index >= chunk->code[parent].child_count) return false;
    size_t cursor = parent + 1;
    for (uint32_t index = 0; index < child_index; index++) {
        if (cursor >= chunk->count || chunk->code[cursor].subtree_size == 0 ||
            chunk->code[cursor].subtree_size > chunk->count - cursor) return false;
        cursor += chunk->code[cursor].subtree_size;
    }
    if (cursor >= chunk->count ||
        cursor + chunk->code[cursor].subtree_size > parent + chunk->code[parent].subtree_size)
        return false;
    *child = cursor;
    return true;
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
    if (node->frame_slot_count > UINT32_MAX || node->local_env_depth > UINT32_MAX ||
        node->local_binding_slot > UINT32_MAX || node->token.length > UINT32_MAX)
        return result(false, chunk->count, "resolved slot metadata exceeds Bytecode limits");
    size_t instruction_index = chunk->count;
    chunk->code[chunk->count++] = (HhyInstruction){
        .opcode = opcode,
        .token_kind = node->token.kind,
        .constant = constant,
        .token_length = (uint32_t)node->token.length,
        .child_count = (uint32_t)node->child_count,
        .subtree_size = 0,
        .frame_slot_count = (uint32_t)node->frame_slot_count,
        .local_env_depth = (uint32_t)node->local_env_depth,
        .local_binding_slot = (uint32_t)node->local_binding_slot,
        .local_slot_resolved = node->local_slot_resolved,
        .line = node->token.line,
        .column = node->token.column
    };
    for (size_t i = 0; i < node->child_count; i++) {
        HhyBytecodeResult child = compile_node(node->children[i], chunk, depth + 1);
        if (!child.ok) return child;
    }
    size_t subtree_size = chunk->count - instruction_index;
    if (subtree_size > UINT32_MAX)
        return result(false, instruction_index, "Bytecode subtree exceeds addressable range");
    chunk->code[instruction_index].subtree_size = (uint32_t)subtree_size;
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
        .opcode = HHY_OP_HALT, .token_kind = HHY_T_EOF,
        .constant = HHY_BYTECODE_NO_CONSTANT, .child_count = 0,
        .subtree_size = 1, .line = line, .column = column
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
    if (instruction.token_kind < HHY_T_EOF || instruction.token_kind > HHY_T_NOT)
        return result(false, current, "invalid token kind %d", (int)instruction.token_kind);
    if (instruction.subtree_size == 0 ||
        instruction.subtree_size > chunk->count - current)
        return result(false, current, "invalid subtree size %u", instruction.subtree_size);
    if (instruction.constant != HHY_BYTECODE_NO_CONSTANT &&
        instruction.constant >= chunk->constant_count)
        return result(false, current, "constant index %u is out of range", instruction.constant);
    if ((instruction.line == 0) != (instruction.column == 0))
        return result(false, current, "incomplete source location");
    for (uint32_t i = 0; i < instruction.child_count; i++) {
        HhyBytecodeResult child = verify_node(chunk, cursor, depth + 1);
        if (!child.ok) return child;
    }
    if (*cursor - current != instruction.subtree_size)
        return result(false, current, "subtree size does not match encoded children");
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
    if (halt.opcode != HHY_OP_HALT || halt.token_kind != HHY_T_EOF ||
        halt.constant != HHY_BYTECODE_NO_CONSTANT || halt.child_count != 0 ||
        halt.subtree_size != 1)
        return result(false, cursor, "root must be followed by a canonical HALT");
    if (cursor + 1 != chunk->count)
        return result(false, cursor + 1, "instructions follow HALT");
    return result(true, cursor, NULL);
}

typedef struct {
    size_t remaining;
} ExecutionFrame;

HhyBytecodeResult hhy_bytecode_prepare_execution(const HhyBytecodeChunk *chunk,
                                                 size_t frame_limit,
                                                 HhyBytecodeExecutionPlan *plan) {
    if (plan == NULL) return result(false, 0, "execution planner received a null plan");
    memset(plan, 0, sizeof(*plan));
    HhyBytecodeResult verified = hhy_bytecode_verify(chunk);
    if (!verified.ok) return verified;
    if (frame_limit == 0) return result(false, 0, "Bytecode frame limit must be positive");

    ExecutionFrame *frames = hhy_alloc(frame_limit * sizeof(*frames));
    size_t frame_count = 0;
    size_t operands = 0;
    size_t cursor = 0;
    while (cursor + 1 < chunk->count) {
        while (frame_count > 0 && frames[frame_count - 1].remaining == 0) {
            frame_count--;
            if (operands > 0) operands--;
        }
        if (frame_count >= frame_limit) {
            free(frames);
            return result(false, cursor, "Bytecode frame limit %zu exceeded", frame_limit);
        }
        if (frame_count > 0) frames[frame_count - 1].remaining--;
        HhyInstruction instruction = chunk->code[cursor++];
        operands++;
        if (operands > plan->max_operand_count) plan->max_operand_count = operands;
        frames[frame_count++] = (ExecutionFrame){.remaining = instruction.child_count};
        if (frame_count > plan->max_frame_count) plan->max_frame_count = frame_count;
    }
    while (frame_count > 0 && frames[frame_count - 1].remaining == 0) {
        frame_count--;
        if (operands > 0) operands--;
    }
    free(frames);
    if (frame_count != 0 || operands != 0)
        return result(false, cursor, "Bytecode execution shape did not converge");
    plan->instruction_pointer = cursor;
    plan->operand_count = operands;
    plan->frame_count = frame_count;
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
        fprintf(output, "%04zu %-13s children=%u subtree=%u", i,
                hhy_opcode_name(instruction.opcode), instruction.child_count,
                instruction.subtree_size);
        if (instruction.constant != HHY_BYTECODE_NO_CONSTANT &&
            instruction.constant < chunk->constant_count) {
            fprintf(output, " constant=%u ", instruction.constant);
            print_constant(output, chunk->constants[instruction.constant]);
        }
        fprintf(output, " @%u:%u\n", instruction.line, instruction.column);
    }
}

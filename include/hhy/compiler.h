#ifndef HHY_COMPILER_H
#define HHY_COMPILER_H

#include "hhy/bytecode.h"

/* Structured, non-SSA IR. Expression trees are ordered regions; CFG blocks
 * describe statement transfers, including callable entries and exceptional exits. */
#define HHY_COMPILER_IR_VERSION 1u
#define HHY_COMPILER_NONE UINT32_MAX
#define HHY_COMPILER_CONST (HHY_N_LITERAL + 1)
#define HHY_COMPILER_PASS_COUNT 6u

enum { HHY_C_THROW = 1, HHY_C_CANCEL = 2, HHY_C_ALLOC = 4, HHY_C_EFFECT = 8 };
typedef enum { HHY_C_DYNAMIC, HHY_C_INT, HHY_C_BOOL, HHY_C_NULL } HhyCompilerType;
typedef struct {
    unsigned op;
    HhyToken token;
    uint32_t parent, first_child, next, child_count;
    uint32_t origin; /* Dominating immutable LET for propagated constants. */
    uint32_t effects;
    HhyCompilerType type;
    int64_t value;
    size_t frame_slots, local_depth, local_slot;
    bool resolved;
} HhyCompilerNode;
typedef struct {
    bool present;
    uint32_t node, owner, phase, next, alternate, exception;
} HhyCompilerBlock;
typedef struct {
    uint32_t version;
    size_t count, capacity;
    HhyCompilerNode *nodes;
    /* Two slots per node: normal entry and synthetic FOR advance block. */
    HhyCompilerBlock *blocks;
} HhyCompilerIR;
typedef struct {
    bool enabled[HHY_COMPILER_PASS_COUNT];
} HhyCompilerOptions;
typedef struct {
    size_t before_nodes, after_nodes, ir_bytes, blocks;
    bool enabled[HHY_COMPILER_PASS_COUNT];
    size_t changed[HHY_COMPILER_PASS_COUNT];
    uint64_t pass_ns[HHY_COMPILER_PASS_COUNT], lower_ns, emit_ns;
    size_t calls, aggregates, closures, rejected_arithmetic;
    size_t local_aggregates, escaping_aggregates, captured_closures;
    size_t baseline_bytecode_bytes, optimized_bytecode_bytes;
    bool size_fallback;
} HhyCompilerReport;

extern const char *const hhy_compiler_pass_names[HHY_COMPILER_PASS_COUNT];
void hhy_compiler_ir_free(HhyCompilerIR *ir);
HhyBytecodeResult hhy_compiler_lower(const HhyNode *program, HhyCompilerIR *out);
HhyBytecodeResult hhy_compiler_verify(const HhyCompilerIR *ir);
HhyBytecodeResult hhy_compiler_optimize(HhyCompilerIR *ir,
    HhyCompilerOptions options, HhyCompilerReport *report);
HhyBytecodeResult hhy_compiler_emit(const HhyCompilerIR *ir, HhyBytecodeChunk *chunk);
HhyBytecodeResult hhy_compiler_compile(const HhyNode *program,
    HhyBytecodeChunk *chunk, HhyCompilerOptions options, HhyCompilerReport *report);
HhyCompilerOptions hhy_compiler_options(void);
void hhy_compiler_dump(const HhyCompilerIR *ir, FILE *file);
void hhy_compiler_dump_json(const HhyCompilerIR *ir, FILE *file);
void hhy_compiler_report(const HhyCompilerReport *report, FILE *file);
#endif

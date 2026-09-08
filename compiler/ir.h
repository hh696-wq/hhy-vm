#ifndef HHY_EXPERIMENTAL_IR_H
#define HHY_EXPERIMENTAL_IR_H
#include "hhy/ast.h"
#define HHY_IR_VERSION 1u
#define HHY_IR_LIMIT 256u
#define HHY_IR_NONE UINT32_MAX
/* Prototype: one I64 basic block, no calls, bindings, phi or external effects. */
typedef enum { IR_CONST, IR_ADD, IR_SUB, IR_MUL, IR_MOD, IR_NEG, IR_RETURN, IR_OP_COUNT } HhyIROp;
typedef struct {
    HhyIROp op;
    uint32_t lhs, rhs, line, column, length;
    int64_t immediate;
    bool may_throw, may_cancel, allocates, external_effect;
} HhyIRInstruction;
typedef struct {
    uint32_t version, blocks, entry, count;
    HhyIRInstruction code[HHY_IR_LIMIT];
} HhyIR;
typedef struct { bool ok; int64_t value; uint32_t instruction; const char *reason; } HhyIRResult;
bool hhy_ir_lower(const HhyNode *, HhyIR *);
bool hhy_ir_verify(const HhyIR *);
HhyIRResult hhy_ir_evaluate(const HhyIR *);
/* Transactional: malformed input is rejected without modifying destination. */
bool hhy_ir_fold(const HhyIR *, HhyIR *, bool enabled);
const char *hhy_ir_op_name(HhyIROp);
#endif

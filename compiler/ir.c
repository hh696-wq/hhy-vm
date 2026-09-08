#include "ir.h"
#include <limits.h>
#include <string.h>
const char *hhy_ir_op_name(HhyIROp op) {
    static const char *names[]={"CONST_I64","ADD_CHECKED","SUB_CHECKED","MUL_CHECKED","MOD_CHECKED","NEG_CHECKED","RETURN"};
    return op>=0 && op<IR_OP_COUNT?names[op]:"INVALID";
}
bool hhy_ir_verify(const HhyIR *ir) {
    if (!ir || ir->version!=HHY_IR_VERSION || ir->blocks!=1 || ir->entry!=0 || ir->count<2 || ir->count>HHY_IR_LIMIT) return false;
    for (uint32_t i=0;i<ir->count;i++) {
        const HhyIRInstruction *x=&ir->code[i];
        if (x->op<0 || x->op>=IR_OP_COUNT || !x->line || !x->column || !x->length ||
            x->may_cancel || x->allocates || x->external_effect) return false;
        if (x->may_throw!=(x->op>=IR_ADD && x->op<=IR_NEG)) return false;
        if ((x->op==IR_RETURN)!=(i==ir->count-1)) return false;
        if (x->op==IR_CONST) {
            if (x->lhs!=HHY_IR_NONE || x->rhs!=HHY_IR_NONE) return false;
        } else {
            if (x->immediate!=0 || x->lhs>=i) return false;
            if (x->op==IR_NEG || x->op==IR_RETURN) { if (x->rhs!=HHY_IR_NONE) return false; }
            else if (x->rhs>=i) return false;
        }
    }
    return true;
}
static bool lower(const HhyNode *node,HhyIR *ir,unsigned depth,uint32_t *out) {
    if (!node || depth>=HHY_IR_LIMIT || ir->count>=HHY_IR_LIMIT-1 || node->token.length>UINT32_MAX) return false;
    HhyIRInstruction x={.lhs=HHY_IR_NONE,.rhs=HHY_IR_NONE,.line=node->token.line,.column=node->token.column,.length=(uint32_t)node->token.length};
    if (node->kind==HHY_N_LITERAL && node->token.kind==HHY_T_INT && node->child_count==0) {
        if (!node->token.start || !node->token.length) return false;
        uint64_t value=0;
        for (size_t i=0;i<node->token.length;i++) {
            unsigned char c=(unsigned char)node->token.start[i];
            if (c<'0' || c>'9' || value>((uint64_t)INT64_MAX-(c-'0'))/10) return false;
            value=value*10+(c-'0');
        }
        x.op=IR_CONST;x.immediate=(int64_t)value;
    } else if (node->kind==HHY_N_UNARY && node->token.kind==HHY_T_MINUS && node->child_count==1) {
        if (!lower(node->children[0],ir,depth+1,&x.lhs)) return false;
        x.op=IR_NEG;x.may_throw=true;
    } else if (node->kind==HHY_N_BINARY && node->child_count==2) {
        switch (node->token.kind) {
            case HHY_T_PLUS:x.op=IR_ADD;break;
            case HHY_T_MINUS:x.op=IR_SUB;break;
            case HHY_T_STAR:x.op=IR_MUL;break;
            case HHY_T_MOD:x.op=IR_MOD;break;
            default:return false;
        }
        if (!lower(node->children[0],ir,depth+1,&x.lhs) || !lower(node->children[1],ir,depth+1,&x.rhs)) return false;
        x.may_throw=true;
    } else return false;
    if (ir->count>=HHY_IR_LIMIT-1) return false;
    *out=ir->count;ir->code[ir->count++]=x;return true;
}
bool hhy_ir_lower(const HhyNode *node,HhyIR *out) {
    if (!out) return false;
    HhyIR ir={.version=HHY_IR_VERSION,.blocks=1};uint32_t value;
    if (!lower(node,&ir,0,&value)) return false;
    ir.code[ir.count++]=(HhyIRInstruction){.op=IR_RETURN,.lhs=value,.rhs=HHY_IR_NONE,.line=node->token.line,.column=node->token.column,.length=(uint32_t)node->token.length};
    if (!hhy_ir_verify(&ir)) return false;
    *out=ir;return true;
}
HhyIRResult hhy_ir_evaluate(const HhyIR *ir) {
    if (!hhy_ir_verify(ir)) return (HhyIRResult){.reason="invalid_ir"};
    int64_t values[HHY_IR_LIMIT]={0};
    for (uint32_t i=0;i<ir->count;i++) {
        const HhyIRInstruction *x=&ir->code[i];int64_t a=x->lhs==HHY_IR_NONE?0:values[x->lhs],b=x->rhs==HHY_IR_NONE?0:values[x->rhs];
        bool overflow=false;
        switch (x->op) {
            case IR_CONST:values[i]=x->immediate;break;
            case IR_ADD:overflow=__builtin_add_overflow(a,b,&values[i]);break;
            case IR_SUB:overflow=__builtin_sub_overflow(a,b,&values[i]);break;
            case IR_MUL:overflow=__builtin_mul_overflow(a,b,&values[i]);break;
            case IR_NEG:overflow=__builtin_sub_overflow((int64_t)0,a,&values[i]);break;
            case IR_MOD:
                if (!b) return (HhyIRResult){.instruction=i,.reason="division_by_zero"};
                values[i]=(a==INT64_MIN && b==-1)?0:a%b;break;
            case IR_RETURN:return (HhyIRResult){.ok=true,.value=a,.instruction=i,.reason="ok"};
            case IR_OP_COUNT:return (HhyIRResult){.reason="invalid_ir"};
        }
        if (overflow) return (HhyIRResult){.instruction=i,.reason=x->op==IR_NEG?"integer_negation_overflow":"integer_overflow"};
    }
    return (HhyIRResult){.reason="invalid_ir"};
}
bool hhy_ir_fold(const HhyIR *input,HhyIR *out,bool enabled) {
    if (!out || !hhy_ir_verify(input)) return false;
    HhyIR result;memcpy(&result,input,sizeof(result));
    if (enabled) {
        HhyIRResult value=hhy_ir_evaluate(input);
        if (value.ok) {
            HhyIRInstruction ret=input->code[input->count-1];
            memset(&result,0,sizeof(result));result.version=HHY_IR_VERSION;result.blocks=1;result.count=2;
            result.code[0]=(HhyIRInstruction){.op=IR_CONST,.lhs=HHY_IR_NONE,.rhs=HHY_IR_NONE,.immediate=value.value,.line=ret.line,.column=ret.column,.length=ret.length};
            ret.lhs=0;result.code[1]=ret;
        }
    }
    if (!hhy_ir_verify(&result)) return false;
    memcpy(out,&result,sizeof(result));return true;
}

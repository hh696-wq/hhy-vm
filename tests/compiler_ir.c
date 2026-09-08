#include "../compiler/ir.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static HhyIR valid(void) {
    HhyIR ir={.version=1,.blocks=1,.count=4};
    ir.code[0]=(HhyIRInstruction){.op=IR_CONST,.lhs=HHY_IR_NONE,.rhs=HHY_IR_NONE,.immediate=40,.line=1,.column=1,.length=2};
    ir.code[1]=ir.code[0];ir.code[1].immediate=2;
    ir.code[2]=(HhyIRInstruction){.op=IR_ADD,.lhs=0,.rhs=1,.line=1,.column=3,.length=1,.may_throw=true};
    ir.code[3]=(HhyIRInstruction){.op=IR_RETURN,.lhs=2,.rhs=HHY_IR_NONE,.line=1,.column=3,.length=1};
    return ir;
}
static void rejected(HhyIR *ir) {
    HhyIR output=valid(),snapshot=output;
    assert(!hhy_ir_verify(ir));assert(!hhy_ir_evaluate(ir).ok);
    assert(!hhy_ir_fold(ir,&output,true));assert(!memcmp(&output,&snapshot,sizeof(output)));
}
int main(void) {
    HhyIR good=valid(),folded,off;
    assert(hhy_ir_verify(&good));assert(hhy_ir_evaluate(&good).value==42);
    assert(hhy_ir_fold(&good,&folded,true));assert(folded.count==2 && hhy_ir_evaluate(&folded).value==42);
    assert(hhy_ir_fold(&good,&off,false));assert(!memcmp(&good,&off,sizeof(off)));
    for (unsigned mutation=0;mutation<16;mutation++) {
        HhyIR bad=good;
        switch(mutation) {
            case 0:bad.version++;break;case 1:bad.blocks=2;break;case 2:bad.entry=1;break;
            case 3:bad.count=HHY_IR_LIMIT+1;break;case 4:bad.count=1;break;
            case 5:bad.code[2].lhs=2;break;case 6:bad.code[2].rhs=3;break;
            case 7:bad.code[2].may_throw=false;break;case 8:bad.code[0].may_throw=true;break;
            case 9:bad.code[2].may_cancel=true;break;case 10:bad.code[2].allocates=true;break;
            case 11:bad.code[2].external_effect=true;break;case 12:bad.code[2].line=0;break;
            case 13:bad.code[3].op=IR_NEG;break;case 14:bad.code[0].op=(HhyIROp)-1;break;
            case 15:bad.code[2].immediate=9;break;
        }
        rejected(&bad);
    }
    good.code[0].immediate=INT64_MAX;assert(hhy_ir_fold(&good,&folded,true));assert(!memcmp(&good,&folded,sizeof(good)));
    assert(!hhy_ir_evaluate(&folded).ok && !strcmp(hhy_ir_evaluate(&folded).reason,"integer_overflow"));
    good=valid();good.code[2].op=IR_MOD;good.code[1].immediate=0;
    assert(hhy_ir_fold(&good,&folded,true));assert(!memcmp(&good,&folded,sizeof(good)));
    assert(!strcmp(hhy_ir_evaluate(&folded).reason,"division_by_zero"));
    uint32_t state=1700;
    for (unsigned n=0;n<10000;n++) {
        HhyIR mutated=valid();
        state^=state<<13;state^=state>>17;state^=state<<5;
        unsigned index=state%4;
        if (state&1) mutated.code[index].immediate=(int64_t)((uint64_t)state*UINT64_C(4294967291));
        else switch ((state>>1)%8) {
            case 0:mutated.count^=state;break;
            case 1:mutated.version^=state;break;
            case 2:mutated.code[index].op=(HhyIROp)(state%10);break;
            case 3:mutated.code[index].lhs^=state;break;
            case 4:mutated.code[index].rhs^=state;break;
            case 5:mutated.code[index].line=0;break;
            case 6:mutated.code[index].may_throw=!mutated.code[index].may_throw;break;
            case 7:mutated.code[index].external_effect=true;break;
        }
        if (!hhy_ir_verify(&mutated)) rejected(&mutated);
        else {
            HhyIRResult a=hhy_ir_evaluate(&mutated);
            assert(hhy_ir_fold(&mutated,&folded,true));
            HhyIRResult b=hhy_ir_evaluate(&folded);
            assert(a.ok==b.ok && a.value==b.value && !strcmp(a.reason,b.reason));
        }
    }
    puts("IR 10000 mutations and version/block/definitions/effects/source/transactional rejection checks passed");
}

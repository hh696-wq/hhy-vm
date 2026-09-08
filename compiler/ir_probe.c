#define _POSIX_C_SOURCE 200809L
#include "ir.h"
#include "hhy/parser.h"
#include <jansson.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static uint64_t now(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
static json_t *dump(const HhyIR *ir) {
    json_t *code=json_array();
    for (uint32_t i=0;i<ir->count;i++) {
        const HhyIRInstruction *x=&ir->code[i];
        json_array_append_new(code,json_pack("{s:i,s:s,s:I,s:I,s:I,s:i,s:i,s:i,s:b,s:b,s:b,s:b}","id",i,"op",hhy_ir_op_name(x->op),"lhs",(json_int_t)x->lhs,"rhs",(json_int_t)x->rhs,"immediate",(json_int_t)x->immediate,"line",x->line,"column",x->column,"length",x->length,"may_throw",x->may_throw,"may_cancel",x->may_cancel,"allocates",x->allocates,"external_effect",x->external_effect));
    }
    return json_pack("{s:i,s:i,s:i,s:o}","version",ir->version,"blocks",ir->blocks,"entry",ir->entry,"instructions",code);
}
int main(int argc,char **argv) {
    if (argc!=3 || (strcmp(argv[1],"--fold") && strcmp(argv[1],"--no-fold") && strcmp(argv[1],"--text"))) return 2;
    HhySource source={0};HhyTokenList tokens={0};HhyNode *program=NULL;
    if (!hhy_source_load(argv[2],&source)) return 2;
    if (!hhy_lex(&source,&tokens) || !hhy_parse(&source,&tokens,&program).ok) {hhy_node_free(program);hhy_tokens_free(&tokens);hhy_source_free(&source);return 2;}
    const HhyNode *expression=NULL;
    /* Deliberately accept only print(one_closed_expression), not arbitrary programs. */
    if (program->child_count==1 && program->children[0]->kind==HHY_N_EXPR_STMT) {
        const HhyNode *call=program->children[0]->children[0];
        if (call->kind==HHY_N_CALL && call->child_count==2 && call->children[0]->kind==HHY_N_IDENTIFIER && call->children[0]->token.length==5 && !memcmp(call->children[0]->token.start,"print",5)) expression=call->children[1];
    }
    HhyIR before,after;uint64_t started=now();bool supported=hhy_ir_lower(expression,&before);uint64_t lowered=now();
    bool verified=supported && hhy_ir_fold(&before,&after,strcmp(argv[1],"--no-fold")!=0);uint64_t finished=now();
    json_t *report=json_pack("{s:i,s:b,s:b,s:I,s:I}","schema_version",1,"supported",supported,"verified",verified,"lower_verify_ns",(json_int_t)(lowered-started),"fold_verify_ns",(json_int_t)(finished-lowered));
    json_object_set_new(report,"reserved_ir_bytes",json_integer(sizeof(HhyIR)));
    json_object_set_new(report,"instruction_bytes",json_integer(sizeof(HhyIRInstruction)));
    if (!supported) json_object_set_new(report,"refusal_reason",json_string("requires_print_of_closed_decimal_i64_expression_within_256_instructions"));
    if (verified) {
        HhyIRResult value=hhy_ir_evaluate(&after);
        json_object_set_new(report,"before",dump(&before));json_object_set_new(report,"after",dump(&after));
        json_object_set_new(report,"result",json_pack("{s:b,s:I,s:s,s:i}","ok",value.ok,"value",(json_int_t)value.value,"reason",value.reason,"instruction",value.instruction));
        if (!strcmp(argv[1],"--text")) {
            for (uint32_t i=0;i<after.count;i++) printf("b0 v%u = %s lhs=%u rhs=%u imm=%"PRId64" (source %u:%u)\n",i,hhy_ir_op_name(after.code[i].op),after.code[i].lhs,after.code[i].rhs,after.code[i].immediate,after.code[i].line,after.code[i].column);
        }
    }
    if (strcmp(argv[1],"--text")) {json_dumpf(report,stdout,JSON_INDENT(2));fputc('\n',stdout);}
    json_decref(report);hhy_node_free(program);hhy_tokens_free(&tokens);hhy_source_free(&source);
    return supported && !verified?1:0;
}

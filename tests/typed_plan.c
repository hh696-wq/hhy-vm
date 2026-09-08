#define _POSIX_C_SOURCE 200809L
#include "hhy/compiler.h"
#include "hhy/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void require(bool ok, const char *message) {
    if (!ok) { fprintf(stderr, "typed plan: %s\n", message); exit(1); }
}
static void reserve(void *context, size_t count) { *(size_t *)context += count; }
int main(void) {
    const char *text = "fn f(x) { return [x + 1, x * 2][0] }\n";
    HhySource source = {.path="<typed>",.text=(char *)text,.length=strlen(text)};
    HhyTokenList tokens={0}; HhyNode *program=NULL; HhyBytecodeChunk chunk={0};
    require(hhy_lex(&source,&tokens),"lex");
    require(hhy_parse(&source,&tokens,&program).ok,"parse");
    hhy_resolve_slots(program);
    setenv("HHY_FEEDBACK_SPECIALIZATION","1",1);
    require(hhy_bytecode_compile_direct(program,&chunk).ok,"compile");
    require(chunk.typed_plan_count==1,"plan missing");
    HhyTypedPlan *p=&chunk.typed_plans[0], original=*p;
    int64_t args[]={40}; size_t elements=0;
    HhyTypedResult r=hhy_typed_execute(p,args,reserve,&elements);
    require(r.ok && r.value==41 && elements==2,"scalar execution");
    args[0]=INT64_MAX;
    r=hhy_typed_execute(p,args,reserve,&elements);
    require(!r.ok && p->code[r.failure].op==HT_ADD && r.left==INT64_MAX && r.right==1,"deopt operands");
#define MUTATE(field) do {p->field ^= 1u; require(!hhy_bytecode_verify(&chunk).ok,"accepted " #field); *p=original;} while(0)
    MUTATE(version); MUTATE(owner); MUTATE(expression); MUTATE(parameters); MUTATE(count); MUTATE(result); MUTATE(boolean_result);
#undef MUTATE
    for(uint32_t i=0;i<p->count;i++) {
#define MUTATE(field) do {p->code[i].field ^= 1u; require(!hhy_bytecode_verify(&chunk).ok,"accepted instruction " #field); *p=original;} while(0)
        MUTATE(op); MUTATE(source); MUTATE(lhs); MUTATE(rhs); MUTATE(immediate);
#undef MUTATE
    }
    HhyTypedProfile profile={.enabled=true}; unsigned kinds[]={2};
    for(unsigned i=0;i<7;i++) require(!hhy_typed_observe(&profile,16,1,1,kinds),"early hot site");
    require(hhy_typed_observe(&profile,16,1,1,kinds)!=NULL,"hot site missing");
    kinds[0]=3;
    for(unsigned i=0;i<8;i++) require(!hhy_typed_observe(&profile,16,1,1,kinds),"unguarded entry");
    kinds[0]=2;
    for(unsigned i=0;i<16;i++) require(!hhy_typed_observe(&profile,16,1,1,kinds),"disabled site active");
    require(!hhy_typed_observe(&profile,16,65,1,kinds) && profile.dropped==1,"collision fallback");
    hhy_bytecode_chunk_free(&chunk); hhy_node_free(program); hhy_tokens_free(&tokens);
    puts("typed plan verification, registers, guards and deopt passed"); return 0;
}

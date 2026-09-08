#include "hhy/typed_plan.h"
#include "hhy/bytecode.h"
#include <inttypes.h>
#include <string.h>

static uint32_t child(const HhyBytecodeChunk *c, uint32_t p, uint32_t n) {
    size_t out;
    return hhy_bytecode_child(c, p, n, &out) ? (uint32_t)out : HHY_TYPED_NONE;
}
static bool literal(const HhyBytecodeChunk *c, uint32_t id, int64_t *value) {
    HhyInstruction x = c->code[id];
    if (x.opcode != HHY_OP_LITERAL || x.child_count || x.token_kind != HHY_T_INT || !x.token_length ||
        x.token_length > 18 || x.constant >= c->constant_count ||
        strlen(c->constants[x.constant]) < x.token_length) return false;
    int64_t v = 0;
    for (uint32_t i = 0; i < x.token_length; i++) {
        char ch = c->constants[x.constant][i];
        if (ch < '0' || ch > '9') return false;
        v = v * 10 + ch - '0';
    }
    *value = v; return true;
}
static uint32_t append(HhyTypedPlan *p, HhyTypedOpcode op, uint32_t source,
                       uint32_t a, uint32_t b, int64_t value) {
    if (p->count == HHY_TYPED_OPS) return HHY_TYPED_NONE;
    uint32_t id = p->count++;
    p->code[id] = (HhyTypedInstruction){op, source, a, b, value}; return id;
}
static uint32_t expression(const HhyBytecodeChunk *c, HhyTypedPlan *p, uint32_t id,
                           const uint32_t *parameters, bool scalar, unsigned depth) {
    if (id >= c->count || depth >= HHY_TYPED_OPS) return HHY_TYPED_NONE;
    HhyInstruction x = c->code[id]; int64_t value;
    if (literal(c, id, &value)) return append(p, HT_INT, id, HHY_TYPED_NONE, HHY_TYPED_NONE, value);
    if (x.opcode == HHY_OP_IDENTIFIER) {
        if (x.child_count || x.constant >= c->constant_count ||
            strlen(c->constants[x.constant]) != x.token_length) return HHY_TYPED_NONE;
        for (uint32_t i = 0; i < p->parameters; i++)
            if (x.constant == parameters[i]) return append(p, HT_ARG, id, HHY_TYPED_NONE, HHY_TYPED_NONE, i);
        return HHY_TYPED_NONE; /* Captures and dynamic binding loads are barriers. */
    }
    if (scalar && x.opcode == HHY_OP_INDEX && x.child_count == 2) {
        uint32_t list = child(c,id,0), index = child(c,id,1);
        if (list == HHY_TYPED_NONE || index == HHY_TYPED_NONE || c->code[list].opcode != HHY_OP_LIST ||
            !literal(c,index,&value) || value < 0 || (uint64_t)value >= c->code[list].child_count)
            return HHY_TYPED_NONE;
        for (uint32_t i = 0; i < p->count; i++)
            if (p->code[i].op == HT_RESERVE) return HHY_TYPED_NONE;
        if (append(p, HT_RESERVE, list, HHY_TYPED_NONE, HHY_TYPED_NONE, c->code[list].child_count) == HHY_TYPED_NONE)
            return HHY_TYPED_NONE;
        uint32_t selected = HHY_TYPED_NONE;
        for (uint32_t i = 0; i < c->code[list].child_count; i++) {
            uint32_t item = expression(c,p,child(c,list,i),parameters,false,depth+1);
            if (item == HHY_TYPED_NONE) return item;
            if (p->code[item].op >= HT_LT && p->code[item].op <= HT_GE) return HHY_TYPED_NONE;
            if (i == (uint64_t)value) selected = item;
        }
        return append(p, HT_COPY, id, selected, HHY_TYPED_NONE, 0);
    }
    HhyTypedOpcode op;
    if (x.opcode == HHY_OP_UNARY && x.child_count == 1 && x.token_kind == HHY_T_MINUS) op = HT_NEG;
    else if (x.opcode == HHY_OP_BINARY && x.child_count == 2) {
        switch (x.token_kind) {
            case HHY_T_PLUS: op=HT_ADD; break; case HHY_T_MINUS: op=HT_SUB; break;
            case HHY_T_STAR: op=HT_MUL; break; case HHY_T_MOD: op=HT_MOD; break;
            case HHY_T_LT: op=HT_LT; break; case HHY_T_LTE: op=HT_LE; break;
            case HHY_T_GT: op=HT_GT; break; case HHY_T_GTE: op=HT_GE; break;
            default: return HHY_TYPED_NONE;
        }
    } else return HHY_TYPED_NONE;
    uint32_t a = expression(c,p,child(c,id,0),parameters,scalar,depth+1);
    if (a == HHY_TYPED_NONE) return a;
    uint32_t b = HHY_TYPED_NONE;
    if (op != HT_NEG) b = expression(c,p,child(c,id,1),parameters,scalar,depth+1);
    if (op != HT_NEG && b == HHY_TYPED_NONE) return b;
    /* Comparisons may only define the final expression result. */
    if ((p->code[a].op >= HT_LT && p->code[a].op <= HT_GE) ||
        (b != HHY_TYPED_NONE && p->code[b].op >= HT_LT && p->code[b].op <= HT_GE)) return HHY_TYPED_NONE;
    return append(p,op,id,a,b,0);
}
bool hhy_typed_build(const HhyBytecodeChunk *c, uint32_t owner, HhyTypedPlan *out) {
    if (!c || !out || owner >= c->count) return false;
    HhyInstruction x=c->code[owner]; uint32_t params[HHY_TYPED_ARGS], body;
    HhyTypedPlan p={.version=HHY_TYPED_VERSION,.owner=owner};
    if (x.opcode == HHY_OP_FN_DECL && x.child_count >= 2 && x.child_count <= HHY_TYPED_ARGS+2) {
        p.parameters=x.child_count-2;
        for (uint32_t i=0;i<p.parameters;i++) {
            uint32_t param=child(c,owner,i+1);
            if (param>=c->count || c->code[param].opcode!=HHY_OP_IDENTIFIER || c->code[param].child_count ||
                c->code[param].constant>=c->constant_count ||
                strlen(c->constants[c->code[param].constant])!=c->code[param].token_length) return false;
            params[i]=c->code[param].constant;
            for (uint32_t j=0;j<i;j++) if(params[i]==params[j]) return false;
        }
        body=child(c,owner,x.child_count-1);
        if (body>=c->count || c->code[body].opcode != HHY_OP_BLOCK || c->code[body].child_count != 1) return false;
        body=child(c,body,0);
        if (body>=c->count || c->code[body].opcode != HHY_OP_RETURN || c->code[body].child_count != 1) return false;
    } else if (x.opcode == HHY_OP_CLOSURE && x.child_count == 2) {
        uint32_t param=child(c,owner,0);
        if(param>=c->count || c->code[param].opcode!=HHY_OP_IDENTIFIER || c->code[param].child_count ||
                c->code[param].constant>=c->constant_count ||
                strlen(c->constants[c->code[param].constant])!=c->code[param].token_length) return false;
        p.parameters=1; params[0]=c->code[param].constant;
        body=child(c,owner,1);
        if (body>=c->count || (c->code[body].opcode != HHY_OP_EXPR_STMT && c->code[body].opcode != HHY_OP_RETURN) || c->code[body].child_count != 1) return false;
    } else return false;
    p.expression=child(c,body,0);
    p.result=expression(c,&p,p.expression,params,true,0);
    if (p.result==HHY_TYPED_NONE) return false;
    p.boolean_result=p.code[p.result].op>=HT_LT && p.code[p.result].op<=HT_GE;
    *out=p; return true;
}
bool hhy_typed_verify(const HhyBytecodeChunk *c,const HhyTypedPlan *p) {
    if (!p || p->version!=HHY_TYPED_VERSION || !p->count || p->count>HHY_TYPED_OPS ||
        p->parameters>HHY_TYPED_ARGS || p->result>=p->count) return false;
    HhyTypedPlan expected;
    if (!hhy_typed_build(c,p->owner,&expected) || expected.expression!=p->expression ||
        expected.parameters!=p->parameters || expected.count!=p->count || expected.result!=p->result ||
        expected.boolean_result!=p->boolean_result) return false;
    for (uint32_t i=0;i<p->count;i++) {
        HhyTypedInstruction a=p->code[i], b=expected.code[i];
        if (a.op!=b.op || a.source!=b.source || a.lhs!=b.lhs || a.rhs!=b.rhs || a.immediate!=b.immediate) return false;
    }
    return true;
}
const HhyTypedPlan *hhy_typed_find(const HhyBytecodeChunk *c,uint32_t owner) {
    size_t lo=0,hi=c->typed_plan_count;
    while(lo<hi) {size_t mid=lo+(hi-lo)/2;if(c->typed_plans[mid].owner<owner)lo=mid+1;else hi=mid;}
    return lo<c->typed_plan_count && c->typed_plans[lo].owner==owner?&c->typed_plans[lo]:NULL;
}
HhyTypedResult hhy_typed_execute(const HhyTypedPlan *p,const int64_t *args,void (*reserve)(void *,size_t),void *context) {
    int64_t values[HHY_TYPED_OPS]={0};
    for(uint32_t i=0;i<p->count;i++) {
        HhyTypedInstruction x=p->code[i];
        int64_t a=x.lhs==HHY_TYPED_NONE?0:values[x.lhs], b=x.rhs==HHY_TYPED_NONE?0:values[x.rhs];
        bool ok=true;
        switch(x.op) {
            case HT_ARG: values[i]=args[x.immediate];break;
            case HT_INT: values[i]=x.immediate;break;
            case HT_ADD: ok=!__builtin_add_overflow(a,b,&values[i]);break;
            case HT_SUB: ok=!__builtin_sub_overflow(a,b,&values[i]);break;
            case HT_MUL: ok=!__builtin_mul_overflow(a,b,&values[i]);break;
            case HT_MOD: if(!b)ok=false;else values[i]=a==INT64_MIN&&b==-1?0:a%b;break;
            case HT_NEG: if(a==INT64_MIN)ok=false;else values[i]=-a;break;
            case HT_LT: values[i]=a<b;break;case HT_LE: values[i]=a<=b;break;
            case HT_GT: values[i]=a>b;break;case HT_GE: values[i]=a>=b;break;
            case HT_COPY: values[i]=a;break;
            case HT_RESERVE: reserve(context,(size_t)x.immediate);break;
        }
        if(!ok)return(HhyTypedResult){false,i,0,a,b};
    }
    return(HhyTypedResult){true,HHY_TYPED_NONE,values[p->result],0,0};
}
HhyTypedSite *hhy_typed_observe(HhyTypedProfile *p,uintptr_t chunk,uint32_t owner,size_t argc,const unsigned *kinds) {
    p->total_calls++;
    if(argc>HHY_TYPED_ARGS){p->unsupported++;return NULL;}
    size_t slot=((chunk>>4)^owner)%HHY_TYPED_SITES;
    HhyTypedSite *s=&p->sites[slot];
    if(s->occupied && (s->chunk!=chunk || s->owner!=owner)){p->dropped++;return NULL;}
    s->occupied=true;s->chunk=chunk;s->owner=owner;
    if(s->observations<UINT32_MAX)s->observations++;
    bool integers=true;
    for(size_t i=0;i<argc;i++){if(kinds[i]<64)s->argument_masks[i]|=UINT64_C(1)<<kinds[i];integers &= kinds[i]==2;}
    if(integers){if(s->stable<UINT32_MAX)s->stable++;}
    else {s->stable=0;s->guard_deopts++;if(++s->misses>=8)s->disabled=true;}
    return integers && s->stable>=8 && !s->disabled?s:NULL;
}
void hhy_typed_json(const HhyTypedProfile *p,FILE *out) {
    fprintf(out,"{\"schema_version\":1,\"enabled\":%s,\"scalar_enabled\":%s,\"reserved_bytes\":%zu,\"total_calls\":%"PRIu64",\"unsupported\":%"PRIu64",\"dropped\":%"PRIu64",\"sites\":[",p&&p->enabled?"true":"false",p&&p->scalar_enabled?"true":"false",p?sizeof(*p):0,p?p->total_calls:0,p?p->unsupported:0,p?p->dropped:0);
    bool first=true;
    if(p)for(size_t i=0;i<HHY_TYPED_SITES;i++) {
        const HhyTypedSite *s=&p->sites[i];if(!s->occupied)continue;
        fprintf(out,"%s{\"owner\":%u,\"observations\":%u,\"hits\":%"PRIu64",\"guard_deopts\":%"PRIu64",\"arithmetic_deopts\":%"PRIu64",\"scalar_reservations\":%"PRIu64",\"disabled\":%s,\"argument_kind_masks\":[",first?"":",",s->owner,s->observations,s->hits,s->guard_deopts,s->arithmetic_deopts,s->scalar_reservations,s->disabled?"true":"false");
        for(size_t j=0;j<HHY_TYPED_ARGS;j++)fprintf(out,"%s%"PRIu64,j?",":"",s->argument_masks[j]);
        fputs("]}",out);first=false;
    }
    fputs("]}",out);
}

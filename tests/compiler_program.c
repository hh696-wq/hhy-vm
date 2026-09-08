#include "hhy/compiler.h"
#include "hhy/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require(bool ok, const char *message) {
    if (!ok) {fprintf(stderr, "compiler: %s\n", message); exit(1);}
}
static HhyNode *parse(const char *text, HhySource *source, HhyTokenList *tokens) {
    *source = (HhySource){.path = "<compiler-test>", .text = (char *)text, .length = strlen(text)};
    HhyNode *program = NULL;
    require(hhy_lex(source, tokens), "lex");
    require(hhy_parse(source, tokens, &program).ok, "parse");
    hhy_resolve_slots(program); return program;
}
static void same_chunk(const HhyBytecodeChunk *a, const HhyBytecodeChunk *b) {
    require(a->count == b->count && a->constant_count == b->constant_count &&
        a->call_plan_count == b->call_plan_count && a->exception_region_count == b->exception_region_count &&
        a->stream_kernel_count == b->stream_kernel_count, "no-pass backend dimensions");
    for (size_t i = 0; i < a->count; i++) {
        HhyInstruction x = a->code[i], y = b->code[i];
        require(x.opcode == y.opcode && x.token_kind == y.token_kind && x.constant == y.constant &&
            x.token_length == y.token_length && x.child_count == y.child_count && x.subtree_size == y.subtree_size &&
            x.frame_slot_count == y.frame_slot_count && x.local_env_depth == y.local_env_depth &&
            x.local_binding_slot == y.local_binding_slot && x.local_slot_resolved == y.local_slot_resolved &&
            x.line == y.line && x.column == y.column, "no-pass instruction changed");
    }
    for (size_t i = 0; i < a->constant_count; i++) require(!strcmp(a->constants[i], b->constants[i]), "no-pass constant changed");
    require(hhy_bytecode_verify(a).ok && hhy_bytecode_verify(b).ok, "emitted table verification");
}
int main(void) {
    const char *text =
        "let a = 10\nlet b = a\nprint(b + (2 * 3))\n"
        "fn f(x) {\n 1 + 2\n if x { return 4 }\n return 5 + 0\n print(99)\n}\n"
        "for x in [1, 2] { if x == 1 { continue }\n break }\n"
        "let mut x = 0\nwhile x < 2 { x = x + 1 }\n"
        "try { throw(1) } catch e { print(e) }\n"
        "print([1] |> map { x -> 1 + 2 })\n";
    HhySource source; HhyTokenList tokens = {0};
    HhyNode *program = parse(text, &source, &tokens);
    HhyCompilerIR ir = {0}; HhyBytecodeChunk baseline = {0}, output = {0};
    require(hhy_compiler_lower(program, &ir).ok, "whole-program lower");
    require(hhy_bytecode_compile_direct(program, &baseline).ok, "baseline compile");
    require(hhy_compiler_emit(&ir, &output).ok, "IR emit"); same_chunk(&baseline, &output);
    size_t cfg_blocks = 0, loops = 0, handlers = 0, callable = 0;
    for (size_t i = 0; i < 2 * ir.count; i++) if (ir.blocks[i].present) {
        cfg_blocks++; loops += ir.blocks[i].phase == 1; handlers += ir.blocks[i].phase == 2;
        callable += ir.blocks[i].owner != 0;
        HhyCompilerBlock saved = ir.blocks[i];
#define EDGE_MUTATE(field) do { ir.blocks[i].field ^= 1u; \
    require(!hhy_compiler_verify(&ir).ok, "accepted CFG " #field); ir.blocks[i] = saved; } while (0)
        EDGE_MUTATE(node); EDGE_MUTATE(owner); EDGE_MUTATE(phase);
        EDGE_MUTATE(next); EDGE_MUTATE(alternate); EDGE_MUTATE(exception);
#undef EDGE_MUTATE
    }
    require(cfg_blocks > 15 && loops && handlers && callable, "CFG coverage");
    for (size_t i = 0; i < ir.count; i++) {
        HhyCompilerNode saved = ir.nodes[i];
#define NODE_MUTATE(field, value) do { ir.nodes[i].field = value; \
    require(!hhy_compiler_verify(&ir).ok, "accepted node " #field); ir.nodes[i] = saved; } while (0)
        NODE_MUTATE(op, HHY_COMPILER_CONST + 1);
        NODE_MUTATE(first_child, (uint32_t)i);
        NODE_MUTATE(next, (uint32_t)i);
        NODE_MUTATE(child_count, UINT32_MAX);
        NODE_MUTATE(effects, saved.effects ^ HHY_C_ALLOC);
        NODE_MUTATE(type, (HhyCompilerType)99);
        if (saved.resolved) {
            NODE_MUTATE(local_depth, UINT32_MAX);
            NODE_MUTATE(local_slot, UINT32_MAX);
        }
#undef NODE_MUTATE
    }
    for (size_t i = 0; i < ir.count; i++) if (ir.nodes[i].op == HHY_N_FN_DECL) {
        uint32_t name = ir.nodes[i].first_child;
        unsigned saved = ir.nodes[name].op; ir.nodes[name].op = HHY_N_LITERAL;
        require(!hhy_compiler_verify(&ir).ok, "function name accepted as a value operand");
        ir.nodes[name].op = saved;
    }
    ir.version++;
    HhyInstruction *saved_code = output.code;
    require(!hhy_compiler_emit(&ir, &output).ok && saved_code == output.code, "invalid emit must not touch output");
    ir.version--;
    unsigned rng = 1703;
    for (size_t iteration = 0; iteration < 10000; iteration++) {
        rng = rng * 1664525u + 1013904223u;
        size_t i = rng % ir.count; HhyCompilerNode saved = ir.nodes[i];
        rng = rng * 1664525u + 1013904223u;
        switch (iteration % 7) {
            case 0: ir.nodes[i].first_child = rng; break;
            case 1: ir.nodes[i].next = rng; break;
            case 2: ir.nodes[i].child_count = rng; break;
            case 3: ir.nodes[i].parent = rng; break;
            case 4: ir.nodes[i].effects = rng; break;
            case 5: ir.nodes[i].value = rng; break;
            default: ir.nodes[i].op = rng % (HHY_COMPILER_CONST + 4); break;
        }
        (void)hhy_compiler_verify(&ir); ir.nodes[i] = saved;
    }
    for (unsigned p = 0; p < HHY_COMPILER_PASS_COUNT; p++) {
        HhyCompilerOptions options = {{false}}; options.enabled[p] = true;
        HhyCompilerReport report = {0};
        require(hhy_compiler_lower(program, &ir).ok, "reset IR");
        require(hhy_compiler_optimize(&ir, options, &report).ok, "independent pass");
        require(hhy_compiler_emit(&ir, &output).ok, "pass backend");
        require(report.changed[p] > 0, "enabled pass never exercised");
        for (unsigned q = 0; q < HHY_COMPILER_PASS_COUNT; q++)
            if (q != p) require(report.changed[q] == 0, "disabled pass changed nodes");
    }
    HhyCompilerReport copy_report = {0};
    HhyCompilerOptions copy_only = {{false, false, true, false, false, false}};
    require(hhy_compiler_lower(program, &ir).ok, "origin lower");
    require(hhy_compiler_optimize(&ir, copy_only, &copy_report).ok, "origin optimize");
    size_t origins = 0;
    for (size_t i = 0; i < ir.count; i++) if (ir.nodes[i].origin != HHY_COMPILER_NONE) {
        origins++;
        HhyCompilerNode saved = ir.nodes[i];
        ir.nodes[i].origin = (uint32_t)i;
        require(!hhy_compiler_verify(&ir).ok, "self-dominating copy accepted");
        ir.nodes[i] = saved; ir.nodes[i].value++;
        require(!hhy_compiler_verify(&ir).ok, "copy value differs from dominating definition");
        ir.nodes[i] = saved;
    }
    require(origins > 0, "no copy provenance tested");
    HhyCompilerReport report = {0}; HhyCompilerOptions all = {{true,true,true,true,true,true}};
    require(hhy_compiler_compile(program, &output, all, &report).ok, "full pipeline");
    require(output.count < baseline.count && report.changed[0] && report.changed[1] &&
        report.changed[2] && report.changed[4] && report.changed[5], "passes did not optimize");
    /* A missing child in a detached instruction must fail before metadata walks it. */
    for (size_t i = 0; i < ir.count; i++) if (ir.nodes[i].op == HHY_N_BINARY) {
        HhyCompilerNode saved = ir.nodes[i]; ir.nodes[i].first_child = HHY_COMPILER_NONE;
        require(!hhy_compiler_verify(&ir).ok, "missing expression children"); ir.nodes[i] = saved;
    }
    hhy_compiler_ir_free(&ir); hhy_bytecode_chunk_free(&baseline); hhy_bytecode_chunk_free(&output);
    hhy_node_free(program); hhy_tokens_free(&tokens);
    HhyTokenList budget_tokens = {0}; HhySource budget_source;
    HhyNode *budget_program = parse("let a = 0xff\nprint(a)\n", &budget_source, &budget_tokens);
    HhyCompilerReport budget_report = {0};
    require(hhy_compiler_compile(budget_program, &output, all, &budget_report).ok, "size budget compile");
    require(budget_report.size_fallback && budget_report.optimized_bytecode_bytes >
        budget_report.baseline_bytecode_bytes, "constant-pool growth did not fall back");
    require(hhy_bytecode_compile_direct(budget_program, &baseline).ok, "budget baseline");
    same_chunk(&baseline, &output);
    hhy_node_free(budget_program); hhy_tokens_free(&budget_tokens);
    hhy_bytecode_chunk_free(&output); hhy_bytecode_chunk_free(&baseline);
    puts("compiler: whole-program backend, CFG mutations, 10000 typed mutations, independent passes passed");
    return 0;
}

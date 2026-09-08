#include "hhy/compiler.h"
#include "hhy/parser.h"
#include <string.h>
int main(int argc, char **argv) {
    if (argc != 3 || (strcmp(argv[1], "--text") && strcmp(argv[1], "--metrics") && strcmp(argv[1], "--json"))) return 2;
    HhySource source = {0}; HhyTokenList tokens = {0}; HhyNode *program = NULL;
    if (!hhy_source_load(argv[2], &source)) return 2;
    bool ok = hhy_lex(&source, &tokens) && hhy_parse(&source, &tokens, &program).ok;
    HhyCompilerIR ir = {0}; HhyBytecodeChunk chunk = {0}; HhyCompilerReport report = {0};
    if (ok) {
        hhy_resolve_slots(program);
        if (!strcmp(argv[1], "--json")) {
            ok = hhy_compiler_lower(program, &ir).ok;
            if (ok) {
                fputs("{\"before\":", stdout); hhy_compiler_dump_json(&ir, stdout);
                ok = hhy_compiler_optimize(&ir, hhy_compiler_options(), &report).ok;
                fputs(",\"after\":", stdout); hhy_compiler_dump_json(&ir, stdout);
                fputs("}\n", stdout);
            }
        } else if (!strcmp(argv[1], "--text")) {
            ok = hhy_compiler_lower(program, &ir).ok &&
                hhy_compiler_optimize(&ir, hhy_compiler_options(), &report).ok;
            if (ok) hhy_compiler_dump(&ir, stdout);
        } else {
            ok = hhy_compiler_compile(program, &chunk, hhy_compiler_options(), &report).ok;
            if (ok) hhy_compiler_report(&report, stdout);
        }
    }
    hhy_compiler_ir_free(&ir); hhy_bytecode_chunk_free(&chunk);
    hhy_node_free(program); hhy_tokens_free(&tokens); hhy_source_free(&source);
    return ok ? 0 : 2;
}

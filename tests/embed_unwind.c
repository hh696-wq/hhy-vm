#define _POSIX_C_SOURCE 200809L
#include "hhy/embed.h"
#include "hhy/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glob.h>
#include <unistd.h>

static void require(int ok, const char *message) {
    if (!ok) { fprintf(stderr, "embed unwind failure: %s\n", message); exit(1); }
}
static void recovered(HhyContext *context) {
    HhyEmbedResult result = hhy_call(context, "add", "[20,22]");
    require(result.ok && result.json && strcmp(result.json, "42") == 0,
            result.error_json ? result.error_json : "context did not recover");
    hhy_embed_result_free(&result);
}
static void memory_error(HhyEmbedResult *result) {
    require(!result->ok && result->exit_code == 1 && result->error_json &&
            strstr(result->error_json, "HHY_MEMORY_LIMIT"), "missing memory error");
    hhy_embed_result_free(result);
}
static void init_failure(HhyExecutionEngine engine) {
    HhyApplication *bad = hhy_application_load("tests/acceptance/embed-init-memory.hhy");
    require(bad != NULL, "cannot parse init fixture");
    HhyRuntimeLimits limits = hhy_runtime_limits_default();
    limits.max_memory = 256 * 1024;
    HhyContext *failed = hhy_context_new_engine(bad, &limits, engine);
    require(failed == NULL, "oversized initialization succeeded");
    hhy_application_free(bad);
    /* A stale active chunk would misroute this explicit AST execution. */
    HhySource source = {.path = "<after-init-failure>", .text = "exit(23)\n", .length = 9};
    HhyTokenList tokens = {0}; HhyNode *program = NULL;
    require(hhy_lex(&source, &tokens), "cannot lex AST fixture");
    require(hhy_parse(&source, &tokens, &program).ok, "cannot parse AST fixture");
    HhyRunResult result = hhy_run_program_engine(&source, program, 0, NULL, false, NULL, HHY_ENGINE_AST);
    require(result.ok && result.exit_code == 23, "AST after failed context initialization was misrouted");
    hhy_node_free(program); hhy_tokens_free(&tokens);
}
int main(int argc, char **argv) {
    require(argc >= 3, "expected mode and engine");
    HhyExecutionEngine engine = strcmp(argv[2], "ast") == 0 ? HHY_ENGINE_AST : HHY_ENGINE_BYTECODE;
    if (strcmp(argv[1], "init") == 0) { init_failure(engine); return 0; }
    HhyApplication *app = hhy_application_load("tests/acceptance/embed-unwind.hhy");
    require(app != NULL, "cannot load fixture");
    HhyRuntimeLimits limits = hhy_runtime_limits_default();
    limits.max_memory = 256 * 1024; limits.max_open_files = 2; limits.max_recursion = 8;
    if (strcmp(argv[1], "time") == 0) limits.max_runtime_ns = 1;
    HhyContext *context = hhy_context_new_engine(app, &limits, engine);
    require(context != NULL, "cannot initialize context");
    if (strcmp(argv[1], "arguments") == 0) {
        size_t size = 1024 * 1024;
        char *input = malloc(size + 5); require(input != NULL, "malloc");
        input[0] = '['; input[1] = '"'; memset(input + 2, 'x', size);
        memcpy(input + size + 2, "\"]", 3);
        for (int i = 0; i < 8; i++) {
            HhyEmbedResult result = hhy_call(context, "echo", input);
            memory_error(&result); recovered(context);
        }
        free(input);
        /* The argument vector itself must also be inside the memory boundary. */
        size_t count = 10000;
        input = malloc(count * 2 + 2); require(input != NULL, "argv malloc");
        input[0] = '[';
        for (size_t i = 0; i < count; i++) { input[1 + i * 2] = '0'; input[2 + i * 2] = ','; }
        input[count * 2] = ']'; input[count * 2 + 1] = '\0';
        HhyEmbedResult vector = hhy_call(context, "echo", input);
        memory_error(&vector); recovered(context); free(input);
    } else if (strcmp(argv[1], "files") == 0 || strcmp(argv[1], "atomic") == 0) {
        require(argc == 4, "expected scratch directory");
        char large[1024], small[1024], target[1024], input[2200], pattern[1060];
        snprintf(large, sizeof(large), "%s/large.txt", argv[3]);
        snprintf(small, sizeof(small), "%s/small.txt", argv[3]);
        snprintf(target, sizeof(target), "%s/target.txt", argv[3]);
        snprintf(pattern, sizeof(pattern), "%s.hhy-tmp-*", target);
        FILE *file = fopen(large, "wb"); require(file != NULL, "large file");
        for (int i = 0; i < 1024 * 1024; i++) fputc('x', file);
        fclose(file);
        file = fopen(small, "wb"); require(file != NULL, "small file"); fputs("healthy", file); fclose(file);
        file = fopen(target, "wb"); require(file != NULL, "target file"); fputs("preserved", file); fclose(file);
        const char *functions[] = {"text_file", "bytes_file", "atomic_copy"};
        for (int iteration = 0; iteration < 12; iteration++) {
            int selection = (iteration + (strcmp(argv[1], "atomic") == 0 ? 2 : 0)) % 3;
            const char *function = functions[selection];
            if (selection == 2) snprintf(input, sizeof(input), "[\"%s\",\"%s\"]", large, target);
            else snprintf(input, sizeof(input), "[\"%s\"]", large);
            HhyEmbedResult result = hhy_call(context, function, input);
            memory_error(&result);
            glob_t paths = {0}; int status = glob(pattern, 0, NULL, &paths);
            require(status == GLOB_NOMATCH, "temporary file escaped call boundary"); globfree(&paths);
            snprintf(input, sizeof(input), "[\"%s\"]", small);
            result = hhy_call(context, "text_file", input);
            require(result.ok && result.json && strcmp(result.json, "\"healthy\"") == 0,
                    "read handle or file budget leaked");
            hhy_embed_result_free(&result); recovered(context);
        }
        char content[16] = {0}; file = fopen(target, "rb"); require(file != NULL, "target disappeared");
        require(fread(content, 1, sizeof(content), file) == 9 && strcmp(content, "preserved") == 0,
                "failed atomic save changed target"); fclose(file);
    } else if (strcmp(argv[1], "time") == 0) {
        for (int i = 0; i < 4; i++) {
            HhyEmbedResult result = hhy_call(context, "timed", "[]");
            require(!result.ok && result.exit_code == 5 && result.error_json &&
                    strstr(result.error_json, "HHY_RUNTIME_TIMEOUT"), "timeout did not unwind");
            hhy_embed_result_free(&result); recovered(context);
        }
    } else if (strcmp(argv[1], "errors") == 0) {
        const char *functions[] = {"unsupported", "thrown_function", "recurse"};
        const char *errors[] = {"SerializationError", "RuntimeError", "HHY_RECURSION_LIMIT"};
        for (size_t i = 0; i < 3; i++) {
            HhyEmbedResult result = hhy_call(context, functions[i], i == 2 ? "[0]" : "[]");
            require(!result.ok && result.error_json && strstr(result.error_json, errors[i]),
                    "error or unsupported return was accepted");
            hhy_embed_result_free(&result); recovered(context);
        }
    } else require(0, "unknown mode");
    hhy_context_free(context); hhy_application_free(app);
    return 0;
}

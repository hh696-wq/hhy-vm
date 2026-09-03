#include "hhy/embed.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    HhyApplication *application = hhy_application_load("tests/acceptance/embed-handler.hhy");
    if (application == NULL) return 1;
    HhyContext *context = hhy_context_new_engine(application, NULL, HHY_ENGINE_BYTECODE);
    HhyContext *ast_context = hhy_context_new_engine(application, NULL, HHY_ENGINE_AST);
    if (context == NULL || ast_context == NULL) { hhy_context_free(context); hhy_context_free(ast_context); hhy_application_free(application); return 2; }

    HhyEmbedResult first = hhy_context_call_json(context, "handler", "[{\"name\":\"HHY\"}]");
    HhyEmbedResult second = hhy_context_call_json(context, "add", "[20,22]");
    HhyEmbedResult ast = hhy_call(ast_context, "handler", "[{\"name\":\"HHY\"}]");
    HhyEmbedResult missing = hhy_context_call_json(context, "missing", "[]");
    int failed = !first.ok || first.json == NULL ||
        strcmp(first.json, "{\"ok\":true,\"value\":{\"name\":\"HHY\"}}") != 0 ||
        !second.ok || second.json == NULL || strcmp(second.json, "42") != 0 ||
        !ast.ok || strcmp(first.json, ast.json) != 0 ||
        missing.ok || missing.error_json == NULL;
    for (int i = 0; i < 100000 && !failed; i++) {
        HhyEmbedResult repeated = hhy_call(context, "add", "[1,2]");
        failed = !repeated.ok || repeated.json == NULL || strcmp(repeated.json, "3") != 0;
        hhy_embed_result_free(&repeated);
    }
    if (failed) {
        fprintf(stderr, "embed test failed: first=%s second=%s missing=%s\n",
                first.json == NULL ? "<null>" : first.json,
                second.json == NULL ? "<null>" : second.json,
                missing.error_json == NULL ? "<null>" : missing.error_json);
    }
    hhy_embed_result_free(&first);
    hhy_embed_result_free(&second);
    hhy_embed_result_free(&ast);
    hhy_embed_result_free(&missing);
    hhy_context_free(context);
    hhy_context_free(ast_context);
    hhy_application_free(application);
    return failed;
}

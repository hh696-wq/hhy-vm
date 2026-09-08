/* Private state assertions complement the public embedding/differential tests. */
#include "../src/runtime.c"
#include <stdio.h>

static void require_frame(bool ok, const char *message) {
    if (!ok) { fprintf(stderr, "call unwind: %s\n", message); exit(1); }
}
static void assert_clean(Runtime *rt) {
    require_frame(rt->call_frame_count == 0 && rt->call_depth == 0 && rt->call_stack_count == 0,
        "call depth/trace/registry not restored");
    require_frame(rt->current_contract == NULL && rt->effect_allowed && rt->signal == SIGNAL_NONE,
        "contract/effect/control state not restored");
    for (size_t i = 0; i < rt->call_frame_capacity * sizeof(*rt->call_frames); i++)
        require_frame(((unsigned char *)rt->call_frames)[i] == 0, "dead call record retained roots");
}
int main(int argc, char **argv) {
    require_frame(argc == 2, "expected engine");
    HhyApplication *app = hhy_application_load("tests/acceptance/embed-call-frames.hhy");
    require_frame(app != NULL, "load fixture");
    HhyRuntimeLimits limits = hhy_runtime_limits_default();
    limits.max_memory = 256 * 1024; limits.max_recursion = 64;
    HhyContext *context = hhy_context_new_engine(app, &limits,
        strcmp(argv[1], "ast") == 0 ? HHY_ENGINE_AST : HHY_ENGINE_BYTECODE);
    require_frame(context != NULL, "initialize context");
    Runtime *rt = context->runtime;
    const char *functions[] = {"exhaust", "fail", "recurse", "timed"};
    const char *expected[] = {"HHY_MEMORY_LIMIT", "failure", "HHY_RECURSION_LIMIT", "HHY_RUNTIME_TIMEOUT"};
    for (size_t pass = 0; pass < 4; pass++) {
        for (size_t i = 0; i < 4; i++) {
            rt->limits.max_memory = i == 0 ? 256 * 1024 : 2 * 1024 * 1024;
            rt->memory_check_budget = 0;
            rt->limits.max_recursion = i == 2 ? 8 : 64;
            rt->limits.max_runtime_ns = i == 3 ? 1000000 : 0;
            HhyEmbedResult result = hhy_call(context, functions[i], "[24]");
            if (result.ok || !result.error_json || !strstr(result.error_json, expected[i]))
                fprintf(stderr, "pass=%zu function=%s status=%d error=%s\n", pass, functions[i], result.exit_code,
                    result.error_json ? result.error_json : "<none>");
            require_frame(!result.ok && result.error_json && strstr(result.error_json, expected[i]), "missing expected failure");
            hhy_embed_result_free(&result); assert_clean(rt);
            rt->limits.max_runtime_ns = 0;
            result = hhy_call(context, "add", "[20,22]");
            require_frame(result.ok && result.json && strcmp(result.json, "42") == 0, "context failed to recover");
            hhy_embed_result_free(&result); assert_clean(rt);
        }
    }
    if (rt->registered_call_unwind)
        require_frame(rt->call_frame_capacity >= 32, "registry growth was not exercised");
    rt->limits.max_memory = 256 * 1024; rt->memory_check_budget = 0;
    HhyEmbedResult imported = hhy_call(context, "import_failure", "[]");
    require_frame(!imported.ok && imported.error_json && strstr(imported.error_json, "HHY_MEMORY_LIMIT"), "module initialization did not exhaust memory");
    hhy_embed_result_free(&imported); assert_clean(rt);
    require_frame(rt->source == &app->source, "failed import left module source active");
    imported = hhy_call(context, "add", "[20,22]");
    require_frame(imported.ok && imported.json && strcmp(imported.json, "42") == 0, "call after failed import did not recover");
    hhy_embed_result_free(&imported); assert_clean(rt);
    hhy_context_free(context); hhy_application_free(app);
    puts("registered call frames restore state and clear roots after repeated host failures");
    return 0;
}

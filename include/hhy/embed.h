#ifndef HHY_EMBED_H
#define HHY_EMBED_H

#include <stdbool.h>
#include "hhy/runtime.h"

typedef struct HhyApplication HhyApplication;
typedef struct HhyContext HhyContext;

typedef struct {
    bool ok;
    int exit_code;
    char *json;
    char *error_json;
} HhyEmbedResult;

/* Parse an HHY application once. The application must outlive its contexts. */
HhyApplication *hhy_application_load(const char *path);
void hhy_application_free(HhyApplication *application);

/* Execute top-level initialization once and retain its environment. */
HhyContext *hhy_context_new(HhyApplication *application,
                            const HhyRuntimeLimits *limits);
HhyContext *hhy_context_new_engine(HhyApplication *application,
                                   const HhyRuntimeLimits *limits,
                                   HhyExecutionEngine engine);

/* Call a top-level function repeatedly through a JSON array/object boundary. */
HhyEmbedResult hhy_context_call_json(HhyContext *context,
                                     const char *function_name,
                                     const char *arguments_json);
HhyEmbedResult hhy_call(HhyContext *context, const char *function_name,
                        const char *arguments_json);
void hhy_embed_result_free(HhyEmbedResult *result);
void hhy_context_free(HhyContext *context);

#endif

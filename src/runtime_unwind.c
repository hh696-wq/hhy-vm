#include "runtime_unwind.h"
#include <stddef.h>

/* Streams may own extension handles: close them before ending that scope.
   Restore execution state last, after every resource has been detached. */
const HhyUnwindTable hhy_runtime_boundary_unwind = {
    .version = HHY_RUNTIME_UNWIND_VERSION,
    .count = HHY_UNWIND_ACTION_COUNT,
    .actions = {HHY_UNWIND_STREAMS, HHY_UNWIND_TEMPORARIES,
                HHY_UNWIND_EXTENSION_SCOPE, HHY_UNWIND_EXECUTION_STATE}
};

bool hhy_runtime_unwind_verify(const HhyUnwindTable *table) {
    if (table == NULL || table->version != HHY_RUNTIME_UNWIND_VERSION ||
        table->count != HHY_UNWIND_ACTION_COUNT) return false;
    for (uint32_t i = 0; i < table->count; i++)
        if (table->actions[i] != (HhyUnwindAction)i) return false;
    return true;
}

bool hhy_runtime_unwind_apply(const HhyUnwindTable *table, void *state,
                               void (*step)(void *, HhyUnwindAction)) {
    /* Validate the entire table before performing any cleanup side effect. */
    if (!hhy_runtime_unwind_verify(table) || step == NULL) return false;
    for (uint32_t i = 0; i < table->count; i++) step(state, table->actions[i]);
    return true;
}

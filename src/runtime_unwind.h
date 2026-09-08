#ifndef HHY_RUNTIME_UNWIND_H
#define HHY_RUNTIME_UNWIND_H

#include <stdbool.h>
#include <stdint.h>

/* Internal host-boundary cleanup contract, not a public Bytecode ABI or a
   replacement for language try/catch. Callbacks must not allocate managed data. */
#define HHY_RUNTIME_UNWIND_VERSION 1u

typedef enum {
    HHY_UNWIND_STREAMS,
    HHY_UNWIND_TEMPORARIES,
    HHY_UNWIND_EXTENSION_SCOPE,
    HHY_UNWIND_EXECUTION_STATE,
    HHY_UNWIND_ACTION_COUNT
} HhyUnwindAction;

typedef struct {
    uint32_t version;
    uint32_t count;
    HhyUnwindAction actions[HHY_UNWIND_ACTION_COUNT];
} HhyUnwindTable;

extern const HhyUnwindTable hhy_runtime_boundary_unwind;
bool hhy_runtime_unwind_verify(const HhyUnwindTable *table);
bool hhy_runtime_unwind_apply(const HhyUnwindTable *table, void *state,
                               void (*step)(void *, HhyUnwindAction));
#endif

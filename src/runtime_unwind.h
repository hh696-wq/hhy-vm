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
/* Registered call frames survive host longjmp in scanned managed storage.
   This describes Runtime state restoration, not platform machine-stack ABI. */
#define HHY_CALL_UNWIND_VERSION 1u
typedef enum {
    HHY_CALL_UNWIND_ENVIRONMENT,
    HHY_CALL_UNWIND_CONTRACT,
    HHY_CALL_UNWIND_DEPTH_TRACE,
    HHY_CALL_UNWIND_PROFILER,
    HHY_CALL_UNWIND_ROOTS,
    HHY_CALL_UNWIND_ACTION_COUNT
} HhyCallUnwindAction;
typedef struct {
    uint32_t version, count;
    HhyCallUnwindAction actions[HHY_CALL_UNWIND_ACTION_COUNT];
} HhyCallUnwindTable;
extern const HhyCallUnwindTable hhy_runtime_call_unwind;
bool hhy_call_unwind_verify(const HhyCallUnwindTable *table);
bool hhy_call_unwind_apply(const HhyCallUnwindTable *table, void *state,
    void (*step)(void *, HhyCallUnwindAction));
#endif

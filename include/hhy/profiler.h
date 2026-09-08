#ifndef HHY_PROFILER_H
#define HHY_PROFILER_H

#include "hhy/typed_plan.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

typedef struct HhyProfiler HhyProfiler;
void hhy_profiler_typed_report(HhyProfiler *, const HhyTypedProfile *);

#define HHY_PROFILE_MAX_OPTIMIZATION_STAGES 16u
#define HHY_PROFILE_MAX_KERNEL_OPCODES 8u

typedef struct {
    const char *operation;
    uint32_t kernel_version;
    size_t opcode_count;
    const char *opcodes[HHY_PROFILE_MAX_KERNEL_OPCODES];
} HhyProfileOptimizationStage;

typedef struct {
    bool cpu;
    bool heap;
    bool json;
    FILE *output;
    const char *engine;
} HhyProfileOptions;

HhyProfiler *hhy_profiler_start(const HhyProfileOptions *options,
                                const char *source_path, size_t heap_baseline);
typedef enum {
    HHY_PROFILE_DISPATCH_GENERIC,
    HHY_PROFILE_DISPATCH_ARGUMENT_ATTEMPT,
    HHY_PROFILE_DISPATCH_DOMAIN_COUNT
} HhyProfileDispatchDomain;

/* Counts recursive VM switch entries, not adjacent fusible instructions. */
void hhy_profiler_dispatch(HhyProfiler *profiler, unsigned opcode, HhyProfileDispatchDomain domain);
typedef enum {
    HHY_FRAME_CONFIGURED, HHY_FRAME_ALLOCATED, HHY_FRAME_REUSED, HHY_FRAME_CACHED,
    HHY_FRAME_ESCAPED, HHY_FRAME_DISCARDED
} HhyFramePoolEvent;
void hhy_profiler_frame_pool(HhyProfiler *profiler, HhyFramePoolEvent event,
    bool bounded, size_t probes, size_t retained, size_t retained_gc_bytes);
typedef enum {
    HHY_CALL_CONFIGURED, HHY_CALL_PUSH, HHY_CALL_RETURN, HHY_CALL_ERROR,
    HHY_CALL_CANCEL, HHY_CALL_RESOURCE
} HhyCallUnwindEvent;
void hhy_profiler_call_unwind(HhyProfiler *profiler, HhyCallUnwindEvent event,
    bool enabled, size_t active, size_t reserved_bytes);
void hhy_profiler_exception_layout(HhyProfiler *profiler, bool selected);
void hhy_profiler_call_layout(HhyProfiler *profiler, bool selected);
struct HhyLookupProfile;
void hhy_profiler_lookup_report(HhyProfiler *, const struct HhyLookupProfile *);
void hhy_profiler_stop(HhyProfiler *profiler, size_t heap_current);
void hhy_profiler_free(HhyProfiler *profiler);

size_t hhy_profiler_enter(HhyProfiler *profiler, const char *name,
                          const char *path, uint32_t line, uint32_t column);
size_t hhy_profiler_enter_n(HhyProfiler *profiler, const char *name, size_t name_length,
                            const char *path, uint32_t line, uint32_t column);
size_t hhy_profiler_register(HhyProfiler *profiler, const char *name,
                             const char *path, uint32_t line, uint32_t column);
size_t hhy_profiler_enter_registered(HhyProfiler *profiler, size_t entry);
void hhy_profiler_leave(HhyProfiler *profiler, size_t previous_entry);
bool hhy_profiler_tracks_heap(const HhyProfiler *profiler);
void hhy_profiler_allocation(HhyProfiler *profiler, size_t bytes,
                             size_t heap_current);
void hhy_profiler_optimization(HhyProfiler *profiler, bool selected,
                               const char *reason,
                               const HhyProfileOptimizationStage *stages,
                               size_t stage_count);

#endif

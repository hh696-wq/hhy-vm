#ifndef HHY_PROFILER_H
#define HHY_PROFILER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

typedef struct HhyProfiler HhyProfiler;

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

#ifndef HHY_PROFILER_H
#define HHY_PROFILER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

typedef struct HhyProfiler HhyProfiler;

typedef struct {
    bool cpu;
    bool heap;
    bool json;
    FILE *output;
} HhyProfileOptions;

HhyProfiler *hhy_profiler_start(const HhyProfileOptions *options,
                                const char *source_path, size_t heap_baseline);
void hhy_profiler_stop(HhyProfiler *profiler, size_t heap_current);
void hhy_profiler_free(HhyProfiler *profiler);

size_t hhy_profiler_enter(HhyProfiler *profiler, const char *name,
                          const char *path, uint32_t line, uint32_t column);
void hhy_profiler_leave(HhyProfiler *profiler, size_t previous_entry);
void hhy_profiler_allocation(HhyProfiler *profiler, size_t bytes,
                             size_t heap_current);

#endif

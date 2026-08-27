#define _POSIX_C_SOURCE 200809L
#include "hhy/profiler.h"

#include <inttypes.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>

typedef struct {
    char *name;
    size_t name_length;
    char *path;
    uint32_t line;
    uint32_t column;
    volatile sig_atomic_t samples;
    uint64_t calls;
    uint64_t allocated_bytes;
    uint64_t allocations;
} ProfileEntry;

struct HhyProfiler {
    HhyProfileOptions options;
    char *source_path;
    ProfileEntry *entries;
    size_t entry_count;
    size_t entry_capacity;
    volatile sig_atomic_t current_entry;
    volatile sig_atomic_t total_samples;
    uint64_t total_allocated;
    uint64_t total_allocations;
    size_t heap_baseline;
    size_t heap_peak;
    size_t heap_current;
    struct timespec wall_started;
    struct rusage usage_started;
    double wall_seconds;
    double cpu_seconds;
    struct sigaction previous_action;
    struct itimerval previous_timer;
    bool sampling;
};

static HhyProfiler *active_profiler;

static char *copy_text_n(const char *text, size_t length) {
    char *copy = malloc(length + 1);
    if (copy != NULL) { memcpy(copy, text, length); copy[length] = '\0'; }
    return copy;
}

static char *copy_text(const char *text) { return copy_text_n(text, strlen(text)); }

static void sample_handler(int signal_number) {
    (void)signal_number;
    HhyProfiler *profiler = active_profiler;
    if (profiler == NULL) return;
    sig_atomic_t index = profiler->current_entry;
    if (index >= 0 && (size_t)index < profiler->entry_count) {
        profiler->entries[index].samples++;
        profiler->total_samples++;
    }
}

static double timespec_delta(struct timespec end, struct timespec start) {
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
}

static double timeval_seconds(struct timeval value) {
    return (double)value.tv_sec + (double)value.tv_usec / 1000000.0;
}

static bool same_entry(const ProfileEntry *entry, const char *name, size_t name_length,
                       const char *path,
                       uint32_t line, uint32_t column) {
    return entry->line == line && entry->column == column && entry->name_length == name_length &&
           memcmp(entry->name, name, name_length) == 0 && strcmp(entry->path, path) == 0;
}

static size_t find_entry(HhyProfiler *profiler, const char *name, size_t name_length,
                         const char *path, uint32_t line, uint32_t column) {
    for (size_t i = 0; i < profiler->entry_count; i++)
        if (same_entry(&profiler->entries[i], name, name_length, path, line, column)) return i;
    return SIZE_MAX;
}

static size_t find_or_add(HhyProfiler *profiler, const char *name, size_t name_length,
                          const char *path,
                          uint32_t line, uint32_t column) {
    size_t existing = find_entry(profiler, name, name_length, path, line, column);
    if (existing != SIZE_MAX) return existing;
    if (profiler->entry_count == profiler->entry_capacity) {
        size_t capacity = profiler->entry_capacity < 16 ? 16 : profiler->entry_capacity * 2;
        ProfileEntry *entries = realloc(profiler->entries, capacity * sizeof(*entries));
        if (entries == NULL) return SIZE_MAX;
        profiler->entries = entries;
        profiler->entry_capacity = capacity;
    }
    size_t index = profiler->entry_count++;
    ProfileEntry *entry = &profiler->entries[index];
    memset(entry, 0, sizeof(*entry));
    entry->name = copy_text_n(name, name_length);
    entry->path = copy_text(path);
    if (entry->name == NULL || entry->path == NULL) {
        free(entry->name); free(entry->path); profiler->entry_count--; return SIZE_MAX;
    }
    entry->name_length = name_length; entry->line = line; entry->column = column;
    return index;
}

HhyProfiler *hhy_profiler_start(const HhyProfileOptions *options,
                                const char *source_path, size_t heap_baseline) {
    HhyProfiler *profiler = calloc(1, sizeof(*profiler));
    if (profiler == NULL) return NULL;
    profiler->options = *options;
    profiler->source_path = copy_text(source_path);
    profiler->heap_baseline = heap_baseline;
    profiler->current_entry = -1;
    clock_gettime(CLOCK_MONOTONIC, &profiler->wall_started);
    getrusage(RUSAGE_SELF, &profiler->usage_started);
    if (profiler->options.cpu) {
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = sample_handler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESTART;
        if (sigaction(SIGPROF, &action, &profiler->previous_action) == 0) {
            struct itimerval timer;
            memset(&timer, 0, sizeof(timer));
            timer.it_interval.tv_usec = 1000;
            timer.it_value = timer.it_interval;
            active_profiler = profiler;
            if (setitimer(ITIMER_PROF, &timer, &profiler->previous_timer) == 0)
                profiler->sampling = true;
            else active_profiler = NULL;
        }
    }
    return profiler;
}

size_t hhy_profiler_enter(HhyProfiler *profiler, const char *name,
                          const char *path, uint32_t line, uint32_t column) {
    return hhy_profiler_enter_n(profiler, name, strlen(name), path, line, column);
}

size_t hhy_profiler_enter_n(HhyProfiler *profiler, const char *name, size_t name_length,
                            const char *path, uint32_t line, uint32_t column) {
    if (profiler == NULL) return SIZE_MAX;
    size_t previous = profiler->current_entry < 0 ? SIZE_MAX : (size_t)profiler->current_entry;
    size_t index = find_entry(profiler, name, name_length, path, line, column);
    if (index != SIZE_MAX) {
        profiler->entries[index].calls++;
        profiler->current_entry = (sig_atomic_t)index;
        return previous;
    }
    sigset_t blocked, previous_mask;
    sigemptyset(&blocked); sigaddset(&blocked, SIGPROF);
    sigprocmask(SIG_BLOCK, &blocked, &previous_mask);
    index = find_or_add(profiler, name, name_length, path, line, column);
    if (index != SIZE_MAX) {
        profiler->entries[index].calls++;
        profiler->current_entry = (sig_atomic_t)index;
    }
    sigprocmask(SIG_SETMASK, &previous_mask, NULL);
    return previous;
}

bool hhy_profiler_tracks_heap(const HhyProfiler *profiler) {
    return profiler != NULL && profiler->options.heap;
}

void hhy_profiler_leave(HhyProfiler *profiler, size_t previous_entry) {
    if (profiler == NULL) return;
    profiler->current_entry = previous_entry == SIZE_MAX ? -1 : (sig_atomic_t)previous_entry;
}

void hhy_profiler_allocation(HhyProfiler *profiler, size_t bytes, size_t heap_current) {
    if (profiler == NULL || !profiler->options.heap) return;
    profiler->total_allocated += bytes;
    profiler->total_allocations++;
    size_t local = heap_current > profiler->heap_baseline
        ? heap_current - profiler->heap_baseline : 0;
    if (local > profiler->heap_peak) profiler->heap_peak = local;
    sig_atomic_t index = profiler->current_entry;
    if (index >= 0 && (size_t)index < profiler->entry_count) {
        profiler->entries[index].allocated_bytes += bytes;
        profiler->entries[index].allocations++;
    }
}

static int compare_cpu(const void *left, const void *right) {
    const ProfileEntry *a = *(const ProfileEntry * const *)left;
    const ProfileEntry *b = *(const ProfileEntry * const *)right;
    if (a->samples != b->samples) return a->samples < b->samples ? 1 : -1;
    return a->calls < b->calls ? 1 : a->calls > b->calls ? -1 : 0;
}

static int compare_heap(const void *left, const void *right) {
    const ProfileEntry *a = *(const ProfileEntry * const *)left;
    const ProfileEntry *b = *(const ProfileEntry * const *)right;
    return a->allocated_bytes < b->allocated_bytes ? 1 :
           a->allocated_bytes > b->allocated_bytes ? -1 : 0;
}

static void json_string(FILE *out, const char *text) {
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p == '"' || *p == '\\') { fputc('\\', out); fputc(*p, out); }
        else if (*p == '\n') fputs("\\n", out);
        else if (*p < 0x20) fprintf(out, "\\u%04x", *p);
        else fputc(*p, out);
    }
    fputc('"', out);
}

static void print_json(HhyProfiler *p, FILE *out) {
    fputs("{\n  \"source\": ", out); json_string(out, p->source_path);
    fprintf(out, ",\n  \"wall_seconds\": %.9f,\n  \"cpu_seconds\": %.9f,"
            "\n  \"cpu_samples\": %d,\n  \"heap_peak_bytes\": %zu,"
            "\n  \"heap_current_bytes\": %zu,\n  \"allocated_bytes\": %" PRIu64
            ",\n  \"allocations\": %" PRIu64 ",\n  \"hotspots\": [",
            p->wall_seconds, p->cpu_seconds, (int)p->total_samples, p->heap_peak,
            p->heap_current, p->total_allocated, p->total_allocations);
    for (size_t i = 0; i < p->entry_count; i++) {
        ProfileEntry *e = &p->entries[i];
        fputs(i ? ",\n    {\"name\": " : "\n    {\"name\": ", out); json_string(out, e->name);
        fputs(", \"path\": ", out); json_string(out, e->path);
        fprintf(out, ", \"line\": %u, \"column\": %u, \"calls\": %" PRIu64
                ", \"cpu_samples\": %d, \"allocated_bytes\": %" PRIu64
                ", \"allocations\": %" PRIu64 "}", e->line, e->column, e->calls,
                (int)e->samples, e->allocated_bytes, e->allocations);
    }
    fputs(p->entry_count ? "\n  ]\n}\n" : "]\n}\n", out);
}

static void print_bytes(FILE *out, uint64_t bytes) {
    static const char *units[] = {"B", "KiB", "MiB", "GiB"};
    double value = (double)bytes; size_t unit = 0;
    while (value >= 1024.0 && unit < 3) { value /= 1024.0; unit++; }
    if (unit == 0) fprintf(out, "%" PRIu64 " B", bytes);
    else fprintf(out, "%.1f %s", value, units[unit]);
}

static void print_text(HhyProfiler *p, FILE *out) {
    fprintf(out, "HHY profile: %s\n\nSummary\n", p->source_path);
    fprintf(out, "  Wall time        %.3f s\n  CPU time         %.3f s\n", p->wall_seconds, p->cpu_seconds);
    fprintf(out, "  CPU utilization  %.1f%%\n", p->wall_seconds > 0 ? p->cpu_seconds / p->wall_seconds * 100.0 : 0.0);
    if (p->options.cpu) fprintf(out, "  CPU samples      %d\n", (int)p->total_samples);
    if (p->options.heap) {
        fputs("  Heap peak        ", out); print_bytes(out, p->heap_peak); fputc('\n', out);
        fputs("  Heap after GC    ", out); print_bytes(out, p->heap_current); fputc('\n', out);
        fputs("  Allocated        ", out); print_bytes(out, p->total_allocated); fputc('\n', out);
        fprintf(out, "  Allocations      %" PRIu64 "\n", p->total_allocations);
    }
    ProfileEntry **sorted = p->entry_count ? malloc(p->entry_count * sizeof(*sorted)) : NULL;
    if (sorted == NULL && p->entry_count) return;
    for (size_t i = 0; i < p->entry_count; i++) sorted[i] = &p->entries[i];
    if (p->options.cpu) {
        qsort(sorted, p->entry_count, sizeof(*sorted), compare_cpu);
        fputs("\nCPU hotspots\n  CPU%    Samples      Calls  Function\n", out);
        size_t shown = p->entry_count < 15 ? p->entry_count : 15;
        for (size_t i = 0; i < shown; i++) {
            ProfileEntry *e = sorted[i];
            double percent = p->total_samples ? (double)e->samples * 100.0 / p->total_samples : 0.0;
            fprintf(out, "  %5.1f%%  %7d  %9" PRIu64 "  %s  %s:%u:%u\n",
                    percent, (int)e->samples, e->calls, e->name, e->path, e->line, e->column);
        }
        if (p->total_samples < 10)
            fputs("  Note: fewer than 10 CPU samples; use a larger workload for stable results.\n", out);
    }
    if (p->options.heap) {
        qsort(sorted, p->entry_count, sizeof(*sorted), compare_heap);
        fputs("\nAllocation hotspots\n  Bytes          Objects  Function\n", out);
        size_t shown = p->entry_count < 15 ? p->entry_count : 15;
        for (size_t i = 0; i < shown; i++) {
            ProfileEntry *e = sorted[i];
            if (e->allocations == 0) continue;
            fputs("  ", out); print_bytes(out, e->allocated_bytes);
            fprintf(out, "%*s  %9" PRIu64 "  %s  %s:%u:%u\n",
                    e->allocated_bytes < 1024 ? 8 : 6, "", e->allocations,
                    e->name, e->path, e->line, e->column);
        }
    }
    free(sorted);
}

void hhy_profiler_stop(HhyProfiler *profiler, size_t heap_current) {
    if (profiler == NULL) return;
    if (profiler->sampling) {
        struct itimerval stopped;
        memset(&stopped, 0, sizeof(stopped));
        setitimer(ITIMER_PROF, &stopped, NULL);
        active_profiler = NULL;
        sigaction(SIGPROF, &profiler->previous_action, NULL);
        setitimer(ITIMER_PROF, &profiler->previous_timer, NULL);
        profiler->sampling = false;
    }
    struct timespec wall_ended; struct rusage usage_ended;
    clock_gettime(CLOCK_MONOTONIC, &wall_ended); getrusage(RUSAGE_SELF, &usage_ended);
    profiler->wall_seconds = timespec_delta(wall_ended, profiler->wall_started);
    profiler->cpu_seconds =
        timeval_seconds(usage_ended.ru_utime) + timeval_seconds(usage_ended.ru_stime) -
        timeval_seconds(profiler->usage_started.ru_utime) - timeval_seconds(profiler->usage_started.ru_stime);
    profiler->heap_current = heap_current > profiler->heap_baseline
        ? heap_current - profiler->heap_baseline : 0;
    if (profiler->heap_current > profiler->heap_peak) profiler->heap_peak = profiler->heap_current;
    FILE *out = profiler->options.output == NULL ? stderr : profiler->options.output;
    if (profiler->options.json) print_json(profiler, out); else print_text(profiler, out);
}

void hhy_profiler_free(HhyProfiler *profiler) {
    if (profiler == NULL) return;
    for (size_t i = 0; i < profiler->entry_count; i++) {
        free(profiler->entries[i].name); free(profiler->entries[i].path);
    }
    free(profiler->entries); free(profiler->source_path); free(profiler);
}

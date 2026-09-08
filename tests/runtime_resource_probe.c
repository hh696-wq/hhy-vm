/* Standalone diagnostic process. No collector hooks enter the production CLI. */
#include "../src/runtime.c"
#define PROBE_SAMPLES 4096u
static struct {
    pid_t owner;
    uint64_t collection_start, pause_start, collections, pauses;
    uint64_t collection_ns, pause_ns, max_pause_ns, samples[PROBE_SAMPLES];
    size_t count;
} probe;
static uint64_t probe_now(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) abort();
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) + (uint64_t)value.tv_nsec;
}
/* GC calls with its allocation lock held: no allocation, logging or GC APIs. */
static void GC_CALLBACK probe_event(GC_EventType event) {
    if (getpid() != probe.owner) return;
    uint64_t now = probe_now();
    if (event == GC_EVENT_START) probe.collection_start = now;
    else if (event == GC_EVENT_END && probe.collection_start) {
        probe.collection_ns += now - probe.collection_start;
        probe.collection_start = 0; probe.collections++;
    } else if (event == GC_EVENT_PRE_STOP_WORLD) probe.pause_start = now;
    else if (event == GC_EVENT_POST_START_WORLD && probe.pause_start) {
        uint64_t elapsed = now - probe.pause_start;
        probe.pause_ns += elapsed; probe.pauses++;
        if (elapsed > probe.max_pause_ns) probe.max_pause_ns = elapsed;
        if (probe.count < PROBE_SAMPLES) probe.samples[probe.count++] = elapsed;
        probe.pause_start = 0;
    }
}
static uint64_t allocated(const struct GC_prof_stats_s *s) {
    return (uint64_t)s->allocd_bytes_before_gc + s->bytes_allocd_since_gc;
}
int main(int argc, char **argv) {
    if (argc < 4 || (strcmp(argv[2], "ast") && strcmp(argv[2], "bytecode"))) {
        fputs("usage: resource-probe report.json ast|bytecode source.hhy [program args...]\n", stderr);
        return 2;
    }
    HhyApplication *app = hhy_application_load(argv[3]);
    if (!app) return 2;
    GC_INIT();
    GC_on_collection_event_proc previous = GC_get_on_collection_event();
    if (previous != NULL) { fputs("resource probe requires an unowned collector callback\n",stderr); hhy_application_free(app); return 2; }
    struct GC_prof_stats_s before = {0}, after = {0};
    if (GC_get_prof_stats(&before,sizeof(before)) < offsetof(struct GC_prof_stats_s,gc_no)+sizeof(before.gc_no)) return 2;
    probe.owner = getpid();
    GC_set_on_collection_event(probe_event);
    uint64_t started = probe_now();
    bool live_context = getenv("HHY_PROBE_LIVE_CONTEXT") && !strcmp(getenv("HHY_PROBE_LIVE_CONTEXT"), "1");
    if (live_context && argc != 4) { GC_set_on_collection_event(previous); hhy_application_free(app); return 2; }
    HhyContext *context = NULL;
    HhyExecutionEngine engine = !strcmp(argv[2],"ast")?HHY_ENGINE_AST:HHY_ENGINE_BYTECODE;
    HhyRunResult result;
    if (live_context) {
        context = hhy_context_new_engine(app, NULL, engine);
        result = (HhyRunResult){.ok = context != NULL, .exit_code = context ? 0 : 1};
    } else result = hhy_run_program_engine(&app->source, app->program, argc-4, argv+4, false, NULL, engine);
    uint64_t elapsed = probe_now()-started;
    GC_set_on_collection_event(previous);
    GC_get_prof_stats(&after,sizeof(after));
    size_t before_final = GC_get_memory_use();
    uint64_t final_started = probe_now();
    GC_gcollect();
    uint64_t final_ns = probe_now()-final_started;
    size_t retained = GC_get_memory_use();
    struct rusage usage, children;
    getrusage(RUSAGE_SELF,&usage); getrusage(RUSAGE_CHILDREN,&children);
    FILE *out = fopen(argv[1],"w");
    if (!out) { hhy_application_free(app); return 2; }
    fprintf(out,"{\"schema_version\":1,\"exit_code\":%d,\"scope\":\"parent_process_runtime_execution\",\"elapsed_ns\":%"PRIu64",\"allocated_bytes\":%"PRIu64",\"collections\":%"PRIu64",\"gc_cycle_delta\":%lu,\"collection_ns\":%"PRIu64",\"pause_count\":%"PRIu64",\"pause_ns\":%"PRIu64",\"max_pause_ns\":%"PRIu64",\"sample_limit\":%u,\"samples_dropped\":%"PRIu64",\"callback_restored\":%s,\"incremental\":%s,\"promotion_bytes\":null,\"write_barrier_ns\":null,\"heap_bytes\":%lu,\"pre_final_used_bytes\":%zu,\"post_run_retained_estimate_bytes\":%zu,\"forced_final_collection_ns\":%"PRIu64",\"max_rss_native_units\":%ld,\"children_user_us\":%lld,\"children_system_us\":%lld,\"pause_samples_ns\":[",
        result.exit_code,elapsed,allocated(&after)-allocated(&before),probe.collections,(unsigned long)(after.gc_no-before.gc_no),probe.collection_ns,probe.pauses,probe.pause_ns,probe.max_pause_ns,PROBE_SAMPLES,probe.pauses-probe.count,
        GC_get_on_collection_event()==previous?"true":"false",GC_is_incremental_mode()?"true":"false",(unsigned long)after.heapsize_full,before_final,retained,final_ns,usage.ru_maxrss,
        (long long)children.ru_utime.tv_sec*1000000+children.ru_utime.tv_usec,(long long)children.ru_stime.tv_sec*1000000+children.ru_stime.tv_usec);
    for (size_t i=0;i<probe.count;i++) fprintf(out,"%s%"PRIu64,i?",":"",probe.samples[i]);
    fprintf(out, "],\"live_context_retained\":%s}\n", context ? "true" : "false");
    bool written = !ferror(out); if (fclose(out) != 0) written=false;
    if (context) hhy_context_free(context);
    hhy_application_free(app);
    return written?result.exit_code:2;
}

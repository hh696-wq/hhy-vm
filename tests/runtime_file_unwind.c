/* Exercise private resource ownership at deterministic allocation boundaries. */
#ifndef HHY_RUNTIME_SOURCE
#define HHY_RUNTIME_SOURCE "../src/runtime.c"
#endif
#include HHY_RUNTIME_SOURCE
#include <sys/resource.h>
#include <dirent.h>
#include <fcntl.h>

static void require_file_test(bool ok, const char *message) {
    if (!ok) { fprintf(stderr, "file unwind: %s\n", message); exit(1); }
}
static size_t open_descriptors(void) {
    size_t count = 0;
    for (int fd = 0; fd < 1024; fd++) if (fcntl(fd, F_GETFD) >= 0) count++;
    return count;
}
static bool no_temporary(const char *directory) {
    DIR *dir = opendir(directory); require_file_test(dir != NULL, "open scratch directory");
    struct dirent *entry; bool clean = true;
    while ((entry = readdir(dir)) != NULL)
        if (strstr(entry->d_name, ".hhy-tmp-") != NULL) clean = false;
    closedir(dir); return clean;
}
int main(int argc, char **argv) {
    require_file_test(argc == 3, "expected mode and scratch directory");
    GC_INIT();
    Runtime *rt = GC_malloc_uncollectable(sizeof(*rt));
    require_file_test(rt != NULL, "allocate runtime"); memset(rt, 0, sizeof(*rt));
    rt->limits = hhy_runtime_limits_default(); rt->effect_allowed = true;
    HhyNode site = {.token = {.line = 1, .column = 1}};
    char source[4096], target[4096];
    require_file_test(snprintf(source, sizeof(source), "%s/source", argv[2]) < (int)sizeof(source), "source path");
    require_file_test(snprintf(target, sizeof(target), "%s/target", argv[2]) < (int)sizeof(target), "target path");
    FILE *file = fopen(source, "wb"); require_file_test(file != NULL, "create source");
    for (int i = 0; i < 8192; i++) fputc('s', file);
    require_file_test(fclose(file) == 0, "close source");
    file = fopen(target, "wb"); require_file_test(file != NULL, "create target");
    require_file_test(fputs("preserved", file) >= 0 && fclose(file) == 0, "initialize target");
    const size_t descriptors = open_descriptors();
    if (strcmp(argv[1], "write-failure") == 0) {
        struct rlimit original; require_file_test(getrlimit(RLIMIT_FSIZE, &original) == 0, "get file size limit");
        struct rlimit zero = original; zero.rlim_cur = 0;
        signal(SIGXFSZ, SIG_IGN);
        require_file_test(setrlimit(RLIMIT_FSIZE, &zero) == 0, "set file size limit");
        rt->limits.max_open_files = 2;
        for (int i = 0; i < 8; i++) {
            rt->failed = false; rt->exit_code = 0;
            require_file_test(!write_bytes_atomic(rt, &site, target, "payload", 7,
                (OutputOptions){.overwrite = true}), "write unexpectedly succeeded");
            require_file_test(rt->failed && rt->exit_code == 4, "missing original I/O error");
            require_file_test(rt->active_open_files == 0, "file budget leaked after write failure");
            require_file_test(open_descriptors() == descriptors, "native descriptor leaked after write failure");
            require_file_test(no_temporary(argv[2]), "temporary escaped write failure");
        }
        require_file_test(setrlimit(RLIMIT_FSIZE, &original) == 0, "restore file size limit");
    } else {
        bool copy = strcmp(argv[1], "copy-quota") == 0;
        require_file_test(copy || strcmp(argv[1], "write-quota") == 0, "unknown mode");
        /* fdopen's file quota error itself cannot allocate in this Runtime. */
        rt->limits.max_memory = 1; rt->limits.max_open_files = copy ? 1 : 0;
        rt->memory_jump_ready = true;
        if (setjmp(rt->memory_jump) == 0) {
            if (copy) (void)copy_file_atomic(rt, &site, source, target, true);
            else (void)write_bytes_atomic(rt, &site, target, "payload", 7, (OutputOptions){.overwrite = true});
            require_file_test(false, "expected memory quota longjmp");
        }
        rt->memory_jump_ready = false;
        require_file_test(rt->failed, "missing quota failure");
        runtime_cleanup_to(rt, NULL);
        require_file_test(rt->active_open_files == 0, "counted source file escaped quota unwind");
        require_file_test(open_descriptors() == descriptors, "raw descriptor escaped quota unwind");
        require_file_test(no_temporary(argv[2]), "temporary escaped quota unwind");
    }
    char content[16] = {0}; file = fopen(target, "rb"); require_file_test(file != NULL, "read target");
    require_file_test(fread(content, 1, 9, file) == 9 && strcmp(content, "preserved") == 0, "failed operation changed target");
    fclose(file);
    rt->limits = hhy_runtime_limits_default(); rt->failed = false; rt->exit_code = 0;
    rt->memory_check_budget = 0; rt->memory_baseline = GC_get_memory_use();
    require_file_test(write_bytes_atomic(rt, &site, target, "healthy", 7, (OutputOptions){.overwrite = true}), "Runtime did not recover");
    require_file_test(rt->active_open_files == 0 && no_temporary(argv[2]), "successful retry leaked resources");
    runtime_release(rt); GC_free(rt);
    puts("file ownership and recovery passed");
    return 0;
}

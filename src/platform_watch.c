#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "hhy/platform_watch.h"
#include "hhy/common.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __APPLE__
#include <fts.h>
#include <sys/event.h>
#elif defined(__linux__)
#include <fts.h>
#include <sys/inotify.h>
#endif

struct HhyPlatformWatch {
    char *path;
    bool recursive;
    size_t maximum;
    size_t handle_count;
#ifdef __APPLE__
    int queue;
    int *descriptors;
    size_t descriptor_count;
#elif defined(__linux__)
    int descriptor;
#else
    struct stat snapshot;
#endif
};

#if defined(__APPLE__) || defined(__linux__)
static bool is_directory(const char *path) {
    struct stat info;
    return lstat(path, &info) == 0 && S_ISDIR(info.st_mode);
}
#endif

#ifdef __APPLE__
static void clear_handles(HhyPlatformWatch *watch) {
    for (size_t i = 0; i < watch->descriptor_count; i++) close(watch->descriptors[i]);
    free(watch->descriptors); watch->descriptors = NULL;
    watch->descriptor_count = 0; watch->handle_count = watch->queue >= 0 ? 1 : 0;
}

static bool add_kqueue_path(HhyPlatformWatch *watch, const char *path,
                            const char **error) {
    if (watch->handle_count >= watch->maximum) {
        *error = "watch exceeds RuntimeLimits.max_open_files"; return false;
    }
    int descriptor = open(path, O_EVTONLY | O_CLOEXEC);
    if (descriptor < 0) { *error = "cannot open watch target"; return false; }
    struct kevent change;
    EV_SET(&change, descriptor, EVFILT_VNODE, EV_ADD | EV_CLEAR,
           NOTE_WRITE | NOTE_DELETE | NOTE_RENAME | NOTE_ATTRIB | NOTE_EXTEND,
           0, NULL);
    if (kevent(watch->queue, &change, 1, NULL, 0, NULL) != 0) {
        close(descriptor); *error = "cannot register kqueue watch"; return false;
    }
    watch->descriptors = hhy_realloc(watch->descriptors,
        (watch->descriptor_count + 1) * sizeof(int));
    watch->descriptors[watch->descriptor_count++] = descriptor;
    watch->handle_count++;
    return true;
}

static bool register_paths(HhyPlatformWatch *watch, const char **error) {
    clear_handles(watch);
    if (!is_directory(watch->path)) return add_kqueue_path(watch, watch->path, error);
    char *paths[] = {watch->path, NULL};
    FTS *tree = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
    if (tree == NULL) { *error = "cannot traverse watch target"; return false; }
    bool ok = true; FTSENT *entry;
    while ((entry = fts_read(tree)) != NULL) {
        if (entry->fts_info != FTS_D) continue;
        if (!watch->recursive && entry->fts_level > 0) {
            fts_set(tree, entry, FTS_SKIP); continue;
        }
        if (!add_kqueue_path(watch, entry->fts_path, error)) { ok = false; break; }
    }
    fts_close(tree); return ok;
}
#elif defined(__linux__)
static void clear_handles(HhyPlatformWatch *watch) {
    if (watch->descriptor >= 0) close(watch->descriptor);
    watch->descriptor = -1; watch->handle_count = 0;
}

static bool add_inotify_path(HhyPlatformWatch *watch, const char *path,
                             const char **error) {
    uint32_t events = IN_CREATE | IN_MODIFY | IN_CLOSE_WRITE | IN_DELETE |
                      IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB | IN_DELETE_SELF |
                      IN_MOVE_SELF;
    if (inotify_add_watch(watch->descriptor, path, events) < 0) {
        *error = "cannot register inotify watch"; return false;
    }
    return true;
}

static bool register_paths(HhyPlatformWatch *watch, const char **error) {
    clear_handles(watch);
    if (watch->maximum < 1) { *error = "watch exceeds RuntimeLimits.max_open_files"; return false; }
    watch->descriptor = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (watch->descriptor < 0) { *error = "cannot create inotify watcher"; return false; }
    watch->handle_count = 1;
    if (!is_directory(watch->path)) return add_inotify_path(watch, watch->path, error);
    char *paths[] = {watch->path, NULL};
    FTS *tree = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
    if (tree == NULL) { *error = "cannot traverse watch target"; return false; }
    bool ok = true; FTSENT *entry;
    while ((entry = fts_read(tree)) != NULL) {
        if (entry->fts_info != FTS_D) continue;
        if (!watch->recursive && entry->fts_level > 0) {
            fts_set(tree, entry, FTS_SKIP); continue;
        }
        if (!add_inotify_path(watch, entry->fts_path, error)) { ok = false; break; }
    }
    fts_close(tree); return ok;
}
#else
static void clear_handles(HhyPlatformWatch *watch) {
    watch->handle_count = 0;
}

static bool register_paths(HhyPlatformWatch *watch, const char **error) {
    if (watch->maximum < 1) { *error = "watch exceeds RuntimeLimits.max_open_files"; return false; }
    if (lstat(watch->path, &watch->snapshot) != 0) { *error = "cannot stat watch target"; return false; }
    watch->handle_count = 1;
    return true;
}
#endif

HhyPlatformWatch *hhy_platform_watch_open(const char *path, bool recursive,
                                          size_t max_handles, const char **error) {
    HhyPlatformWatch *watch = hhy_alloc(sizeof(*watch));
    watch->path = hhy_strndup(path, strlen(path)); watch->recursive = recursive;
    watch->maximum = max_handles;
#ifdef __APPLE__
    watch->queue = kqueue();
    if (watch->queue < 0) { *error = "cannot create kqueue watcher"; hhy_platform_watch_close(watch); return NULL; }
    watch->handle_count = 1;
#elif defined(__linux__)
    watch->descriptor = -1;
#endif
    if (!register_paths(watch, error)) { hhy_platform_watch_close(watch); return NULL; }
    return watch;
}

int hhy_platform_watch_wait(HhyPlatformWatch *watch, int timeout_ms,
                            const char **error) {
#ifdef __APPLE__
    struct kevent event;
    struct timespec timeout = {.tv_sec = timeout_ms / 1000,
        .tv_nsec = (timeout_ms % 1000) * 1000000L};
    int result = kevent(watch->queue, NULL, 0, &event, 1, &timeout);
    if (result < 0 && errno != EINTR) { *error = "kqueue wait failed"; return -1; }
    return result > 0 ? 1 : 0;
#elif defined(__linux__)
    struct pollfd item = {.fd = watch->descriptor, .events = POLLIN};
    int result = poll(&item, 1, timeout_ms);
    if (result < 0 && errno != EINTR) { *error = "inotify wait failed"; return -1; }
    if (result <= 0) return 0;
    char buffer[16384];
    while (read(watch->descriptor, buffer, sizeof(buffer)) > 0) {}
    return 1;
#else
    (void)poll(NULL, 0, timeout_ms);
    struct stat current;
    if (lstat(watch->path, &current) != 0) { *error = "cannot stat watch target"; return -1; }
    bool changed = current.st_mtime != watch->snapshot.st_mtime ||
                   current.st_size != watch->snapshot.st_size ||
                   current.st_ino != watch->snapshot.st_ino;
    if (changed) watch->snapshot = current;
    return changed ? 1 : 0;
#endif
}

bool hhy_platform_watch_rebuild(HhyPlatformWatch *watch, const char **error) {
    return register_paths(watch, error);
}

size_t hhy_platform_watch_handle_count(const HhyPlatformWatch *watch) {
    return watch == NULL ? 0 : watch->handle_count;
}

void hhy_platform_watch_close(HhyPlatformWatch *watch) {
    if (watch == NULL) return;
    clear_handles(watch);
#ifdef __APPLE__
    if (watch->queue >= 0) close(watch->queue);
#endif
    free(watch->path); free(watch);
}

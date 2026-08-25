#ifndef HHY_PLATFORM_WATCH_H
#define HHY_PLATFORM_WATCH_H

#include <stdbool.h>
#include <stddef.h>

typedef struct HhyPlatformWatch HhyPlatformWatch;

HhyPlatformWatch *hhy_platform_watch_open(const char *path, bool recursive,
                                          size_t max_handles, const char **error);
int hhy_platform_watch_wait(HhyPlatformWatch *watch, int timeout_ms,
                            const char **error);
bool hhy_platform_watch_rebuild(HhyPlatformWatch *watch, const char **error);
size_t hhy_platform_watch_handle_count(const HhyPlatformWatch *watch);
void hhy_platform_watch_close(HhyPlatformWatch *watch);

#endif

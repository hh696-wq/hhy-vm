#ifndef HHY_RUNTIME_LOOKUP_H
#define HHY_RUNTIME_LOOKUP_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#define HHY_LOOKUP_SITES 64u
#define HHY_LOOKUP_KINDS 3u
/* Numeric identities are never dereferenced; this buffer contains no GC roots. */
typedef struct {
    uintptr_t site;
    char source_path[128];
    bool source_truncated;
    uint64_t targets[4], previous;
    size_t slot;
    uint32_t line, column, distinct, invalidations;
    uint64_t observations, transitions, stable;
    bool occupied, slot_valid, megamorphic;
} HhyLookupSite;
typedef struct HhyLookupProfile {
    bool cache_enabled, feedback_enabled;
    HhyLookupSite sites[HHY_LOOKUP_KINDS][HHY_LOOKUP_SITES];
    uint64_t totals[HHY_LOOKUP_KINDS], dropped[HHY_LOOKUP_KINDS];
    uint64_t resolved, binding_hit, binding_search, binding_absent, builtin_lookups;
    uint64_t map_hit, map_miss, map_cold, map_guard, map_missing, map_mega;
    uint64_t map_cached, map_generic;
} HhyLookupProfile;
HhyLookupSite *hhy_lookup_site(HhyLookupProfile *, unsigned, uintptr_t, uint32_t, uint32_t, const char *);
void hhy_lookup_observe(HhyLookupSite *, uint64_t);
void hhy_lookup_json(const HhyLookupProfile *, FILE *);
#endif

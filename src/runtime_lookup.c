#include "runtime_lookup.h"
#include <inttypes.h>
HhyLookupSite *hhy_lookup_site(HhyLookupProfile *p, unsigned kind, uintptr_t site,
    uint32_t line, uint32_t column, const char *path) {
    if (p == NULL || kind >= HHY_LOOKUP_KINDS) return NULL;
    p->totals[kind]++;
    size_t start = (size_t)((site >> 3) ^ (site >> 11) ^ line ^ ((uintptr_t)column << 5)) % HHY_LOOKUP_SITES;
    for (size_t i = 0; i < HHY_LOOKUP_SITES; i++) {
        HhyLookupSite *s = &p->sites[kind][(start + i) % HHY_LOOKUP_SITES];
        if (!s->occupied) {
            s->occupied = true; s->site = site; s->line = line; s->column = column;
            int length = snprintf(s->source_path, sizeof(s->source_path), "%s", path ? path : "<runtime>");
            s->source_truncated = length < 0 || (size_t)length >= sizeof(s->source_path);
        }
        if (s->site == site && s->line == line && s->column == column) return s;
    }
    p->dropped[kind]++; return NULL;
}
void hhy_lookup_observe(HhyLookupSite *s, uint64_t target) {
    if (s == NULL) return;
    if (s->observations++) {
        if (s->previous == target) s->stable++; else s->transitions++;
    }
    s->previous = target;
    for (unsigned i = 0; i < s->distinct && i < 4; i++)
        if (s->targets[i] == target) return;
    if (s->distinct < 4) s->targets[s->distinct++] = target;
    else { s->distinct = 5; s->megamorphic = true; }
}
static void json_string(FILE *out, const char *text) {
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p == '"' || *p == '\\') { fputc('\\', out); fputc(*p, out); }
        else if (*p < 32 || *p >= 127) fprintf(out, "\\u%04x", (unsigned)*p);
        else fputc(*p, out);
    }
    fputc('"', out);
}
void hhy_lookup_json(const HhyLookupProfile *p, FILE *out) {
    fprintf(out, ",\n  \"lookup_profile\": {\"schema_version\": 1, \"enabled\": %s, \"map_cache_enabled\": %s, \"reserved_bytes\": %zu, \"sites_per_kind_limit\": %u",
        p && p->feedback_enabled ? "true" : "false", p && p->cache_enabled ? "true" : "false", p ? sizeof(*p) : 0, HHY_LOOKUP_SITES);
    if (p) {
        fprintf(out, ", \"binding\": {\"resolved\": %"PRIu64", \"cache_hit\": %"PRIu64", \"search\": %"PRIu64", \"absent\": %"PRIu64"}",p->resolved,p->binding_hit,p->binding_search,p->binding_absent);
        fprintf(out, ", \"builtin_resolutions\": %"PRIu64, p->builtin_lookups);
        fprintf(out, ", \"map\": {\"guard_hit\": %"PRIu64", \"guard_miss\": %"PRIu64", \"cold\": %"PRIu64", \"slot_or_key_changed\": %"PRIu64", \"key_missing\": %"PRIu64", \"megamorphic_fallback\": %"PRIu64", \"cached_reads\": %"PRIu64", \"generic_reads\": %"PRIu64"}",p->map_hit,p->map_miss,p->map_cold,p->map_guard,p->map_missing,p->map_mega,p->map_cached,p->map_generic);
        const char *names[] = {"binding", "call_target", "map_access"};
        fputs(", \"domains\": [",out);
        for (unsigned k=0;k<HHY_LOOKUP_KINDS;k++) {
            fprintf(out,"%s{\"kind\": \"%s\", \"observations\": %"PRIu64", \"dropped\": %"PRIu64", \"sites\": [", k?", ":"", names[k],p->totals[k],p->dropped[k]);
            bool first=true;
            for (unsigned i=0;i<HHY_LOOKUP_SITES;i++) {
                const HhyLookupSite *s=&p->sites[k][i]; if (!s->occupied) continue;
                fprintf(out,"%s{\"source_bytes\": ",first?"":", ");
                json_string(out, s->source_path);
                fprintf(out,", \"source_truncated\": %s", s->source_truncated?"true":"false");
                fprintf(out,", \"id\": %u, \"line\": %u, \"column\": %u, \"observations\": %"PRIu64", \"distinct_capped\": %u, \"stable\": %"PRIu64", \"transitions\": %"PRIu64", \"megamorphic\": %s}",i,s->line,s->column,s->observations,s->distinct,s->stable,s->transitions,s->megamorphic?"true":"false");first=false;
            }
            fputs("]}",out);
        }
        fputs("]",out);
    }
    fputs("}",out);
}

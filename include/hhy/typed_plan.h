#ifndef HHY_TYPED_PLAN_H
#define HHY_TYPED_PLAN_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#define HHY_TYPED_VERSION 1u
#define HHY_TYPED_OPS 64u
#define HHY_TYPED_ARGS 8u
#define HHY_TYPED_SITES 64u
#define HHY_TYPED_NONE UINT32_MAX
struct HhyBytecodeChunk;
typedef enum { HT_ARG, HT_INT, HT_ADD, HT_SUB, HT_MUL, HT_MOD, HT_NEG,
    HT_LT, HT_LE, HT_GT, HT_GE, HT_RESERVE, HT_COPY } HhyTypedOpcode;
typedef struct {
    HhyTypedOpcode op;
    uint32_t source, lhs, rhs;
    int64_t immediate;
} HhyTypedInstruction;
typedef struct HhyTypedPlan {
    uint32_t version, owner, expression, parameters, count, result;
    bool boolean_result;
    HhyTypedInstruction code[HHY_TYPED_OPS];
} HhyTypedPlan;
typedef struct {
    uintptr_t chunk;
    uint32_t owner, observations, stable, misses;
    uint64_t argument_masks[HHY_TYPED_ARGS];
    uint64_t hits, guard_deopts, arithmetic_deopts, scalar_reservations;
    bool occupied, disabled;
} HhyTypedSite;
typedef struct {
    bool enabled, scalar_enabled;
    uint64_t dropped, unsupported, total_calls;
    HhyTypedSite sites[HHY_TYPED_SITES];
} HhyTypedProfile;
typedef struct {bool ok; uint32_t failure; int64_t value, left, right;} HhyTypedResult;
/* Called only by the compiler/verified-bytecode boundary. */
bool hhy_typed_build(const struct HhyBytecodeChunk *, uint32_t, HhyTypedPlan *);
bool hhy_typed_verify(const struct HhyBytecodeChunk *, const HhyTypedPlan *);
const HhyTypedPlan *hhy_typed_find(const struct HhyBytecodeChunk *, uint32_t);
/* Reserve callback executes before every scalarized aggregate's elements. */
HhyTypedResult hhy_typed_execute(const HhyTypedPlan *, const int64_t *, void (*)(void *, size_t), void *);
HhyTypedSite *hhy_typed_observe(HhyTypedProfile *, uintptr_t, uint32_t, size_t, const unsigned *);
void hhy_typed_json(const HhyTypedProfile *, FILE *);
#endif

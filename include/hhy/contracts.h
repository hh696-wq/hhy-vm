#ifndef HHY_CONTRACTS_H
#define HHY_CONTRACTS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    HHY_EFFECT_NONE,
    HHY_EFFECT_FILESYSTEM,
    HHY_EFFECT_PROCESS,
    HHY_EFFECT_NETWORK,
    HHY_EFFECT_CUSTOM
} HhyEffect;

typedef struct {
    const char *name;
    size_t minimum_arity;
    size_t maximum_arity;
    HhyEffect effect;
    bool lazy;
    bool cancellable;
    bool sendable;
    bool action;
    const char *input_contract;
    const char *output_contract;
    const char *threading;
} HhyCallableContract;

const HhyCallableContract *hhy_contract_lookup(const char *name);
const HhyCallableContract *hhy_contract_lookup_n(const char *name, size_t length);
const char *hhy_effect_name(HhyEffect effect);
size_t hhy_contract_count(void);
const HhyCallableContract *hhy_contract_at(size_t index);
bool hhy_contract_registry_valid(void);
bool hhy_contract_namespace_installed(const char *name, size_t length);

#endif

#ifndef HHY_FUZZ_H
#define HHY_FUZZ_H

#include <stddef.h>
#include <stdint.h>

void hhy_fuzz_runtime_input(const uint8_t *data, size_t size, unsigned mode);

#endif

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ItchTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

int itch_parse(const uint8_t *data, size_t size, itch_event_t *out);

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdint.h>

#define ITCH_SYMBOL_MAX_LEN 8

typedef enum
{
    ITCH_ADD_ORDER,
    ITCH_EXECUT,
    ITCH_CANCEL,
    ITCH_DELETE,
    ITCH_REPLACE,
    ITCH_TRADE,

    ITCH_UNKNOWN
} itch_event_type_t;

typedef enum
{
    ITCH_SIDE_UNKNOWN = 0,
    ITCH_SIDE_BUY = 'B',
    ITCH_SIDE_SELL = 'S'
} itch_side_indicator_t;

typedef struct
{
    itch_event_type_t type;

    uint16_t stock_locate;
    uint16_t tracking_number;

    uint64_t timestamp_ns;
    uint64_t order_id;

    char symbol[ITCH_SYMBOL_MAX_LEN + 1];
    uint32_t quantity;
    uint64_t price;
    itch_side_indicator_t side;
} itch_event_t;

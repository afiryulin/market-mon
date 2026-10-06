#include <string.h>

#include "ItchParser.h"


static uint16_t read_u16_be(const uint8_t* p)
{
    return ((uint16_t)p[0] << 8) |
           ((uint16_t)p[1]);
}

static uint32_t read_u32_be(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           ((uint32_t)p[3]);
}

static uint64_t read_u48_be(const uint8_t* p)
{
    return ((uint64_t)p[0] << 40) |
           ((uint64_t)p[1] << 32) |
           ((uint64_t)p[2] << 24) |
           ((uint64_t)p[3] << 16) |
           ((uint64_t)p[4] << 8)  |
           ((uint64_t)p[5]);
}

static uint64_t read_u64_be(const uint8_t* p)
{
    return ((uint64_t)p[0] << 56) |
           ((uint64_t)p[1] << 48) |
           ((uint64_t)p[2] << 40) |
           ((uint64_t)p[3] << 32) |
           ((uint64_t)p[4] << 24) |
           ((uint64_t)p[5] << 16) |
           ((uint64_t)p[6] << 8)  |
           ((uint64_t)p[7]);
}


int itch_parse(const uint8_t *data, size_t size, itch_event_t *out)
{
    if(!data || !out || size == 0)
    {
        return -1;
    }


    memset(out, 0, sizeof(*out));

    out->type = ITCH_ADD_ORDER;
    out->stock_locate = read_u16_be(data + 1);
    out->tracking_number = read_u16_be(data + 3);


    return -1;
}

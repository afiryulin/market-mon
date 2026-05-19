#pragma once

#include <cstdint>

struct TradeNotification
{
    uint32_t clientId;
    uint64_t orderId;
    char symbol[32];
    double fillPrice;
    uint32_t fillQuantity;
    bool isFullFill;
};

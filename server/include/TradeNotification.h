#pragma once

#include <array>
#include <cstdint>
#include <spdlog/spdlog.h>

struct TradeNotification
{
    uint32_t clientId;
    uint64_t orderId;
    char symbol[32];
    double fillPrice;
    uint32_t fillQuantity;
    bool isFullFill;
};

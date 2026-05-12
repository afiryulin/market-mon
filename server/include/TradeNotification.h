#pragma once

#include <array>
#include <cstdint>
#include <spdlog/spdlog.h>

#include "SPSCQueue.h"

struct TradeNotification
{
    uint32_t clientId;
    uint64_t orderId;
    double fillPrice;
    uint32_t fillQuantity;
    bool isFullFill;
};

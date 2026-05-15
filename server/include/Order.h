#pragma once

#include <atomic>
#include <cstdint>
#include <string>

enum class eOrderType : uint8_t
{
    LIMIT,
    MARKET
};

enum class eSide : uint8_t
{
    BUY,
    SELL
};

struct alignas(64) Order final
{
    alignas(64) std::atomic<uint32_t> nextIdx{0}; // 64
    uint64_t orderId;                             // 8
    double price;                                 // 8
    int64_t timestamp;                            // 8
    char symbol[32];                              // 8
    uint32_t clientId;                            // 4
    uint32_t quantity;                            // 4
    eSide side;                                   // 4
    eOrderType orderType;                         // 4
    uint8_t responseThreadIdx;                    // 1
};

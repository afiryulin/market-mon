#pragma once

#include <cstdint>
#include <string_view>

enum class eTradeNotificationType : uint8_t
{
    ACCEPTED,
    FILLED,
    PARTIALLY_FILLED,
    CANCELLED,
    CANCELLED_REJECTED,
    REJECTED
};

struct TradeNotification
{
    eTradeNotificationType type{eTradeNotificationType::CANCELLED_REJECTED};
    uint32_t clientId{};
    uint64_t orderId{};
    char symbol[32]{};
    double price{};
    uint32_t quantity{};
    bool isFullFill{};

    static const std::string_view TypeToString(eTradeNotificationType type)
    {
        switch (type)
        {
        case eTradeNotificationType::ACCEPTED:
            return "ACCEPTED";
            break;
        case eTradeNotificationType::FILLED:
            return "FILLED";
            break;
        case eTradeNotificationType::PARTIALLY_FILLED:
            return "PARTIALLY_FILLED";
            break;
        case eTradeNotificationType::CANCELLED:
            return "CANCELLED";
            break;
        case eTradeNotificationType::CANCELLED_REJECTED:
            return "CANCELLED_REJECTED";
            break;
        case eTradeNotificationType::REJECTED:
            return "REJECTED";
            break;
        default:
            return "UNKNOWN";
            break;
        }

        return "UNKNOWN";
    }
};

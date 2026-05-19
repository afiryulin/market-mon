#pragma once

#include <cstdint>
#include <string_view>

enum class eEngineCommandType : uint8_t
{
    NEW_ORDER,
    CANCEL_ORDER
};

struct EngineCommand final
{
    eEngineCommandType type{eEngineCommandType::NEW_ORDER};

    uint32_t orderIdx{};
    uint64_t orderId{};
    uint32_t clientId{};
    uint8_t responseThreadIdx{};
    char symbol[32]{};
    double price{};

    static const std::string_view TypeToString(const eEngineCommandType type)
    {
        switch (type)
        {
        case eEngineCommandType::CANCEL_ORDER:
            return "CANCEL_ORDER";
            break;

        case eEngineCommandType::NEW_ORDER:
            return "NEW_ORDER";
            break;

        default:
            return "UNKNOWN";
            break;
        }
        return "UNKNOWN";
    }
};

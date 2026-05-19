#pragma once
#include <concepts>
#include <grpcpp/grpcpp.h>
#include <string>
#include <string_view>
#include <unordered_map>

#include "TradeNotification.h"

enum class eCallDataAction : uint8_t
{
    CONNECT,
    READ,
    WRITE,
    FINISH
};

enum class eCallDataKind : uint8_t
{
    UNKNOWN,
    TRADE,
    SUBSCRIBE_PRICE,
    GET_PRICE
};

#define REGISTER_CALL_TYPE(ClassName, KindValue)                                                                       \
    const char *GetTypeName() const override { return #ClassName; }                                                    \
    eCallDataKind GetKind() const override { return KindValue; }

class ICallDataBase;
struct CallDataTag
{
    ICallDataBase *parent;
    eCallDataAction actionType;

    static const std::string_view ToString(const eCallDataAction act)
    {
        switch (act)
        {
        case eCallDataAction::CONNECT:
            return "CONNECT";
            break;
        case eCallDataAction::READ:
            return "READ";
            break;
        case eCallDataAction::WRITE:
            return "WRITE";
            break;
        case eCallDataAction::FINISH:
            return "FINISH";
            break;
        default:
            return "UNKNOWN";
        }

        return "UNKNOWN";
    }
};

class ICallDataBase
{
public:
    virtual void ProcessData(CallDataTag *tag, bool ok) = 0;
    virtual ~ICallDataBase() = default;
    virtual const char *GetTypeName() const = 0;
    virtual eCallDataKind GetKind() const = 0;
};

template <typename T, typename Service>
concept IsCallData =
    std::derived_from<T, ICallDataBase> && requires(Service *srv, grpc::ServerCompletionQueue *cq) { new T(srv, cq); };

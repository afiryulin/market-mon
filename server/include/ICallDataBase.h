#pragma once
#include <concepts>
#include <grpcpp/grpcpp.h>
#include <string>
#include <string_view>
#include <unordered_map>

enum class eCallDataAction
{
    CONNECT,
    READ,
    WRITE,
    FINISH
};

#define REGISTER_CALL_TYPE(ClassName)                                                              \
    const char *GetTypeName() const override { return #ClassName; }

class ICallDataBase;
struct CallDataTag
{
    ICallDataBase *mParent;
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
};

template <typename T, typename Service>
concept IsCallData = std::derived_from<T, ICallDataBase> &&
                     requires(Service *srv, grpc::ServerCompletionQueue *cq) { new T(srv, cq); };

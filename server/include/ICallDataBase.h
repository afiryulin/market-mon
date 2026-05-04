#pragma once
#include <memory>
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
    static const std::string &ToString(const eCallDataAction act)
    {
        static std::unordered_map<eCallDataAction, std::string> values{
            {eCallDataAction::CONNECT, "CONNECT"},
            {eCallDataAction::READ, "READ"},
            {eCallDataAction::WRITE, "WRITE"},
            {eCallDataAction::FINISH, "FINISH"},
        };

        return values[act];
    }
};

class ICallDataBase
{
public:
    virtual void ProcessData(CallDataTag *tag, bool ok) = 0;
    virtual ~ICallDataBase() = default;

    virtual const char *GetTypeName() const = 0;
};

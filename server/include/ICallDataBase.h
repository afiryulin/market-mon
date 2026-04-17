#pragma once

class ICallDataBase
{
public:
    virtual void ProcessData(bool ok) = 0;
    virtual ~ICallDataBase() = default;
};
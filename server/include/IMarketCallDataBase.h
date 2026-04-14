#pragma once

class IMarketCallDataBase
{
public:
    virtual void ProcessData(bool ok) = 0;
    virtual ~IMarketCallDataBase() = default;
};
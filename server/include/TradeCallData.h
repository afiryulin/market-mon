#pragma once

#include <grpcpp/grpcpp.h>

#include "ICallDataBase.h"
#include "market/v1/market.grpc.pb.h"
#include "market/v1/market.pb.h"

class TradeCallData : public ICallDataBase
{
public:
    TradeCallData(market::v1::MarketService::AsyncService *service, grpc::ServerCompletionQueue *completionQueue);
    void ProcessData(bool ok) override;

private:
    enum class eState
    {
        CREATE,
        CONNECTED,
        WAIT_READ,
        WAIT_WRITE,
        FINISH
    };

    eState mState = eState::CREATE;

    market::v1::MarketService::AsyncService *mService;
    grpc::ServerCompletionQueue *mCompletionQueue;

    grpc::ServerContext mContext;

    std::unique_ptr<
        grpc::ServerAsyncReaderWriter<
            market::v1::TradeEvent,
            market::v1::TradeRequest>>
        mStream;

    market::v1::TradeRequest mRequest;
    market::v1::TradeEvent mResponse;
};
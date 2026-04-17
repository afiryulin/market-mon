#pragma once

#include <queue>

#include "market/v1/market.grpc.pb.h"
#include "ICallDataBase.h"

class SubscribePriceCallData final : public ICallDataBase
{
public:
    SubscribePriceCallData(
        market::v1::MarketService::AsyncService * /*service*/,
        grpc::ServerCompletionQueue * /*completionQueue*/);

    void ProcessData(bool ok) override;
    void PushPrice(const std::string &symbol, double value);

private:
    void ProcessQueue();
    enum class eState
    {
        CREATE,
        PROCESS,
        WRITE,
        FINISH
    };

    eState mState = eState::CREATE;
    market::v1::MarketService::AsyncService *mService;
    grpc::ServerCompletionQueue *mCompletionQueue;

    grpc::ServerContext mContext;
    market::v1::SubscribeRequest mRequest;
    market::v1::PriceUpdate mResponse;

    std::unique_ptr<grpc::ServerAsyncWriter<market::v1::PriceUpdate>> mPriceWriter;
    std::mutex mWriteMutex;
    std::atomic<bool> mWriteInProgress{false};
    std::queue<market::v1::PriceUpdate> mUpdateQueue;
};
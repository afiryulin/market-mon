#pragma once

#include "market/v1/market.grpc.pb.h"
#include "IMarketCallDataBase.h"

class SubscribePriceCallData final : public IMarketCallDataBase
{
public:
    SubscribePriceCallData(
        market::v1::MarketService::AsyncService * /*service*/,
        grpc::ServerCompletionQueue * /*completionQueue*/);
    void ProcessData(bool ok) override;

private:
    void SendPrice();
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
    std::mutex mMutex;
    bool mWriteInProgress{false};
};
#pragma once

#include <queue>

#include "ICallDataBase.h"
#include "market/v1/market.grpc.pb.h"

class SubscribePriceCallData final : public ICallDataBase
{
public:
    REGISTER_CALL_TYPE(SubscribePriceCallData)

    SubscribePriceCallData(market::v1::MarketService::AsyncService * /*service*/,
                           grpc::ServerCompletionQueue * /*completionQueue*/);

    void ProcessData(CallDataTag *tag, bool ok) override;
    void PushPrice(const std::string &symbol, double value);

private:
    void ProcessQueue();

    market::v1::MarketService::AsyncService *mService;
    grpc::ServerCompletionQueue *mCompletionQueue;

    grpc::ServerContext mContext;
    market::v1::SubscribeRequest mRequest;
    market::v1::PriceUpdate mResponse;

    std::unique_ptr<grpc::ServerAsyncWriter<market::v1::PriceUpdate>> mPriceWriter;
    std::mutex mWriteMutex;
    std::atomic<bool> mWriteInProgress{false};
    std::atomic<bool> mIsFinished{false};
    std::queue<market::v1::PriceUpdate> mUpdateQueue;

    CallDataTag mCreateTag{this, eCallDataAction::CONNECT};
    // CallDataTag mReadTag{this, eCallDataAction::READ};
    CallDataTag mWriteTag{this, eCallDataAction::WRITE};
    CallDataTag mFinishTag{this, eCallDataAction::FINISH};
};

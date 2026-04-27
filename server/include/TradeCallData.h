#pragma once

#include <queue>
#include <mutex>
#include <atomic>
#include <grpcpp/grpcpp.h>

#include "ICallDataBase.h"
#include "market/v1/market.grpc.pb.h"
#include "market/v1/market.pb.h"

using namespace market::v1;
using namespace grpc;

class TradeCallData : public ICallDataBase
{

public:
    TradeCallData(MarketService::AsyncService *service, ServerCompletionQueue *completionQueue);

    void ProcessData(bool ok) override;

private:
    enum class eState
    {
        CREATE,
        CONNECTED,
        READ,
        WRITE,
        FINISH
    };

private:
    void StartRead();
    void EnqueueResponse(const TradeEvent &response);
    void TryWriteNext();
    void Finish();

private:
    eState mState = eState::CREATE;
    MarketService::AsyncService *mService{};
    ServerCompletionQueue *mCompletionQueue{};
    ServerContext mContext;

    std::unique_ptr<ServerAsyncReaderWriter<TradeEvent, TradeRequest>> mStream;

    TradeRequest mRequest;
    TradeEvent mResponse;

    std::mutex mWriteMutex;
    std::queue<TradeEvent> mWriteQueue;

    std::atomic<bool> mIsWriting{false};
    std::atomic<bool> mIsFinished{false};
};
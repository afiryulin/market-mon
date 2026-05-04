#pragma once

#include <atomic>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>

#include "ICallDataBase.h"
#include "market/v1/market.grpc.pb.h"
#include "market/v1/market.pb.h"

using namespace market::v1;
using namespace grpc;

class TradeCallData : public ICallDataBase
{
public:
    REGISTER_CALL_TYPE(TradeCallData)

    TradeCallData(MarketService::AsyncService *service, ServerCompletionQueue *completionQueue);

    void ProcessData(CallDataTag *tag, bool ok) override;

private:
    void StartRead();
    void EnqueueResponse(const TradeEvent &response);
    void TryWriteNext();
    void Finish();

private:
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
    std::atomic<int> mActiveOps{0};

    CallDataTag mConnectTag{this, eCallDataAction::CONNECT};
    CallDataTag mReadTag{this, eCallDataAction::READ};
    CallDataTag mWriteTag{this, eCallDataAction::WRITE};
    CallDataTag mFinishTag{this, eCallDataAction::FINISH};
};

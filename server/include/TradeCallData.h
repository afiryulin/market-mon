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
    void HandleConnect(bool ok);
    void HandleRead(bool ok);
    void HandleWrite(bool ok);
    void HandleFinish();
    void StartRead();
    void EnqueueResponse(const TradeEvent &response);
    void TryWriteNext();
    void TryDelete();
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
    std::atomic<bool> mReadClosed{false};
    std::atomic<bool> mFinishStarted{false};
    std::atomic<bool> mFinishCompleted{false};
    std::atomic<bool> mDeleteStarted{false};
    std::atomic<int> mActiveOps{0};

    CallDataTag mConnectTag{this, eCallDataAction::CONNECT};
    CallDataTag mReadTag{this, eCallDataAction::READ};
    CallDataTag mWriteTag{this, eCallDataAction::WRITE};
    CallDataTag mFinishTag{this, eCallDataAction::FINISH};
};

#include <memory>

#include "../include/TradeCallData.h"
#include "spdlog/spdlog.h"

TradeCallData::TradeCallData(MarketService::AsyncService *service,
                             ServerCompletionQueue *completionQueue)
    : mService(service), mCompletionQueue(completionQueue), mContext{}, mRequest{}, mResponse{}
{
    mStream = std::make_unique<ServerAsyncReaderWriter<TradeEvent, TradeRequest>>(&mContext);

    mActiveOps.fetch_add(1, std::memory_order_relaxed);
    mService->RequestTradeStream(&mContext, mStream.get(), mCompletionQueue, mCompletionQueue,
                                 &mConnectTag);
}

void TradeCallData::ProcessData(CallDataTag *tag, bool ok)
{
    if (!ok || mIsFinished.load(std::memory_order_relaxed))
    {
        Finish();
        return;
    }

    if (eCallDataAction::CONNECT == tag->actionType)
    {
        spdlog::info("TradeStream connected: {}", mContext.peer());
        new TradeCallData(mService, mCompletionQueue);
        StartRead();
    }

    if (eCallDataAction::READ == tag->actionType)
    {
        spdlog::info("Trade Order: {} {} {}", mRequest.symbol(), mRequest.quantity(),
                     mRequest.is_buy() ? "BUY" : "SELL");

        TradeEvent response{};
        response.set_symbol(mRequest.symbol());
        response.set_price(1000 + rand() % 100);
        response.set_status("ACCEPTED");

        EnqueueResponse(response);
        StartRead();
    }

    if (eCallDataAction::WRITE == tag->actionType)
    {
        {
            std::lock_guard<std::mutex> lockWriter(mWriteMutex);
            mIsWriting.store(false, std::memory_order_relaxed);
        }

        TryWriteNext();
    }

    if (mActiveOps.load(std::memory_order_acquire) == 0)
        delete this;
}

void TradeCallData::StartRead()
{
    if (mIsFinished.load(std::memory_order_relaxed))
        return;

    mActiveOps.fetch_add(1, std::memory_order_relaxed);
    mStream->Read(&mRequest, &mReadTag);
}

void TradeCallData::EnqueueResponse(const TradeEvent &response)
{
    {
        std::lock_guard<std::mutex> lock(mWriteMutex);
        mWriteQueue.push(response);
    }

    TryWriteNext();
}

void TradeCallData::TryWriteNext()
{
    std::lock_guard<std::mutex> locker(mWriteMutex);

    if (mIsFinished.load(std::memory_order_relaxed) || mIsWriting.load(std::memory_order_relaxed) ||
        mWriteQueue.empty())
    {
        return;
    }

    mResponse = std::move(mWriteQueue.front());
    mWriteQueue.pop();

    mActiveOps.fetch_add(1, std::memory_order_relaxed);
    mIsWriting.store(true, std::memory_order_relaxed);
    mStream->Write(mResponse, &mWriteTag);
}

void TradeCallData::Finish()
{
    if (mIsFinished.exchange(true, std::memory_order_seq_cst))
        return;

    spdlog::info("TradeStream disconnected: {}", mContext.peer());
    mActiveOps.fetch_add(1, std::memory_order_relaxed);
    mStream->Finish(Status::OK, &mFinishTag);
}

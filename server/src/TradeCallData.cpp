#include <memory>

#include "../include/TradeCallData.h"
#include "spdlog/spdlog.h"

TradeCallData::TradeCallData(MarketService::AsyncService *service,
                             ServerCompletionQueue *completionQueue)
    : mService(service), mCompletionQueue(completionQueue), mContext{}, mRequest{}, mResponse{}
{
    mStream = std::make_unique<ServerAsyncReaderWriter<TradeEvent, TradeRequest>>(&mContext);

    mService->RequestTradeStream(&mContext, mStream.get(), mCompletionQueue, mCompletionQueue,
                                 &mConnectTag);
}

void TradeCallData::ProcessData(CallDataTag *tag, bool ok)
{
    // if (!ok || mIsFinished.load(std::memory_order_relaxed))
    // {
    //     Finish();
    //     return;
    // }

    if (eCallDataAction::CONNECT == tag->actionType)
    {
        if (!ok)
        {
            delete this;
            return;
        }

        spdlog::info("TradeStream connected: {}", mContext.peer());
        new TradeCallData(mService, mCompletionQueue);
        StartRead();
    }

    if (eCallDataAction::READ == tag->actionType)
    {
        if (!ok)
        {
            spdlog::info("TradeSteam read closed: {}", mContext.peer());
            mReadClosed.store(true, std::memory_order_release);
            TryWriteNext();
            Finish();
            return;
        }

        spdlog::info("Trade Order: {} {} {}", mRequest.symbol(), mRequest.quantity(),
                     mRequest.is_buy() ? "BUY" : "SELL");

        TradeEvent response{};
        response.set_symbol(mRequest.symbol());
        response.set_price(1000 + rand() % 100);
        response.set_status("ACCEPTED");

        EnqueueResponse(response);
        StartRead();
        return;
    }

    if (eCallDataAction::WRITE == tag->actionType)
    {
        if (!ok)
        {
            spdlog::warn("TradeStream write failed: {}", mContext.peer());
            mReadClosed.store(true, std::memory_order_release);
            mIsWriting.store(false, std::memory_order_release);
            Finish();
            return;
        }

        {
            std::lock_guard<std::mutex> lockWriter(mWriteMutex);
            mIsWriting.store(false, std::memory_order_relaxed);
        }

        TryWriteNext();
        Finish();
        return;
    }

    if (eCallDataAction::FINISH == tag->actionType)
    {
        spdlog::info("TradeStream finished: {}", mContext.peer());
        delete this;
        return;
    }
}

void TradeCallData::StartRead()
{
    if (mReadClosed.load(std::memory_order_acquire) ||
        mFinishStartd.load(std::memory_order_acquire))
    {
        return;
    }

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

    if (mFinishStartd.load(std::memory_order_acquire) ||
        mIsWriting.load(std::memory_order_acquire) || mWriteQueue.empty())
    {
        return;
    }

    mResponse = std::move(mWriteQueue.front());
    mWriteQueue.pop();

    mIsWriting.store(true, std::memory_order_relaxed);
    mStream->Write(mResponse, &mWriteTag);
}

void TradeCallData::Finish()
{
    if (!mReadClosed.load(std::memory_order_acquire))
        return;

    {
        std::lock_guard<std::mutex> lock(mWriteMutex);

        if (mIsWriting.load(std::memory_order_acquire) || !mWriteQueue.empty())
            return;
    }

    if (mFinishStartd.exchange(true, std::memory_order_acq_rel))
        return;

    spdlog::info("TradeStream disconnected: {}", mContext.peer());
    mStream->Finish(Status::OK, &mFinishTag);
}

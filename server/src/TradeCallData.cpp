#include <memory>
#include <random>

#include "../include/TradeCallData.h"
#include "spdlog/spdlog.h"

TradeCallData::TradeCallData(MarketService::AsyncService *service,
                             ServerCompletionQueue *completionQueue)
    : mService(service), mCompletionQueue(completionQueue), mContext{}, mRequest{}, mResponse{}
{
    mStream = std::make_unique<ServerAsyncReaderWriter<TradeEvent, TradeRequest>>(&mContext);

    mActiveOps.fetch_add(1, std::memory_order_acq_rel);
    mService->RequestTradeStream(&mContext, mStream.get(), mCompletionQueue, mCompletionQueue,
                                 &mConnectTag);
}

void TradeCallData::ProcessData(CallDataTag *tag, bool ok)
{
    mActiveOps.fetch_sub(1, std::memory_order_acq_rel);

    switch (tag->actionType)
    {
    case eCallDataAction::CONNECT:
        HandleConnect(ok);
        break;
    case eCallDataAction::READ:
        HandleRead(ok);
        break;
    case eCallDataAction::WRITE:
        HandleWrite(ok);
        break;
    case eCallDataAction::FINISH:
        HandleFinish();
        break;
    default:
        TryDelete();
        break;
    }

    TryDelete();
}

void TradeCallData::HandleConnect(bool ok)
{
    if (!ok)
    {
        mFinishCompleted.store(true, std::memory_order_release);
    }

    spdlog::info("TradeStream connected: {}", mContext.peer());
    new TradeCallData(mService, mCompletionQueue);
    StartRead();
}

void TradeCallData::HandleRead(bool ok)
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

    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<double> distr(1000.0, 1100.0);

    TradeEvent response{};
    response.set_symbol(mRequest.symbol());
    response.set_price(distr(gen));
    response.set_status("ACCEPTED");

    EnqueueResponse(response);
    StartRead();
    return;
}

void TradeCallData::HandleWrite(bool ok)
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

    if (mReadClosed.load(std::memory_order_acquire))
    {
        TryWriteNext();
        Finish();
    }
    else
    {
        TryWriteNext();
    }
}

void TradeCallData::HandleFinish() { mFinishCompleted.store(true, std::memory_order_release); }

void TradeCallData::TryDelete()
{
    if (!mFinishCompleted.load(std::memory_order_acquire) ||
        mActiveOps.load(std::memory_order_acquire) != 0)
    {
        return;
    }

    if (mDeleteStarted.exchange(true, std::memory_order_acq_rel))
        return;

    delete this;
}

void TradeCallData::StartRead()
{
    if (mFinishStarted.load(std::memory_order_acquire) ||
        mReadClosed.load(std::memory_order_acquire))
    {
        return;
    }

    mActiveOps.fetch_add(1, std::memory_order_acq_rel);
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

    if (mFinishStarted.load(std::memory_order_acquire) ||
        mIsWriting.load(std::memory_order_acquire) || mWriteQueue.empty())
    {
        return;
    }

    mResponse = std::move(mWriteQueue.front());
    mWriteQueue.pop();

    mIsWriting.store(true, std::memory_order_relaxed);

    mActiveOps.fetch_add(1, std::memory_order_acq_rel);
    mStream->Write(mResponse, &mWriteTag);
}

void TradeCallData::Finish()
{
    if (!mReadClosed.load(std::memory_order_acquire))
        return;

    {
        std::lock_guard<std::mutex> locker(mWriteMutex);
        if (mIsWriting.load(std::memory_order_acquire) || !mWriteQueue.empty())
            return;
    }

    if (mFinishStarted.exchange(true, std::memory_order_acq_rel))
        return;

    spdlog::info("TradeStream disconnected: {}", mContext.peer());

    mActiveOps.fetch_add(1, std::memory_order_acq_rel);
    mStream->Finish(Status::OK, &mFinishTag);
}

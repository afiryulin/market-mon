#include <memory>
#include <random>

#include "../include/MatchingEngine.h"
#include "../include/TradeCallData.h"
#include "spdlog/spdlog.h"

TradeCallData::TradeCallData(MarketService::AsyncService *service, ServerCompletionQueue *completionQueue)
    : mService(service), mCompletionQueue(completionQueue), mContext{}, mRequest{}, mResponse{}
{
    mStream = std::make_unique<ServerAsyncReaderWriter<TradeEvent, TradeRequest>>(&mContext);

    mActiveOps.fetch_add(1, std::memory_order_acq_rel);
    mService->RequestTradeStream(&mContext, mStream.get(), mCompletionQueue, mCompletionQueue, &mConnectTag);
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

void TradeCallData::OnTradeNotify(const TradeNotification &note)
{
    TradeEvent fillNotify;
    fillNotify.set_clientid(note.clientId);
    fillNotify.set_orderid(note.orderId);
    fillNotify.set_symbol(note.symbol);
    fillNotify.set_price(note.fillPrice);
    fillNotify.set_quantity(note.fillQuantity);
    fillNotify.set_status(note.isFullFill ? "FILLED" : "PARTIALLY_FILLED");
    fillNotify.set_is_fully_filled(note.isFullFill);

    EnqueueResponse(std::move(fillNotify));
}

uint32_t TradeCallData::GetClientId() const { return mClientId; }
void TradeCallData::SetResponseThreadIdx(uint8_t index) { mResponseThreadIdx = index; }
void TradeCallData::RegisterSessionFromCurrentRequest()
{
    if (mSessionRegistered.exchange(true, std::memory_order_acquire))
        return;

    mClientId = mRequest.clientid();
}

void TradeCallData::HandleConnect(bool ok)
{
    if (!ok)
    {
        mFinishCompleted.store(true, std::memory_order_release);
        return;
    }

    spdlog::info("TradeStream connected: {}", mContext.peer());
    new TradeCallData(mService, mCompletionQueue);
    StartRead();
}

void TradeCallData::HandleRead(bool ok)
{
    spdlog::info("TradeCallData::HandleRead ok=={0}", (ok ? "true" : "false"));
    if (!ok)
    {
        spdlog::info("TradeSteam read closed: {}", mContext.peer());
        mReadClosed.store(true, std::memory_order_release);
        TryWriteNext();
        Finish();
        return;
    }

    if (!mSessionRegistered.exchange(true, std::memory_order_acq_rel))
        mClientId = mRequest.clientid();

    auto &engine = MatchingEngine::Instance();
    auto &pool = engine.GetPoll();

    uint32_t orderIdx = pool.Allocate();
    Order &order = pool.Get(orderIdx);

    order.orderId = mRequest.orderid();
    order.clientId = mClientId;
    order.responseThreadIdx = mResponseThreadIdx;
    order.price = mRequest.price();
    order.quantity = mRequest.quantity();
    order.side = mRequest.is_buy() ? eSide::BUY : eSide::SELL;
    order.orderType = mRequest.is_market_order() ? eOrderType::MARKET : eOrderType::LIMIT;
    std::strncpy(order.symbol, mRequest.symbol().c_str(), sizeof(order.symbol) - 1);
    order.symbol[sizeof(order.symbol) - 1] = '\0';

    engine.SubmitOrder(orderIdx);

    spdlog::info("Trade Order: {} {} {}", mRequest.symbol(), mRequest.quantity(), mRequest.is_buy() ? "BUY" : "SELL");

    TradeEvent accepted{};
    accepted.set_clientid(mRequest.clientid());
    accepted.set_symbol(mRequest.symbol());
    accepted.set_quantity(mRequest.quantity());
    accepted.set_price(mRequest.price());
    accepted.set_status("ACCEPTED");

    EnqueueResponse(std::move(accepted));
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
    if (!mFinishCompleted.load(std::memory_order_acquire) || mActiveOps.load(std::memory_order_acquire) != 0)
    {
        return;
    }

    if (mDeleteStarted.exchange(true, std::memory_order_acq_rel))
        return;

    delete this;
}

void TradeCallData::StartRead()
{
    if (mFinishStarted.load(std::memory_order_acquire) || mReadClosed.load(std::memory_order_acquire))
    {
        return;
    }

    mActiveOps.fetch_add(1, std::memory_order_acq_rel);
    mStream->Read(&mRequest, &mReadTag);
}

void TradeCallData::EnqueueResponse(TradeEvent response)
{
    {
        std::lock_guard<std::mutex> lock(mWriteMutex);
        mWriteQueue.push(std::move(response));
    }

    TryWriteNext();
}

void TradeCallData::TryWriteNext()
{
    std::lock_guard<std::mutex> locker(mWriteMutex);

    if (mFinishStarted.load(std::memory_order_acquire) || mIsWriting.load(std::memory_order_acquire) ||
        mWriteQueue.empty())
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

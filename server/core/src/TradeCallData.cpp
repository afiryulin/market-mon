#include <memory>
#include <random>

#include "../include/MatchingEngine.h"
#include "../include/TradeCallData.h"
#include "TradeCallData.h"
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
    fillNotify.set_price(note.price);
    fillNotify.set_quantity(note.quantity);
    fillNotify.set_status(TradeNotification::TypeToString(note.type));
    fillNotify.set_is_fully_filled(note.isFullFill);

    EnqueueResponse(std::move(fillNotify));
}

uint32_t TradeCallData::GetClientId() const { return mClientId; }

void TradeCallData::SetResponseThreadIdx(uint8_t index) { mResponseThreadIdx = index; }

bool TradeCallData::RegisterSessionFromCurrentRequest()
{

    uint32_t incomingClientId = mRequest.clientid();
    spdlog::info("RegisterSession check: registered={} stored={} incoming={}",
                 mSessionRegistered.load(std::memory_order_acquire), mClientId, incomingClientId);

    if (!mSessionRegistered.exchange(true, std::memory_order_acq_rel))
    {
        mClientId = incomingClientId;
        spdlog::info("Register session client={}", incomingClientId);

        return true;
    }

    return mClientId == incomingClientId;
}

bool TradeCallData::IsValidateSessionFromCurrentRequest() const
{
    return mSessionRegistered.load(std::memory_order_acquire) && mClientId == mRequest.clientid();
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

    const bool sessionOk = IsValidateSessionFromCurrentRequest();

    spdlog::info("HandleRead sessionOk={} requestClient={} storedClient={}", sessionOk, mRequest.clientid(), mClientId);

    if (!sessionOk)
    {
        TradeEvent ev;
        ev.set_symbol(mRequest.symbol());
        ev.set_price(mRequest.price());
        ev.set_clientid(mRequest.clientid());
        ev.set_orderid(mRequest.orderid());
        ev.set_quantity(mRequest.quantity());
        ev.set_status("REJECTED_INVALID_SESSION_CLIENT");
        EnqueueResponse(std::move(ev));
        StartRead();

        return;
    }

    if (mRequest.type() == market::v1::CANCEL_ORDER)
    {
        EngineCommand cmd{};
        cmd.type = eEngineCommandType::CANCEL_ORDER;
        cmd.orderId = mRequest.orderid();
        cmd.clientId = mClientId;
        cmd.responseThreadIdx = mResponseThreadIdx;
        cmd.price = mRequest.price();

        std::strncpy(cmd.symbol, mRequest.symbol().c_str(), sizeof(cmd.symbol) - 1);

        MatchingEngine::Instance().SubmitCancel(cmd);

        StartRead();
        return;
    }

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
    StartRead();
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

    if (mFinishStarted.load(std::memory_order_acquire) || mIsWriting.load(std::memory_order_relaxed) ||
        mWriteQueue.empty())
    {
        return;
    }

    mResponse = std::move(mWriteQueue.front());
    mWriteQueue.pop();

    spdlog::info("Write TradeEvent client={} order={} status={}", mResponse.clientid(), mResponse.orderid(),
                 mResponse.status());

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
        if (mIsWriting.load(std::memory_order_relaxed) || !mWriteQueue.empty())
            return;
    }

    if (mFinishStarted.exchange(true, std::memory_order_acq_rel))
        return;

    spdlog::info("TradeStream disconnected: {}", mContext.peer());

    mActiveOps.fetch_add(1, std::memory_order_acq_rel);
    mStream->Finish(Status::OK, &mFinishTag);
}

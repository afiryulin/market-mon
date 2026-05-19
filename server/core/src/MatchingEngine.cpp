#include <random>
#include <spdlog/spdlog.h>

#include "../include/MatchingEngine.h"
#include "../include/TradeNotificationDispatcher.h"
#include "MatchingEngine.h"

MatchingEngine::MatchingEngine() : mOrders{1000000} { mHeadIdx.store(0); }

MatchingEngine &MatchingEngine::Instance()
{
    static MatchingEngine inst;
    return inst;
}

void MatchingEngine::SubmitOrder(uint32_t orderIdx)
{

    Order &order = mOrders.Get(orderIdx);
    spdlog::info("SubmitOrder idx={} client={} order={} {} {} qty={} price={} type={}", orderIdx, order.clientId,
                 order.orderId, order.symbol, order.side == eSide::BUY ? "BUY" : "SELL", order.quantity, order.price,
                 order.orderType == eOrderType::LIMIT ? "LIMIT" : "MARKET");

    EngineCommand cmd{};
    cmd.type = eEngineCommandType::NEW_ORDER;
    cmd.orderIdx = orderIdx;
    cmd.clientId = order.clientId;
    cmd.orderId = order.orderId;
    cmd.price = order.price;
    cmd.responseThreadIdx = order.responseThreadIdx;
    std::strncpy(cmd.symbol, order.symbol, sizeof(cmd.symbol) - 1);

    if (!mPendingCommands.Push(std::move(cmd)))
    {
        spdlog::critical("MatchingEngine command queue overflow");
        std::terminate();
    }
}

void MatchingEngine::SubmitCancel(const EngineCommand &command)
{
    spdlog::info("SubmitCancel client={} order={} symbol={} threadIdx={}", command.clientId, command.orderId,
                 command.symbol, command.responseThreadIdx);

    if (!mPendingCommands.Push(command))
    {
        spdlog::critical("MatchingEngine command queue overflow");
        std::terminate();
    }
}

OrderPool &MatchingEngine::GetPoll() { return mOrders; }

void MatchingEngine::Start()
{
    spdlog::info("MatchingEngine::Start running={}", mRunning.load());
    if (mRunning.exchange(true, std::memory_order_acq_rel))
        return;

    mThread = std::jthread([this](std::stop_token st) { Run(st); });
}

void MatchingEngine::Stop()
{
    spdlog::info("MatchingEngine::Stop begin");

    if (!mRunning.exchange(false, std::memory_order_acq_rel))
    {
        spdlog::info("MatchingEngine::Stop skipped");
        return;
    }

    if (mThread.joinable())
    {
        spdlog::info("MatchingEngine::Stop request_stop");
        mThread.request_stop();

        spdlog::info("MatchingEngine::Stop join begin");
        mThread.join();
        spdlog::info("MatchingEngine::Stop join end");
    }
}

void MatchingEngine::ResetForTesting()
{
    Stop();
    mPendingCommands.Reset();
    mOrderBook.clear();
    mOrderIndex.clear();
    mOrders.Reset();

    mRunning.store(false, std::memory_order_release);
}

void MatchingEngine::Run(std::stop_token stop)
{
    spdlog::info("MatchingEngine::Run begin");

    while (!stop.stop_requested() && mRunning.load(std::memory_order_acquire))
    {
        auto command = mPendingCommands.Pop();

        if (!command)
        {
            std::this_thread::yield();
            continue;
        }

        spdlog::info("MatchingEngine::Run {} id={} client={} idx={}", EngineCommand::TypeToString(command->type),
                     command->orderId, command->clientId, command->orderIdx);

        switch (command->type)
        {
        case eEngineCommandType::NEW_ORDER: {
            Order &order = mOrders.Get(command->orderIdx);
            ProcessOrder(order, command->orderIdx);
            break;
        }
        case eEngineCommandType::CANCEL_ORDER:
            ProcessCancel(*command);
            break;
        }
    }

    spdlog::info("MatchingEngine::Run end");
}

void MatchingEngine::ProcessOrder(Order &takerOrder, uint32_t takerIdx)
{

    spdlog::info("ProcessOrder idx={} client={} order={} {} {} qty={} price={} type={}", takerIdx, takerOrder.clientId,
                 takerOrder.orderId, takerOrder.symbol, takerOrder.side == eSide::BUY ? "BUY" : "SELL",
                 takerOrder.quantity, takerOrder.price, takerOrder.orderType == eOrderType::LIMIT ? "LIMIT" : "MARKET");

    PostAccepted(takerOrder);

    auto &book = mOrderBook[takerOrder.symbol];

    if (takerOrder.side == eSide::BUY)
    {
        ExecuteMatch(takerOrder, takerIdx, book.asks, book.bids, eSide::BUY);
    }
    else
    {
        ExecuteMatch(takerOrder, takerIdx, book.bids, book.asks, eSide::SELL);
    }
}

void MatchingEngine::ProcessCancel(const EngineCommand &command)
{
    spdlog::info("ProcessCancel client={} order={} symbol={} indexSize={}", command.clientId, command.orderId,
                 command.symbol, mOrderIndex.size());

    bool canceled{false};
    {
        std::lock_guard<std::mutex> lock(mOrderIndexMutex);
        auto it = mOrderIndex.find(command.orderId);

        if (it != mOrderIndex.end())
        {
            Order &order = mOrders.Get(it->second);
            order.cancelled.store(true, std::memory_order_release);
            mOrderIndex.erase(it);

            canceled = true;
        }
    }

    TradeNotification cancelNote{};
    cancelNote.type = canceled ? eTradeNotificationType::CANCELLED : eTradeNotificationType::CANCELLED_REJECTED;
    cancelNote.clientId = command.clientId;
    cancelNote.orderId = command.orderId;
    cancelNote.price = command.price;
    cancelNote.quantity = 0;
    cancelNote.isFullFill = false;
    std::strncpy(cancelNote.symbol, command.symbol, sizeof(cancelNote.symbol) - 1);

    NotificationDispatcher::Instance().Post(command.responseThreadIdx, cancelNote);
}

void MatchingEngine::GenerateTrade(const Order &taker, const Order &maker, uint32_t qty, double price, bool takerFilled,
                                   bool makerFilled)
{
    spdlog::info("TRADE takerClient={} makerClient={} qty={} price={} takerFull={} makerFull={}", taker.clientId,
                 maker.clientId, qty, price, takerFilled, makerFilled);

    TradeNotification takerNote{};
    takerNote.type = takerFilled ? eTradeNotificationType::FILLED : eTradeNotificationType::PARTIALLY_FILLED;
    takerNote.clientId = taker.clientId;
    takerNote.orderId = taker.orderId;
    std::strncpy(takerNote.symbol, taker.symbol, sizeof(taker.symbol) - 1);
    takerNote.price = price;
    takerNote.quantity = qty;
    takerNote.isFullFill = takerFilled;

    NotificationDispatcher::Instance().Post(taker.responseThreadIdx, takerNote);

    TradeNotification makerNote{};
    makerNote.type = makerFilled ? eTradeNotificationType::FILLED : eTradeNotificationType::PARTIALLY_FILLED;
    makerNote.clientId = maker.clientId;
    makerNote.orderId = maker.orderId;
    std::strncpy(makerNote.symbol, maker.symbol, sizeof(maker.symbol) - 1);
    makerNote.price = price;
    makerNote.quantity = qty;
    makerNote.isFullFill = makerFilled;

    NotificationDispatcher::Instance().Post(maker.responseThreadIdx, makerNote);
}

void MatchingEngine::PostAccepted(const Order &order)
{
    TradeNotification note{};
    note.type = eTradeNotificationType::ACCEPTED;
    note.clientId = order.clientId;
    note.orderId = order.orderId;

    std::strncpy(note.symbol, order.symbol, sizeof(note.symbol) - 1);
    note.price = order.price;
    note.quantity = order.quantity;
    note.isFullFill = false;

    NotificationDispatcher::Instance().Post(order.responseThreadIdx, note);
}

void MatchingEngine::PostCancelResult(const Order &order, bool canceled)
{
    TradeNotification note{};
    note.type = canceled ? eTradeNotificationType::CANCELLED : eTradeNotificationType::CANCELLED_REJECTED;
    std::strncpy(note.symbol, order.symbol, sizeof(note.symbol) - 1);
    note.clientId = order.clientId;
    note.orderId = order.orderId;
    note.price = order.price;
    note.quantity = 0;

    NotificationDispatcher::Instance().Post(order.responseThreadIdx, note);
}

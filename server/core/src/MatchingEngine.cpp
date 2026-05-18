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

    // order.nextIdx.store(0, std::memory_order_relaxed);

    // uint32_t prevIdx = mHeadIdx.exchange(orderIdx, std::memory_order_acq_rel);
    // if (prevIdx != 0)
    // {
    //     mOrders.Get(prevIdx).nextIdx.store(orderIdx, std::memory_order_release);
    // }

    // TODO: Implement and apply here MPSCQueue ti avoid race condition and using mutex
    {
        std::lock_guard<std::mutex> lock(mPendingMutex);
        mPendingOrders.push(orderIdx);
    }
}

bool MatchingEngine::CancelOrder(uint64_t orderId)
{
    spdlog::info("CancelOrder request orderId={} indexSize={}", orderId, mOrderIndex.size());

    std::lock_guard<std::mutex> lock(mOrderIndexMutex);
    auto it = mOrderIndex.find(orderId);

    if (it == mOrderIndex.end())
    {
        spdlog::warn("CancelOrder rejected: orderId={} not found", orderId);
        return false;
    }

    auto idx = it->second;
    Order &order = mOrders.Get(idx);

    spdlog::info("CancelOrder found orderId={} idx={} qty={} cancelled={}", orderId, idx, order.quantity,
                 order.cancelled.load());

    order.cancelled.store(true, std::memory_order_release);

    mOrderIndex.erase(it);

    return true;
}

OrderPool &MatchingEngine::GetPoll() { return mOrders; }

void MatchingEngine::Start()
{
    spdlog::info("MatchingEngine::Start");
    if (mRunning.exchange(true))
        return;

    mThread = std::jthread([this](std::stop_token st) { Run(st); });
}

void MatchingEngine::Stop()
{
    spdlog::info("MatchingEngine::Stop");
    mThread.request_stop();

    if (!mRunning.exchange(false, std::memory_order_acq_rel))
        return;

    if (mThread.joinable())
    {
        mThread.join();
    }
}

void MatchingEngine::ResetForTesting()
{
    Stop();

    {
        std::lock_guard lock(mPendingMutex);
        std::queue<uint32_t> empty;
        std::swap(mPendingOrders, empty);
    }

    mOrderBook.clear();
    mOrderIndex.clear();
    mOrders.Reset();
}

void MatchingEngine::Run(std::stop_token stop)
{
    // uint32_t localQueueHead{0};
    spdlog::info("MatchingEngine::Run");

    while (!stop.stop_requested())
    {
        // TODO: Implement and apply here MPSCQueue (ASAP) to avoid race condition and using mutex
        uint32_t orderIdx = 0;

        {
            std::lock_guard<std::mutex> lock(mPendingMutex);

            if (!mPendingOrders.empty())
            {
                orderIdx = mPendingOrders.front();
                mPendingOrders.pop();
            }
        }

        if (orderIdx == 0)
        {
            std::this_thread::yield();
            continue;
        }

        Order &order = mOrders.Get(orderIdx);
        ProcessOrder(order, orderIdx);
    }

    // // Get current available orders
    // uint32_t head = mHeadIdx.exchange(0, std::memory_order_acq_rel);
    // if (head == 0 && localQueueHead == 0)
    // {
    //     std::this_thread::yield();
    //     continue;
    // }

    // // Reverse chunk
    // uint32_t current = head;
    // uint32_t reversedHead = 0;
    // while (current != 0)
    // {
    //     uint32_t next = mOrders.Get(current).nextIdx.load(std::memory_order_acquire);
    //     mOrders.Get(current).nextIdx.store(reversedHead, std::memory_order_relaxed);
    //     reversedHead = current;
    //     current = next;
    // }

    // if (localQueueHead == 0)
    // {
    //     localQueueHead = reversedHead;
    // }
    // else
    // {
    //     uint32_t temp = localQueueHead;
    //     while (mOrders.Get(temp).nextIdx.load(std::memory_order_relaxed) != 0)
    //     {
    //         temp = mOrders.Get(temp).nextIdx.load(std::memory_order_relaxed);
    //     }
    //     mOrders.Get(temp).nextIdx.store(reversedHead, std::memory_order_relaxed);
    // }

    // while (localQueueHead != 0)
    // {
    //     Order &order = mOrders.Get(localQueueHead);
    //     uint32_t nextToProcess = order.nextIdx.load(std::memory_order_relaxed);

    //     uint32_t currentIdx = localQueueHead;

    //     ProcessOrder(order, currentIdx);
    //     localQueueHead = nextToProcess;
    // }
}

void MatchingEngine::ProcessOrder(Order &takerOrder, uint32_t takerIdx)
{

    spdlog::info("ProcessOrder idx={} client={} order={} {} {} qty={} price={} type={}", takerIdx, takerOrder.clientId,
                 takerOrder.orderId, takerOrder.symbol, takerOrder.side == eSide::BUY ? "BUY" : "SELL",
                 takerOrder.quantity, takerOrder.price, takerOrder.orderType == eOrderType::LIMIT ? "LIMIT" : "MARKET");

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

void MatchingEngine::GenerateTrade(const Order &taker, const Order &maker, uint32_t qty, double price, bool takerFilled,
                                   bool makerFilled)
{
    spdlog::info("TRADE takerClient={} makerClient={} qty={} price={} takerFull={} makerFull={}", taker.clientId,
                 maker.clientId, qty, price, takerFilled, makerFilled);

    TradeNotification takerNote{};
    takerNote.clientId = taker.clientId;
    takerNote.orderId = taker.orderId;
    std::strncpy(takerNote.symbol, taker.symbol, sizeof(taker.symbol) - 1);
    takerNote.fillPrice = price;
    takerNote.fillQuantity = qty;
    takerNote.isFullFill = takerFilled;

    NotificationDispatcher::Instance().Post(taker.responseThreadIdx, takerNote);

    TradeNotification makerNote{};
    makerNote.clientId = maker.clientId;
    makerNote.orderId = maker.orderId;
    std::strncpy(makerNote.symbol, maker.symbol, sizeof(maker.symbol) - 1);
    makerNote.fillPrice = price;
    makerNote.fillQuantity = qty;
    makerNote.isFullFill = makerFilled;

    NotificationDispatcher::Instance().Post(maker.responseThreadIdx, makerNote);
}

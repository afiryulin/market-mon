#include <random>
#include <spdlog/spdlog.h>

#include "../include/MatchingEngine.h"
#include "../include/TradeNotificationDispatcher.h"

MatchingEngine::MatchingEngine() : mOrders{1000000} { mHeadIdx.store(0); }

void MatchingEngine::SubmitOrder(uint32_t orderIdx)
{
    Order &order = mOrders.Get(orderIdx);
    order.nextIdx.store(0, std::memory_order_relaxed);

    uint32_t prevIdx = mHeadIdx.exchange(orderIdx, std::memory_order_acq_rel);
    if (prevIdx != 0)
    {
        mOrders.Get(prevIdx).nextIdx.store(orderIdx, std::memory_order_release);
    }
}

OrderPull &MatchingEngine::GetPoll() { return mOrders; }

void MatchingEngine::Start()
{
    mThread = std::jthread([this](std::stop_token st) { Run(st); });
}

void MatchingEngine::Run(std::stop_token stop)
{
    uint32_t localQueueHead{0};

    while (!stop.stop_requested())
    {
        // Get current available orders
        uint32_t head = mHeadIdx.exchange(0, std::memory_order_acq_rel);
        if (head == 0 && localQueueHead == 0)
        {
            std::this_thread::yield();
            continue;
        }

        // Reverse chunk
        uint32_t current = head;
        uint32_t reversedHead = 0;
        while (current != 0)
        {
            uint32_t next = mOrders.Get(current).nextIdx.load(std::memory_order_acquire);
            mOrders.Get(current).nextIdx.store(reversedHead, std::memory_order_relaxed);
            reversedHead = current;
            current = next;
        }

        if (localQueueHead == 0)
        {
            localQueueHead = reversedHead;
        }
        else
        {
            uint32_t temp = localQueueHead;
            while (mOrders.Get(temp).nextIdx.load(std::memory_order_relaxed) != 0)
            {
                temp = mOrders.Get(temp).nextIdx.load(std::memory_order_relaxed);
            }
            mOrders.Get(temp).nextIdx.store(reversedHead, std::memory_order_relaxed);
        }

        while (localQueueHead != 0)
        {
            Order &order = mOrders.Get(localQueueHead);
            uint32_t nextToProcess = order.nextIdx.load(std::memory_order_relaxed);

            uint32_t currentIdx = localQueueHead;

            ProcessOrder(order, currentIdx);
            localQueueHead = nextToProcess;
        }
    }
}

void MatchingEngine::ProcessOrder(Order &takerOrder, uint32_t takerIdx)
{
    auto &book = mOrderBook[takerOrder.symbol];
    if (takerOrder.side == eSide::BUY)
    {
        ExecuteMatch(takerOrder, takerIdx, book.asks, book.bids);
    }
    else
    {
        ExecuteMatch(takerOrder, takerIdx, book.bids, book.asks);
    }
}

void MatchingEngine::GenerateTrade(const Order &taker, const Order &maker, uint32_t qty, double price)
{
    TradeNotification takerNote{taker.clientId, taker.orderId, price, qty, true};
    NotificationDispatcher::Instance().Post(taker.clientId, takerNote);

    TradeNotification makerNote{maker.clientId, maker.orderId, price, qty, maker.quantity == 0};
    NotificationDispatcher::Instance().Post(maker.clientId, makerNote);
}

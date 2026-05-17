#pragma once

#include <deque>
#include <map>
#include <mutex>
#include <queue>
#include <spdlog/spdlog.h>
#include <stop_token>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>

#include "../include/OrderPool.h"

class MatchingEngine
{
public:
    static MatchingEngine &Instance();

    void SubmitOrder(uint32_t orderIdx);
    bool CancelOrder(uint64_t orderId);
    OrderPool &GetPoll();

    void Start();

private:
    MatchingEngine();
    void Run(std::stop_token stop);
    void ProcessOrder(Order &takerOrder, uint32_t takerIdx);

    template <typename OpponentMap, typename SelfMap>
    void ExecuteMatch(Order &takerOrder, uint32_t takerIdx, OpponentMap &opponentBook, SelfMap &selfBook,
                      eSide takerSide);

    void GenerateTrade(const Order &taker, const Order &maker, uint32_t qty, double price, bool takerFilled,
                       bool makerFilled);

private:
    alignas(64) std::atomic<uint32_t> mHeadIdx;
    OrderPool mOrders;
    std::unordered_map<std::string, OrderBook> mOrderBook;

    std::atomic<bool> mRunning{false};
    std::jthread mThread;

    // TODO: Implement and apply here MPSCQueue ti avoid race condition and using mutex
    std::mutex mPendingMutex;
    std::queue<uint32_t> mPendingOrders;

    std::mutex mOrderIndexMutex;
    std::unordered_map<uint64_t, uint32_t> mOrderIndex;
};

template <typename OpponentMap, typename SelfMap>
inline void MatchingEngine::ExecuteMatch(Order &takerOrder, uint32_t takerIdx, OpponentMap &opponentBook,
                                         SelfMap &selfBook, eSide takerSide)
{
    spdlog::info("Opponent book size={}, self book size={}", opponentBook.size(), selfBook.size());

    static_assert(std::is_same_v<typename OpponentMap::mapped_type, std::deque<uint32_t>>,
                  "OpponentMap must contain Order as value type");

    auto it = opponentBook.begin();

    while (it != opponentBook.end() && takerOrder.quantity > 0)
    {
        double bestOpponentPrice = it->first;

        if (takerOrder.orderType == eOrderType::LIMIT)
        {
            spdlog::info("Added to book idx={} side={} price={} qty={}", takerIdx,
                         takerOrder.side == eSide::BUY ? "BUY" : "SELL", takerOrder.price, takerOrder.quantity);

            if (takerSide == eSide::BUY && takerOrder.price < bestOpponentPrice)
                break; // Too expansive

            if (takerSide == eSide::SELL && takerOrder.price > bestOpponentPrice)
                break; // Too cheap
        }

        decltype(auto) orderAtPrice = it->second;
        while (!orderAtPrice.empty() && takerOrder.quantity > 0)
        {
            spdlog::info("TryMatch taker={} {} qty={} price={} with best={} queue={}", takerOrder.orderId,
                         takerOrder.side == eSide::BUY ? "BUY" : "SELL", takerOrder.quantity, takerOrder.price,
                         bestOpponentPrice, orderAtPrice.size());

            uint32_t makerIdx = orderAtPrice.front();
            Order &makerOrder = mOrders.Get(makerIdx);

            if (makerOrder.cancelled.load(std::memory_order_acquire))
            {
                orderAtPrice.pop_front();
                continue;
            }

            uint32_t matchedQty = std::min(takerOrder.quantity, makerOrder.quantity);

            bool takerFilled = takerOrder.quantity == matchedQty;
            bool makerFilled = makerOrder.quantity == matchedQty;

            if (takerFilled)
            {
                std::lock_guard<std::mutex> lock(mOrderIndexMutex);
                mOrderIndex.erase(takerOrder.orderId);
            }
            if (makerFilled)
            {
                std::lock_guard<std::mutex> lock(mOrderIndexMutex);
                mOrderIndex.erase(makerOrder.orderId);
            }

            takerOrder.quantity -= matchedQty;
            makerOrder.quantity -= matchedQty;

            GenerateTrade(takerOrder, makerOrder, matchedQty, bestOpponentPrice, takerFilled, makerFilled);
            if (makerOrder.quantity == 0)
            {
                orderAtPrice.pop_front();
            }
        }

        if (orderAtPrice.empty())
            it = opponentBook.erase(it);
        else
            break;
    }

    if (takerOrder.quantity > 0 && takerOrder.orderType == eOrderType::LIMIT)
    {
        selfBook[takerOrder.price].push_back(takerIdx);
        {
            std::lock_guard<std::mutex> lock(mOrderIndexMutex);
            mOrderIndex[takerOrder.orderId] = takerIdx;
        }
        spdlog::info("Added to book idx={} side={} price={} qty={}", takerIdx, takerSide == eSide::BUY ? "BUY" : "SELL",
                     takerOrder.price, takerOrder.quantity);
    }
}

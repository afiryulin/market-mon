#pragma once

#include <deque>
#include <map>
#include <mutex>
#include <queue>
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
};

template <typename OpponentMap, typename SelfMap>
inline void MatchingEngine::ExecuteMatch(Order &takerOrder, uint32_t takerIdx, OpponentMap &opponentBook,
                                         SelfMap &selfBook, eSide takerSide)
{
    static_assert(std::is_same_v<typename OpponentMap::mapped_type, std::deque<uint32_t>>,
                  "OpponentMap must contain Order as value type");

    auto it = opponentBook.begin();

    while (it != opponentBook.end() && takerOrder.quantity > 0)
    {
        double bestOpponentPrice = it->first;

        if (takerOrder.orderType == eOrderType::LIMIT)
        {
            if (takerSide == eSide::BUY && takerOrder.price < bestOpponentPrice)
                break; // Too expansive

            if (takerSide == eSide::SELL && takerOrder.price > bestOpponentPrice)
                break; // Too cheap
        }

        decltype(auto) deque = it->second;
        while (!deque.empty() && takerOrder.quantity > 0)
        {
            uint32_t makerIdx = deque.front();
            Order &makerOrder = mOrders.Get(makerIdx);

            uint32_t matchedQty = std::min(takerOrder.quantity, makerOrder.quantity);

            takerOrder.quantity -= matchedQty;
            makerOrder.quantity -= matchedQty;

            bool takerFilled = takerOrder.quantity == matchedQty;
            bool makerFilled = makerOrder.quantity == matchedQty;

            GenerateTrade(takerOrder, makerOrder, matchedQty, bestOpponentPrice, takerFilled, makerFilled);
            if (makerOrder.quantity == 0)
            {
                deque.pop_front();
            }
        }

        if (deque.empty())
            it = opponentBook.erase(it);
        else
            break;
    }

    if (takerOrder.quantity > 0 && takerOrder.orderType == eOrderType::LIMIT)
    {
        selfBook[takerOrder.price].push_back(takerIdx);
    }
}

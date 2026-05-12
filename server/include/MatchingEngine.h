#pragma once

#include <deque>
#include <map>
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
    OrderPull &GetPoll();

    void Start();

private:
    MatchingEngine();
    void Run(std::stop_token stop);
    void ProcessOrder(Order &takerOrder, uint32_t takerIdx);

    template <typename OpponentMap, typename SelfMap>
    void ExecuteMatch(Order &takerOrder, uint32_t takerIdx, OpponentMap &opponentBook, SelfMap &selfBook);

    void GenerateTrade(const Order &taker, const Order &maker, uint32_t qty, double price);

private:
    alignas(64) std::atomic<uint32_t> mHeadIdx;
    OrderPull mOrders;
    std::unordered_map<std::string, OrderBook> mOrderBook;

    std::atomic<bool> mRunning{false};
    std::jthread mThread;
};

MatchingEngine &MatchingEngine::Instance()
{
    static MatchingEngine inst;
    return inst;
}

template <typename OpponentMap, typename SelfMap>
inline void MatchingEngine::ExecuteMatch(Order &takerOrder, uint32_t takerIdx, OpponentMap &opponentBook,
                                         SelfMap &selfBook)
{
    static_assert(std::is_same_v<typename OpponentMap::mapped_type, std::deque<uint32_t>>,
                  "OpponentMap must contain Order as value type");

    auto it = opponentBook.begin();

    while (it != opponentBook.end() && takerOrder.quantity > 0)
    {
        double bestOpponentPrice = it->first;

        // Проверка условия цены (Buy: taker.price >= ask.price | Sell: taker.price <= bid.price)
        if constexpr (std::is_same_v<OpponentMap, std::map<double, std::deque<uint32_t>>>)
        {
            if (takerOrder.price < bestOpponentPrice)
                break; // Too expensive
        }
        else
        {
            if (takerOrder.price > bestOpponentPrice)
                break; // Too cheap
        }

        decltype(auto) deque = it->second;
        while (!deque.empty() && takerOrder.quantity > 0)
        {
            uint32_t makerIdx = deque.front();
            Order &makerOrder = mOrders.Get(makerIdx);

            uint32_t matchedQty = std::min(takerOrder.quantity, makerOrder.quantity);
            GenerateTrade(takerOrder, makerOrder, matchedQty, bestOpponentPrice);

            takerOrder.quantity -= matchedQty;
            makerOrder.quantity -= matchedQty;

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

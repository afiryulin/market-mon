#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <stdexcept>
#include <vector>

#include "Order.h"

class OrderPool final
{
public:
    explicit OrderPool(const size_t capacity) : mPool{capacity} {}

    uint32_t Allocate()
    {
        auto index = mNextFreeIdx.fetch_add(1, std::memory_order_relaxed);
        if (index >= mPool.size())
            throw std::runtime_error("Order pool exhausted");
        return index;
    }
    Order &Get(uint32_t idx) { return mPool[idx]; }

    void Reset() { mNextFreeIdx.store(1, std::memory_order_relaxed); };

private:
    std::vector<Order> mPool;
    std::atomic<uint32_t> mNextFreeIdx{1}; // 0 is same logic with nullptr
};

struct OrderBook
{
    std::map<double, std::deque<uint32_t>, std::greater<double>> bids;
    std::map<double, std::deque<uint32_t>, std::less<double>> asks;
};

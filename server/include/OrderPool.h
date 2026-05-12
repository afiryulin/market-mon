#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <vector>

#include "Order.h"

class OrderPull final
{
public:
    explicit OrderPull(const size_t capacity) : mPool{capacity} {}

    uint32_t Allocate() { return mNextFreeIdx.fetch_add(1, std::memory_order_relaxed); }
    Order &Get(uint32_t idx) { return mPool[idx]; }

private:
    std::vector<Order> mPool;
    std::atomic<uint32_t> mNextFreeIdx{1}; // 0 is same logic with nullptr
};

struct OrderBook
{
    std::map<double, std::deque<uint32_t>, std::greater<double>> bids;
    std::map<double, std::deque<uint32_t>, std::less<double>> asks;
};

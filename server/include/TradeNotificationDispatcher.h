#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include "SPSCQueue.h"
#include "TradeNotification.h"

class NotificationDispatcher final
{
public:
    static NotificationDispatcher &Instance()
    {
        static NotificationDispatcher instance;
        return instance;
    }

    void RegisterQueue(size_t responseThreadIdx, SPSCQueue<TradeNotification> &queue);
    void Post(size_t responseThreadIdx, const TradeNotification &notification);

private:
    std::array<std::atomic<SPSCQueue<TradeNotification> *>, 32> mQueues{};
};

inline void NotificationDispatcher::RegisterQueue(size_t responseThreadIdx, SPSCQueue<TradeNotification> &queue)
{
    if (responseThreadIdx >= mQueues.size())
        throw std::out_of_range("Invalid dispatcher thread index");

    mQueues[responseThreadIdx].store(&queue, std::memory_order_release);
}

inline void NotificationDispatcher::Post(size_t responseThreadIdx, const TradeNotification &notification)
{
    if (responseThreadIdx >= mQueues.size())
    {
        spdlog::error("Response queue for thread {} is full", responseThreadIdx);
    }

    auto *queue = mQueues[responseThreadIdx].load(std::memory_order_acquire);
    queue->Push(notification);
}

#pragma once

#include <array>
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

    void RegisterQueue(size_t threadIdx, SPSCQueue<TradeNotification> &queue);
    void Post(size_t threadIdx, const TradeNotification &notification);

private:
    std::array<SPSCQueue<TradeNotification> *, 32> mQueues{nullptr};
};

inline void NotificationDispatcher::RegisterQueue(size_t threadIdx, SPSCQueue<TradeNotification> &queue)
{
    mQueues[threadIdx] = &queue;
}

inline void NotificationDispatcher::Post(size_t threadIdx, const TradeNotification &notification)
{
    if (mQueues[threadIdx] != nullptr && !mQueues[threadIdx]->Push(notification))
    {
        spdlog::error("Response queue for thread {} is full", threadIdx);
    }
}

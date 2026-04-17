#include <algorithm>
#include <spdlog/spdlog.h>

#include "../include/SubscriberManager.h"
#include "../include/SubscribePriceCallData.h"

SubscriberManager &SubscriberManager::Instance()
{
    static SubscriberManager instance;
    return instance;
}

void SubscriberManager::AddSubscriber(SubscribePriceCallData *subscriber)
{
    std::lock_guard<std::mutex> locker(mMutex);
    mSubscribers.push_back(subscriber);
}

void SubscriberManager::RemoveSubscriber(SubscribePriceCallData *subscriber)
{
    spdlog::info("SubscriberManager::RemoveSubscriber");

    std::lock_guard<std::mutex> locker(mMutex);
    mSubscribers.erase(
        std::remove(mSubscribers.begin(), mSubscribers.end(), subscriber),
        mSubscribers.end());
}

void SubscriberManager::BroadcastPrice(const std::string &symbol, double value)
{

    std::vector<SubscribePriceCallData *> buff;

    {
        std::lock_guard<std::mutex> locker(mMutex);
        buff = mSubscribers;
    }
    for (auto *subscriber : buff)
    {
        subscriber->PushPrice(symbol, value);
    }
}

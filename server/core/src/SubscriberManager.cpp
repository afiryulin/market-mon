#include <algorithm>
#include <spdlog/spdlog.h>

#include "../include/SubscribePriceCallData.h"
#include "../include/SubscriberManager.h"

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

    mSubscribers.erase(std::remove(mSubscribers.begin(), mSubscribers.end(), subscriber), mSubscribers.end());
}

void SubscriberManager::Shutdown()
{
    std::lock_guard<std::mutex> locker(mMutex);
    mSubscribers.clear();
}

void SubscriberManager::BroadcastPrice(const std::string &symbol, double value)
{
    std::vector<SubscribePriceCallData *> subscribersCopy;
    {
        std::lock_guard<std::mutex> locker(mMutex);
        subscribersCopy = mSubscribers;
    }

    for (auto subscriber : subscribersCopy)
    {
        subscriber->PushPrice(symbol, value);
    }
}

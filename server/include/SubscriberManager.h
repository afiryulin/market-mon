#pragma once
#include <mutex>
#include <vector>
#include <string>

class SubscribePriceCallData;

class SubscriberManager final
{
public:
    static SubscriberManager &Instance();

    void AddSubscriber(SubscribePriceCallData *subscriber);
    void RemoveSubscriber(SubscribePriceCallData *subscriber);

    void BroadcastPrice(const std::string &symbol, double value);

private:
    std::mutex mMutex;
    std::vector<SubscribePriceCallData *> mSubscribers;
};
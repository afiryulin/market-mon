#include <random>
#include <chrono>

#include "../include/PriceGenerator.h"
#include "../include/SubscriberManager.h"

void PriceGenerator::Start()
{
    mRunning.store(true);
    mThread = std::thread(&PriceGenerator::RunInternal, this);
}

void PriceGenerator::Stop()
{
    mRunning.store(false);
    if (mThread.joinable())
    {
        mThread.join();
    }
}

void PriceGenerator::SetCallback(Callback fn)
{
    mCallback = std::move(fn);
}

void PriceGenerator::RunInternal()
{
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<> distr(-5, 5);

    double btc = 60000;
    double eth = 3000;

    while (mRunning.load())
    {
        btc += distr(rng);
        eth += distr(rng);

        if (mCallback)
        {
            mCallback("BTC", btc);
            mCallback("ETH", eth);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

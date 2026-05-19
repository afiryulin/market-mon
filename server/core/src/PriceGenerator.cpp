#include <chrono>
#include <random>
#include <stop_token>
#include <thread>

#include "../include/PriceGenerator.h"
#include "../include/SubscriberManager.h"

void PriceGenerator::Start()
{
    mRunning.store(true);
    mThread = std::jthread([this](std::stop_token stop) { RunInternal(stop); });
}

void PriceGenerator::Stop()
{
    mRunning.store(false);
    if (mThread.joinable())
    {
        mThread.request_stop();
        mThread.join();
    }
}

void PriceGenerator::SetCallback(Callback fn) { mCallback = std::move(fn); }

void PriceGenerator::RunInternal(std::stop_token stop)
{
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<> distr(-5, 5);

    double btc = 60000;
    double eth = 3000;

    while (!stop.stop_requested() && mRunning.load())
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

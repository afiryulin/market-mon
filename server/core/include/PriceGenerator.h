#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

class PriceGenerator final
{
public:
    using Callback = std::function<void(const std::string, double)>;

    void Start();
    void Stop();

    void SetCallback(Callback fn);

private:
    void RunInternal(std::stop_token stop);

    std::jthread mThread;
    std::atomic<bool> mRunning{false};
    Callback mCallback;
};

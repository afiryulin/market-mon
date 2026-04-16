#pragma once

#include <functional>
#include <string>
#include <thread>
#include <atomic>

class PriceGenerator final
{
public:
    using Callback = std::function<void(const std::string, double)>;

    void Start();
    void Stop();

    void SetCallback(Callback fn);

private:
    void RunInternal();

    std::thread mThread;
    std::atomic<bool> mRunning{false};
    Callback mCallback;
};
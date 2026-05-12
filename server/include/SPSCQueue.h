#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

template <typename T, size_t Size = 1024>
class SPSCQueue final
{
public:
    static_assert((Size & (Size - 1)) == 0, "Size must be a power of 2");

    bool Push(const T &data)
    {
        const size_t head = mHead.load(std::memory_order_relaxed);
        const size_t nextHead = (head + 1) & (Size - 1);

        if (nextHead == mTail.load(std::memory_order_relaxed))
            return false;

        mBuffer[head] = data;
        mHead.store(nextHead, std::memory_order_release);

        return true;
    }

    std::optional<T> Pop()
    {
        const size_t tail = mTail.load(std::memory_order_relaxed);
        if (tail == mHead.load(std::memory_order_acquire))
            return std::nullopt;

        T data = mBuffer[tail];
        mTail.store((tail + 1) & (Size - 1), std::memory_order_release);
        return data;
    }

private:
    alignas(64) std::atomic<size_t> mHead{0};
    alignas(64) std::atomic<size_t> mTail{0};
    T mBuffer[Size];
};

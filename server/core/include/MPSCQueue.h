#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

template <typename T, std::size_t Size = 1024>
class MPSCQueue final
{
public:
    static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");

    MPSCQueue() { Reset(); }

    MPSCQueue(const MPSCQueue &) = delete;
    MPSCQueue &operator=(const MPSCQueue &) = delete;

    bool Push(const T &value)
    {
        Cell *cell = nullptr;
        std::size_t pos = mEnqueuePos.load(std::memory_order_relaxed);

        for (;;)
        {
            cell = &mBuffer[pos & BUFFER_MASK];

            const std::size_t seq = cell->sequence.load(std::memory_order_acquire);

            const auto diff = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos);

            if (diff == 0)
            {
                if (mEnqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed,
                                                      std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (diff < 0)
            {
                return false; // full
            }
            else
            {
                pos = mEnqueuePos.load(std::memory_order_relaxed);
            }
        }

        cell->data = value;
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool Push(T &&value)
    {
        Cell *cell = nullptr;
        std::size_t pos = mEnqueuePos.load(std::memory_order_relaxed);

        for (;;)
        {
            cell = &mBuffer[pos & BUFFER_MASK];

            const std::size_t seq = cell->sequence.load(std::memory_order_acquire);

            const auto diff = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos);

            if (diff == 0)
            {
                if (mEnqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed,
                                                      std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (diff < 0)
            {
                return false; // full
            }
            else
            {
                pos = mEnqueuePos.load(std::memory_order_relaxed);
            }
        }

        cell->data = std::move(value);
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    std::optional<T> Pop()
    {
        Cell *cell = nullptr;
        std::size_t pos = mDequeuePos.load(std::memory_order_relaxed);

        for (;;)
        {
            cell = &mBuffer[pos & BUFFER_MASK];

            const std::size_t seq = cell->sequence.load(std::memory_order_acquire);

            const auto diff = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos + 1);

            if (diff == 0)
            {
                mDequeuePos.store(pos + 1, std::memory_order_relaxed);
                break;
            }
            else if (diff < 0)
            {
                return std::nullopt; // empty
            }
            else
            {
                pos = mDequeuePos.load(std::memory_order_relaxed);
            }
        }

        T value = std::move(cell->data);

        cell->sequence.store(pos + Size, std::memory_order_release);

        return value;
    }

    void Reset()
    {
        mEnqueuePos.store(0, std::memory_order_relaxed);
        mDequeuePos.store(0, std::memory_order_relaxed);

        for (std::size_t i = 0; i < Size; ++i)
        {
            mBuffer[i].sequence.store(i, std::memory_order_relaxed);
            mBuffer[i].data = T{};
        }
    }

private:
    static constexpr std::size_t BUFFER_MASK = Size - 1;

    struct Cell
    {
        std::atomic<std::size_t> sequence{};
        T data{};
    };

private:
    alignas(64) std::array<Cell, Size> mBuffer{};
    alignas(64) std::atomic<std::size_t> mEnqueuePos{0};
    alignas(64) std::atomic<std::size_t> mDequeuePos{0};
};

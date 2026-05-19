#include <gtest/gtest.h>

#include "MPSCQueue.h"
#include "MatchingEngine.h"

#include <atomic>
#include <thread>
#include <unordered_set>
#include <vector>

TEST(MPSCQueueTest, PushPopSingleThread)
{
    MatchingEngine::Instance().ResetForTesting();
    MPSCQueue<int, 8> queue;

    EXPECT_TRUE(queue.Push(1));
    EXPECT_TRUE(queue.Push(2));
    EXPECT_TRUE(queue.Push(3));

    auto a = queue.Pop();
    auto b = queue.Pop();
    auto c = queue.Pop();
    auto d = queue.Pop();

    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    ASSERT_TRUE(c.has_value());
    ASSERT_FALSE(d.has_value());

    EXPECT_EQ(*a, 1);
    EXPECT_EQ(*b, 2);
    EXPECT_EQ(*c, 3);
}

TEST(MPSCQueueTest, MultipleProducersSingleConsumer)
{
    MatchingEngine::Instance().ResetForTesting();
    constexpr int producers = 4;
    constexpr int perProducer = 1000;
    constexpr int total = producers * perProducer;

    MPSCQueue<int, 8192> queue;

    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    std::vector<std::thread> threads;

    for (int p = 0; p < producers; ++p)
    {
        threads.emplace_back([p, &queue, &produced] {
            for (int i = 0; i < perProducer; ++i)
            {
                const int value = p * perProducer + i;

                while (!queue.Push(value))
                {
                    std::this_thread::yield();
                }

                produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    std::unordered_set<int> values;
    values.reserve(total);

    while (consumed.load(std::memory_order_relaxed) < total)
    {
        auto value = queue.Pop();

        if (!value.has_value())
        {
            std::this_thread::yield();
            continue;
        }

        values.insert(*value);
        consumed.fetch_add(1, std::memory_order_relaxed);
    }

    for (auto &thread : threads)
    {
        thread.join();
    }

    EXPECT_EQ(produced.load(), total);
    EXPECT_EQ(consumed.load(), total);
    EXPECT_EQ(values.size(), static_cast<size_t>(total));
}

TEST(MPSCQueueTest, ResetMakesQueueReusable)
{
    MPSCQueue<int, 8> queue;

    EXPECT_TRUE(queue.Push(1));
    EXPECT_TRUE(queue.Push(2));

    auto a = queue.Pop();
    auto b = queue.Pop();

    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());

    EXPECT_EQ(*a, 1);
    EXPECT_EQ(*b, 2);

    queue.Reset();

    EXPECT_TRUE(queue.Push(42));

    auto c = queue.Pop();

    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(*c, 42);
}

TEST(MPSCQueueTest, FullQueueReturnsFalse)
{
    MPSCQueue<int, 4> queue;

    EXPECT_TRUE(queue.Push(1));
    EXPECT_TRUE(queue.Push(2));
    EXPECT_TRUE(queue.Push(3));
    EXPECT_TRUE(queue.Push(4));

    EXPECT_FALSE(queue.Push(5));
}

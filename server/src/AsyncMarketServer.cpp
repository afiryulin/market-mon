#include <cstddef>
#include <spdlog/spdlog.h>
#include <thread>

#include "../include/AsyncMarketServer.h"
#include "../include/CallDataFactory.h"
#include "../include/GetPriceCallData.h"
#include "../include/SPSCQueue.h"
#include "../include/SubscribePriceCallData.h"
#include "../include/SubscriberManager.h"
#include "../include/TradeCallData.h"
#include "../include/TradeNotificationDispatcher.h"

void AsyncMarketServer::Run(const std::string &address)
{
    mPriceGenerator.SetCallback(
        [](const std::string &symbol, double value) { SubscriberManager::Instance().BroadcastPrice(symbol, value); });
    mPriceGenerator.Start();

    spdlog::info("Market Server started on {}", address);

    grpc::ServerBuilder serverBuilder;
    serverBuilder.AddListeningPort(address, grpc::InsecureServerCredentials());
    serverBuilder.RegisterService(&mService);

    const auto threads = std::max(1u, std::thread::hardware_concurrency());
    spdlog::info("Threads concurrency: {}", threads);
    for (size_t i = 0; i < threads; i++)
    {
        mCompletionQueues.emplace_back(serverBuilder.AddCompletionQueue());
        mTradeResponsesQueue.emplace_back(std::make_unique<SPSCQueue<TradeNotification>>());
    }

    mServer = serverBuilder.BuildAndStart();

    for (int i = 0; i < threads; i++)
    {
        auto *completionQueuePtr = mCompletionQueues[i].get();
        mThreads.emplace_back(
            [this, i, completionQueuePtr](std::stop_token token) { HandleCall(token, i, completionQueuePtr); });
    }

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    CallDataFactory<MarketService::AsyncService>::SeedQueues<TradeCallData, SubscribePriceCallData, GetPriceCallData>(
        &mService, mCompletionQueues);
}

void AsyncMarketServer::HandleCall(std::stop_token stop_token, size_t threadIdx, grpc::ServerCompletionQueue *queue)
{
    static std::atomic<uint8_t> threads_counter;
    std::stringstream ss;
    ss << std::this_thread::get_id();
    spdlog::info("Server's thread [{}] started [{}]", ss.str(), threads_counter++);

    decltype(auto) thisThreadTradeNoteQueue = mTradeResponsesQueue[threadIdx].get();
    NotificationDispatcher::Instance().RegisterQueue(threadIdx, thisThreadTradeNoteQueue);

    void *tag;
    bool ok;

    while (!stop_token.stop_requested() && queue->Next(&tag, &ok))
    {
        CallDataTag *dataTag = static_cast<CallDataTag *>(tag);

        if (!dataTag || !dataTag->parent)
        {
            spdlog::error("CQ: CRITICAL - invalid tag or parent");
            continue;
        }

        spdlog::info("CQ: Got tag {:p} ({}), ok: {} act: {}", tag, dataTag->parent->GetTypeName(), ok,
                     CallDataTag::ToString(dataTag->actionType));
        dataTag->parent->ProcessData(dataTag, ok);
    }
}

void AsyncMarketServer::Shutdown()
{
    spdlog::trace("Server shutdown.");
    if (mServer)
    {
        mServer->Shutdown();
    }

    for (auto &queue : mCompletionQueues)
    {
        queue->Shutdown();
    }

    for (auto &thread : mThreads)
    {
        thread.request_stop();
    }

    mPriceGenerator.Stop();
}

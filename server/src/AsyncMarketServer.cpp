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
    spdlog::info("Server's thread [{}] started.", threadIdx);

    // Local trading session for current thread only without blocking
    std::unordered_map<uint32_t, TradeCallData *> localTradeSession;

    decltype(auto) thisThreadTradeNoteQueue = mTradeResponsesQueue[threadIdx].get();
    NotificationDispatcher::Instance().RegisterQueue(threadIdx, *thisThreadTradeNoteQueue);

    while (!stop_token.stop_requested())
    {

        while (auto note = thisThreadTradeNoteQueue->Pop())
        {
            auto it = localTradeSession.find(note->clientId);
            if (it != localTradeSession.end())
            {
                it->second->OnTradeNotify(*note);
            }
        }

        void *tag;
        bool ok;

        auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(5);
        auto grpcStatus = queue->AsyncNext(&tag, &ok, deadline);

        if (grpcStatus == grpc::CompletionQueue::GOT_EVENT)
        {
            CallDataTag *dataTag = static_cast<CallDataTag *>(tag);

            assert(dataTag && dataTag->parent);

            if (std::strcmp(dataTag->parent->GetTypeName(), "TradeCallData"))
            {
                auto *tradeData = static_cast<TradeCallData *>(dataTag->parent);
                if (dataTag->actionType == eCallDataAction::CONNECT && ok)
                {
                    if (tradeData)
                    {
                        tradeData->SetResponseThreadIdx(threadIdx);
                        tradeData->RegisterSessionFromCurrentRequest();
                        localTradeSession[tradeData->GetClientId()] = tradeData;
                    }
                }

                if (!ok || dataTag->actionType == eCallDataAction::FINISH)
                {
                    if (tradeData)
                    {
                        localTradeSession.erase(tradeData->GetClientId());
                    }
                }
            }

            dataTag->parent->ProcessData(dataTag, ok);
        }
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

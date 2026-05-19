#include <cstddef>
#include <spdlog/spdlog.h>
#include <thread>

#include "../include/AsyncMarketServer.h"
#include "../include/CallDataFactory.h"
#include "../include/GetPriceCallData.h"
#include "../include/MatchingEngine.h"
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

    MatchingEngine::Instance().Start();

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
    CallDataFactory<MarketService::AsyncService>::SeedQueues<TradeCallData, SubscribePriceCallData>(&mService,
                                                                                                    mCompletionQueues);
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
            spdlog::info("Pop notification threadIdx={} client={} order={} qty={} price={} full={}", threadIdx,
                         note->clientId, note->orderId, note->quantity, note->price, note->isFullFill);

            auto it = localTradeSession.find(note->clientId);
            if (it != localTradeSession.end())
            {
                it->second->OnTradeNotify(*note);
            }
            else
            {
                spdlog::warn("No local thread session for clientId = {}", note->clientId);
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

            const bool isTrade = std::strcmp(dataTag->parent->GetTypeName(), "TradeCallData") == 0;
            auto *tradeData = isTrade ? static_cast<TradeCallData *>(dataTag->parent) : nullptr;

            if (isTrade && (!ok || dataTag->actionType == eCallDataAction::FINISH))
            {
                localTradeSession.erase(tradeData->GetClientId());
            }

            if (isTrade && ok && dataTag->actionType == eCallDataAction::READ)
            {
                if (tradeData->RegisterSessionFromCurrentRequest())
                {
                    localTradeSession[tradeData->GetClientId()] = tradeData;
                    spdlog::info("Registered local session clientId={} threadIdx={}", tradeData->GetClientId(),
                                 threadIdx);
                }
            }

            dataTag->parent->ProcessData(dataTag, ok);
        }
    }
}

void AsyncMarketServer::Shutdown()
{
    spdlog::info("Server shutdown...");

    // Stop price generation first to prevent new broadcasts
    mPriceGenerator.Stop();

    // Stop the matching engine
    MatchingEngine::Instance().Stop();

    // Shutdown the gRPC server (this cancels all pending operations)
    if (mServer)
    {
        mServer->Shutdown();
    }

    // Shutdown completion queues (this ensures no new events are generated)
    for (auto &queue : mCompletionQueues)
    {
        queue->Shutdown();
    }

    // Wait for all handler threads to finish (they will clean up CallData objects)
    mThreads.clear();

    // Finally clear any remaining subscribers
    SubscriberManager::Instance().Shutdown();
}

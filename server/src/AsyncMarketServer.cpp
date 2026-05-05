#include <cstddef>
#include <spdlog/spdlog.h>
#include <thread>

#include "../include/AsyncMarketServer.h"
#include "../include/GetPriceCallData.h"
#include "../include/SubscribePriceCallData.h"
#include "../include/SubscriberManager.h"
#include "../include/TradeCallData.h"

void AsyncMarketServer::Run(const std::string &address)
{
    mPriceGenerator.SetCallback([](const std::string &symbol, double value) {
        SubscriberManager::Instance().BroadcastPrice(symbol, value);
    });
    mPriceGenerator.Start();

    grpc::ServerBuilder serverBuilder;
    serverBuilder.AddListeningPort(address, grpc::InsecureServerCredentials());
    serverBuilder.RegisterService(&mService);

    mCompletionQueue = serverBuilder.AddCompletionQueue();
    mServer = serverBuilder.BuildAndStart();

    spdlog::info("Market Server started on {}", address);

    new SubscribePriceCallData(&mService, mCompletionQueue.get());
    new GetPriceCallData(&mService, mCompletionQueue.get());
    new TradeCallData(&mService, mCompletionQueue.get());

    const size_t THREADS = std::thread::hardware_concurrency() / 2;
    for (int i = 0; i < THREADS; i++)
    {
        std::thread(&AsyncMarketServer::HandleCall, this).detach();
    }
}

void AsyncMarketServer::Shutdown()
{
    spdlog::trace("Server shutdown.");
    if (mServer)
    {
        mServer->Shutdown();
    }
    if (mCompletionQueue)
    {
        mCompletionQueue->Shutdown();
    }
    mPriceGenerator.Stop();
}

void AsyncMarketServer::HandleCall()
{
    static std::atomic<uint8_t> threads_counter;
    std::stringstream ss;
    ss << std::this_thread::get_id();
    spdlog::info("Server's thread [{}] started [{}]", ss.str(), threads_counter++);

    void *tag;
    bool ok;

    while (mCompletionQueue->Next(&tag, &ok))
    {
        CallDataTag *dataTag = static_cast<CallDataTag *>(tag);

        if (!dataTag || !dataTag->mParent)
        {
            spdlog::error("CQ: CRITICAL - invalid tag or parent");
        }

        spdlog::info("CQ: Got tag {:p} ({}), ok: {} act: {}", tag, dataTag->mParent->GetTypeName(),
                     ok, CallDataTag::ToString(dataTag->actionType));
        dataTag->mParent->ProcessData(dataTag, ok);
    }
}

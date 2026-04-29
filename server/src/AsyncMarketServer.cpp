#include "../include/AsyncMarketServer.h"
#include "../include/GetPriceCallData.h"
#include "../include/SubscribePriceCallData.h"
#include "../include/SubscriberManager.h"
#include "../include/TradeCallData.h"
#include <cstddef>
#include <spdlog/spdlog.h>
#include <thread>

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

    // const uint THREADS = std::thread::hardware_concurrency();
    const size_t THREADS = 10;
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
    std::stringstream ss;
    ss << std::this_thread::get_id();
    spdlog::info("Server's thread [{}] started", ss.str());

    void *tag;
    bool ok;

    while (mCompletionQueue->Next(&tag, &ok))
    {
        static_cast<ICallDataBase *>(tag)->ProcessData(ok);
    }
}

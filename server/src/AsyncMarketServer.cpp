#include <spdlog/spdlog.h>
#include "../include/AsyncMarketServer.h"
#include "../include/SubscribePriceCallData.h"

void AsyncMarketServer::Run(const std::string &address)
{
    mPriceGenerator.SetCallback([](const std::string &symbol, double value)
                                { SubscriberManager::Instance().BroadcastPrice(symbol, value); });
    mPriceGenerator.Start();

    grpc::ServerBuilder serverBuilder;
    serverBuilder.AddListeningPort(address, grpc::InsecureServerCredentials());
    serverBuilder.RegisterService(&mService);

    mCompletionQueue = serverBuilder.AddCompletionQueue();
    mServer = serverBuilder.BuildAndStart();

    spdlog::info("Market Server started on {}", address);

    new SubscribePriceCallData(&mService, mCompletionQueue.get());

    const uint THREADS = std::thread::hardware_concurrency();
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
        static_cast<IMarketCallDataBase *>(tag)->ProcessData(ok);
    }
}

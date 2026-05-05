#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <thread>
#include <vector>

#include "market/v1/market.grpc.pb.h"

#include "PriceGenerator.h"

class AsyncMarketServer
{
public:
    void Run(const std::string &address);
    void Shutdown();

private:
    void HandleCall(std::stop_token stop_token, grpc::ServerCompletionQueue *queue);

    std::vector<std::unique_ptr<grpc::ServerCompletionQueue>> mCompletionQueues;
    std::vector<std::jthread> mThreads;
    market::v1::MarketService::AsyncService mService{};
    std::unique_ptr<grpc::Server> mServer;
    PriceGenerator mPriceGenerator;
};

#pragma once

#include <grpcpp/grpcpp.h>
#include "market/v1/market.grpc.pb.h"

#include "PriceGenerator.h"

class AsyncMarketServer
{
public:
    void Run(const std::string &address);
    void Shutdown();

private:
    void HandleCall();

    std::unique_ptr<grpc::ServerCompletionQueue> mCompletionQueue;
    market::v1::MarketService::AsyncService mService{};
    std::unique_ptr<grpc::Server> mServer;

    PriceGenerator mPriceGenerator;
};
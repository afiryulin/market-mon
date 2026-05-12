#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <stop_token>
#include <thread>
#include <vector>

#include "market/v1/market.grpc.pb.h"

class TradeClient
{
public:
    TradeClient(std::shared_ptr<grpc::Channel> channel);
    void Run(const std::string &symbol);

private:
    std::unique_ptr<market::v1::MarketService::Stub> mMarketStub;

    void TradeWriterFn(std::stop_token stop, const std::string &symbol, size_t fakeClientId);

private:
    std::vector<std::jthread> mClientThreads;
};

#pragma once

#include "market/v1/market.grpc.pb.h"
#include <grpcpp/channel.h>
#include <stop_token>
#include <thread>

class PriceClient final
{
public:
    PriceClient(std::shared_ptr<grpc::Channel> channel);
    void Subscribe(const std::string &symbol);

private:
    void ReadThreadFn(std::stop_token st, const std::string &symbol);

private:
    std::unique_ptr<market::v1::MarketService::Stub> mMarketStub;
    std::jthread mReadThread;
};

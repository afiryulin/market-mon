#include <spdlog/spdlog.h>

#include "PriceClient.h"
#include "market/v1/market.grpc.pb.h"

PriceClient::PriceClient(std::shared_ptr<grpc::Channel> channel)
    : mMarketStub(market::v1::MarketService::NewStub(channel))
{
}

void PriceClient::Subscribe(const std::string &symbol)
{
    market::v1::SubscribeRequest request;
    request.set_symbol(symbol);

    grpc::ClientContext context;
    auto reader = mMarketStub->SubscribePrices(&context, request);

    market::v1::PriceUpdate updPrice;
    while (reader->Read(&updPrice))
    {
        spdlog::info("PRICE: [{}] {} {}", updPrice.timestamp(), updPrice.symbol(), updPrice.price());
    }

    grpc::Status status = reader->Finish();

    if (!status.ok())
    {
        spdlog::error("RPC failed: {}", status.error_message());
    }
}

#include <chrono>
#include <format>
#include <spdlog/spdlog.h>

#include "PriceClient.h"
#include "market/v1/market.grpc.pb.h"

PriceClient::PriceClient(std::shared_ptr<grpc::Channel> channel)
    : mMarketStub(market::v1::MarketService::NewStub(channel))
{
}

void PriceClient::ReadThreadFn(std::stop_token st, const std::string &symbol)
{
    market::v1::PriceUpdate updPrice;
    market::v1::SubscribeRequest request;
    request.set_symbol(symbol);

    grpc::ClientContext context;
    auto reader = mMarketStub->SubscribePrices(&context, request);
    while (!st.stop_requested() && reader->Read(&updPrice))
    {
        auto tp = std::chrono::system_clock::from_time_t(updPrice.timestamp());
        std::string priceTime = std::format("{:%Y-%m-%d %H:%M:%S}", tp);
        spdlog::info("PRICE: [{}] {} {}", priceTime, updPrice.symbol(), updPrice.price());
    }

    grpc::Status status = reader->Finish();
    if (!status.ok())
    {
        spdlog::error("RPC failed: {}", status.error_message());

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

void PriceClient::Subscribe(const std::string &symbol)
{
    mReadThread = std::jthread(&PriceClient::ReadThreadFn, this, symbol);
}

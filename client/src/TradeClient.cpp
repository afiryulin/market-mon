#include <array>
#include <random>
#include <spdlog/spdlog.h>
#include <thread>

#include "TradeClient.h"
#include "market/v1/market.pb.h"

TradeClient::TradeClient(std::shared_ptr<grpc::Channel> channel)
    : mMarketStub(market::v1::MarketService::NewStub(channel))
{
}

void TradeClient::Run(const std::string &symbol)
{
    const uint32_t CLIENTS_COUNT = 15;

    for (size_t i{0}; i < CLIENTS_COUNT; i++)
    {
        mClientThreads.emplace_back(std::jthread(
            [this, i](std::stop_token stop, const std::string symbol) { TradeWriterFn(stop, symbol, i); }, symbol));
    }
}

void TradeClient::TradeWriterFn(std::stop_token stop, const std::string &symbol, size_t fakeClientId)
{
    grpc::ClientContext context;
    auto stream = mMarketStub->TradeStream(&context);

    spdlog::info("ClientTradeData");

    // std::mt19937 rng(std::random_device{}());
    // std::uniform_real_distribution<> distr(0, 1000);

    std::thread reader([&stream]() {
        market::v1::TradeEvent ev;
        while (stream->Read(&ev))
        {
            spdlog::info("TRADE Event: {} - {}", ev.symbol(), ev.status());
        }
    });

    for (int i = 0; i < 50; i++)
    {
        if (stop.stop_requested())
            return;

        market::v1::TradeRequest rq;
        rq.set_clientid(fakeClientId);
        rq.set_symbol(symbol);
        rq.set_quantity(1.0 + i);
        rq.set_is_buy(true);
        if (!stream->Write(rq))
        {
            spdlog::error("TraceClient: Failed to write to stream");
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    stream->WritesDone();
    if (reader.joinable())
        reader.join();

    auto status = stream->Finish();
    if (!status.ok())
        spdlog::error("Stream finished with error: {}", status.error_message());
}

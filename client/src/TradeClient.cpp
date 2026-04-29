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
    grpc::ClientContext context;
    auto stream = mMarketStub->TradeStream(&context);

    std::jthread writer = std::jthread(
        [this, &stream](std::stop_token stop, const std::string symbol) {
            TradeWriterFn(stop, stream, symbol);
        },
        symbol);

    market::v1::TradeEvent ev;
    while (stream->Read(&ev))
    {
        spdlog::info("TRADE Client: {}: {} - {}", ev.symbol(), ev.price(), ev.status());
    }

    stream->Finish();
}

void TradeClient::TradeWriterFn(
    std::stop_token stop,
    std::unique_ptr<
        ::grpc::ClientReaderWriter<::market::v1::TradeRequest, ::market::v1::TradeEvent>> &stream,
    const std::string &symbol)
{
    for (int i = 0; i < 10; i++)
    {
        if (stop.stop_requested())
            return;

        market::v1::TradeRequest rq;
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
}

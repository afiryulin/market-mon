#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>
#include <stop_token>
#include <thread>

#include "common/Config.h"
#include "market/v1/market.grpc.pb.h"
#include "market/v1/market.pb.h"

void CancellationOrderTest()
{
    using namespace market::v1;
    const std::string hostAddress = std::string(CLIENT_ADDRESS).append(":").append(SERVER_PORT);
    auto channel = grpc::CreateChannel(hostAddress, grpc::InsecureChannelCredentials());

    auto stub = market::v1::MarketService::NewStub(channel);

    grpc::ClientContext context;
    auto stream = stub->TradeStream(&context);

    std::jthread reader([&stream](std::stop_token st) {
        TradeEvent ev;
        while (stream->Read(&ev) && !st.stop_requested())
        {
            spdlog::info("Client: {} {} {} qty={} price={}", ev.clientid(), ev.symbol(), ev.status(), ev.quantity(),
                         ev.price());
        }
    });

    constexpr uint32_t client1_id = 1u;
    constexpr uint32_t client2_id = 2u;
    constexpr uint64_t order1_id = 100u;
    constexpr uint64_t order2_id = 200u;
    const std::string symbol = "USDT";

    TradeRequest buy;
    buy.set_type(market::v1::NEW_ORDER);

    buy.set_symbol(symbol);
    buy.set_price(1000);

    buy.set_quantity(10);

    buy.set_clientid(client1_id);
    buy.set_orderid(order1_id);

    buy.set_is_buy(true);
    buy.set_is_market_order(false);

    TradeRequest cancel;
    cancel.set_type(market::v1::CANCEL_ORDER);

    cancel.set_symbol(symbol);
    cancel.set_price(1000);

    cancel.set_clientid(client1_id);
    cancel.set_orderid(order1_id);

    TradeRequest sell;
    sell.set_type(market::v1::NEW_ORDER);

    sell.set_symbol(symbol);

    sell.set_clientid(client2_id);
    sell.set_orderid(order2_id);

    sell.set_price(990);
    sell.set_quantity(5);

    sell.set_is_buy(false);
    sell.set_is_market_order(false);

    bool wr = stream->Write(buy);

    if (!wr)
    {
        spdlog::error("Stream not wrote (buy)");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    wr = stream->Write(cancel);
    if (!wr)
    {
        spdlog::error("Stream not wrote (cancel)");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    wr = stream->Write(sell);
    if (!wr)
    {
        spdlog::error("Stream not wrote (sell)");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    stream->WritesDone();

    reader.request_stop();
    reader.join();

    auto status = stream->Finish();
    spdlog::info("TradeStream finish status ok={}", status.ok());
}

void MatchingEngineTest()
{
    using namespace market::v1;
    const std::string hostAddress = std::string(CLIENT_ADDRESS).append(":").append(SERVER_PORT);
    auto channel = grpc::CreateChannel(hostAddress, grpc::InsecureChannelCredentials());
    auto stub = market::v1::MarketService::NewStub(channel);

    grpc::ClientContext context1;
    grpc::ClientContext context2;
    grpc::ClientContext context3;
    auto stream1 = stub->TradeStream(&context1);
    auto stream2 = stub->TradeStream(&context2);
    auto stream3 = stub->TradeStream(&context3);

    std::jthread reader1([&stream1](const std::stop_token &st) {
        spdlog::info("Reader started");
        TradeEvent ev;
        while (stream1->Read(&ev) && !st.stop_requested())
        {
            spdlog::info("Client: {} {} {} qty={} price={}", ev.clientid(), ev.symbol(), ev.status(), ev.quantity(),
                         ev.price());
        }
    });

    std::jthread reader2([&stream2](const std::stop_token &st) {
        spdlog::info("Reader started");
        TradeEvent ev;
        while (stream2->Read(&ev))
        {
            spdlog::info("Client: {} {} {} qty={} price={}", ev.clientid(), ev.symbol(), ev.status(), ev.quantity(),
                         ev.price());
        }
    });

    std::jthread reader3([&stream3](const std::stop_token &st) {
        spdlog::info("Reader started");
        TradeEvent ev;
        while (stream3->Read(&ev))
        {
            spdlog::info("Client: {} {} {} qty={} price={}", ev.clientid(), ev.symbol(), ev.status(), ev.quantity(),
                         ev.price());
        }
    });

    TradeRequest rq1;
    rq1.set_symbol("USDT");
    rq1.set_price(1000);
    rq1.set_quantity(10);
    rq1.set_is_buy(true);
    rq1.set_clientid(1u);
    rq1.set_is_market_order(false);

    TradeRequest rq2;
    rq2.set_symbol("USDT");
    rq2.set_price(990);
    rq2.set_quantity(5);
    rq2.set_is_buy(false);
    rq2.set_clientid(2u);
    rq2.set_is_market_order(false);

    TradeRequest rq3;
    rq3.set_symbol("USDT");
    rq3.set_price(995);
    rq3.set_quantity(5);
    rq3.set_is_buy(false);
    rq3.set_clientid(3u);
    rq3.set_is_market_order(false);

    bool res1 = stream1->Write(rq1);
    bool res2 = stream2->Write(rq2);
    bool res3 = stream3->Write(rq3);

    if (res1 && res2 && res3)
    {
        spdlog::info("requests sent");
    }
}

int main()
{
    CancellationOrderTest();

    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
}

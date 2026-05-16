#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>
#include <stop_token>
#include <thread>

#include "common/Config.h"
#include "market/v1/market.grpc.pb.h"
#include "market/v1/market.pb.h"

int main()
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
        while (stream1->Read(&ev))
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

    return 0;
}

#include <gtest/gtest.h>

#include "AsyncMarketServer.h"
#include "TestTradeClient.h"

#include <chrono>
#include <thread>

namespace {
constexpr auto kAddress = "127.0.0.1:50055";

market::v1::TradeRequest MakeLimitOrder(uint32_t clientId, uint64_t orderId, const std::string &symbol, bool isBuy,
                                        uint32_t qty, double price)
{
    market::v1::TradeRequest req;
    req.set_type(market::v1::NEW_ORDER);
    req.set_clientid(clientId);
    req.set_orderid(orderId);
    req.set_symbol(symbol);
    req.set_is_buy(isBuy);
    req.set_quantity(qty);
    req.set_price(price);
    req.set_is_market_order(false);
    return req;
}

market::v1::TradeRequest MakeCancel(uint32_t clientId, uint64_t orderId, const std::string &symbol, double price = 0.0)
{
    market::v1::TradeRequest req;
    req.set_type(market::v1::CANCEL_ORDER);
    req.set_clientid(clientId);
    req.set_orderid(orderId);
    req.set_symbol(symbol);
    req.set_price(price);
    return req;
}

std::optional<market::v1::TradeEvent> WaitForStatus(TestTradeClient &client, uint64_t orderId,
                                                    const std::string &status)
{
    for (int i = 0; i < 10; ++i)
    {
        auto ev = client.ReadOne(std::chrono::milliseconds(1000));

        if (!ev.has_value())
        {
            continue;
        }

        if (ev->orderid() == orderId && ev->status() == status)
        {
            return *ev;
        }
    }

    return std::nullopt;
}

} // namespace

TEST(TradeStreamIntegrationTest, PartialAndFullFill)
{
    AsyncMarketServer server;
    server.Run("127.0.0.1:50056");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    TestTradeClient buyerClient("127.0.0.1:50056");
    TestTradeClient sellerClient("127.0.0.1:50056");

    ASSERT_TRUE(buyerClient.Write(MakeLimitOrder(1, 100, "USDT", true, 10, 1000)));
    auto buyAccepted = WaitForStatus(buyerClient, 100, "ACCEPTED");
    ASSERT_TRUE(buyAccepted.has_value()) << "Timeout waiting for ACCEPTED status";
    EXPECT_EQ(buyAccepted->quantity(), 10);

    ASSERT_TRUE(sellerClient.Write(MakeLimitOrder(2, 200, "USDT", false, 5, 990)));
    auto sell1Accepted = WaitForStatus(sellerClient, 200, "ACCEPTED");
    ASSERT_TRUE(sell1Accepted.has_value());
    EXPECT_EQ(sell1Accepted->quantity(), 5);

    auto sell1Filled = WaitForStatus(sellerClient, 200, "FILLED");
    ASSERT_TRUE(sell1Filled.has_value()) << "Seller did not get FILLED status";
    EXPECT_EQ(sell1Filled->quantity(), 5);
    EXPECT_DOUBLE_EQ(sell1Filled->price(), 1000);

    auto buyPartial = WaitForStatus(buyerClient, 1100, "PARTIALLY_FILLED");
    ASSERT_TRUE(buyPartial.has_value()) << "Buyer did not get PARTIALLY_FILLED status";
    EXPECT_EQ(buyPartial->quantity(), 5);
    EXPECT_DOUBLE_EQ(buyPartial->price(), 1000);

    ASSERT_TRUE(sellerClient.Write(MakeLimitOrder(2, 300, "USDT", false, 5, 995)));
    auto sell2Accepted = WaitForStatus(sellerClient, 300, "ACCEPTED");
    ASSERT_TRUE(sell2Accepted.has_value()) << "Timeout waiting for ACCEPTED status";
    EXPECT_EQ(sell2Accepted->quantity(), 5);

    auto sell2Filled = WaitForStatus(sellerClient, 300, "FILLED");
    ASSERT_TRUE(sell2Filled.has_value()) << "Timeout waiting for FILLED status";
    EXPECT_EQ(sell2Filled->clientid(), 2);
    EXPECT_EQ(sell2Filled->quantity(), 5);
    EXPECT_DOUBLE_EQ(sell2Filled->price(), 1000);

    auto buyFilled = WaitForStatus(buyerClient, 100, "FILLED");
    ASSERT_TRUE(buyFilled.has_value()) << "Timeout waiting for ACCEPTED status";
    EXPECT_EQ(buyFilled->quantity(), 5);
    EXPECT_DOUBLE_EQ(buyFilled->price(), 1000);

    sellerClient.Finish();
    buyerClient.Finish();
    server.Shutdown();
}

TEST(TradeStreamIntegrationTest, CancelPreventsFill)
{
    AsyncMarketServer server;
    server.Run("127.0.0.1:50057");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    TestTradeClient client("127.0.0.1:50057");

    ASSERT_TRUE(client.Write(MakeLimitOrder(1, 100, "USDT", true, 10, 1000)));
    ASSERT_TRUE(client.Write(MakeCancel(1, 100, "USDT", 1000)));
    ASSERT_TRUE(client.Write(MakeLimitOrder(2, 200, "USDT", false, 5, 990)));

    auto buyAccepted = WaitForStatus(client, 1, "ACCEPTED");
    ASSERT_TRUE(buyAccepted.has_value()) << "Timeout waiting for ACCEPTED status";
    EXPECT_EQ(buyAccepted->quantity(), 10);

    auto cancelled = WaitForStatus(client, 1, "CANCELLED");
    ASSERT_TRUE(cancelled.has_value()) << "Timeout waiting for ACCEPTED status";
    EXPECT_EQ(cancelled->orderid(), 100);

    auto sellAccepted = WaitForStatus(client, 2, "ACCEPTED");
    ASSERT_TRUE(sellAccepted.has_value()) << "Timeout waiting for ACCEPTED status";
    EXPECT_EQ(sellAccepted->quantity(), 5);

    auto maybeExtra = client.ReadOne(std::chrono::milliseconds(300));
    ASSERT_FALSE(maybeExtra.has_value());

    client.Finish();
    server.Shutdown();
}

#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "AsyncMarketServer.h"
#include "MatchingEngine.h"
#include "TestTradeClient.h"

#include "TradeStreamIntegrationTest.h"

namespace {

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

std::optional<market::v1::TradeEvent> WaitForOrderStatus(TestTradeClient &client, uint64_t orderId,
                                                         const std::string &status)
{
    for (int i = 0; i < 10; ++i)
    {
        auto ev = client.ReadOne(std::chrono::milliseconds(500));

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

TEST_F(TradeStreamIntegrationTest, PartialAndFullFill)
{

    auto buyer = std::make_unique<TestTradeClient>(address);
    auto seller = std::make_unique<TestTradeClient>(address);

    ASSERT_TRUE(buyer->Write(MakeLimitOrder(1, 100, "USDT", true, 10, 1000)));
    auto buyAccepted = WaitForOrderStatus(*buyer, 100, "ACCEPTED");
    ASSERT_TRUE(buyAccepted.has_value()) << "Timeout waiting for ACCEPTED status";
    EXPECT_EQ(buyAccepted->quantity(), 10);

    ASSERT_TRUE(seller->Write(MakeLimitOrder(2, 200, "USDT", false, 5, 990)));
    auto sell1Accepted = WaitForOrderStatus(*seller, 200, "ACCEPTED");
    ASSERT_TRUE(sell1Accepted.has_value());
    EXPECT_EQ(sell1Accepted->quantity(), 5);

    auto sell1Filled = WaitForOrderStatus(*seller, 200, "FILLED");
    ASSERT_TRUE(sell1Filled.has_value()) << "Seller did not get FILLED status";
    EXPECT_EQ(sell1Filled->quantity(), 5);
    EXPECT_DOUBLE_EQ(sell1Filled->price(), 1000);

    auto buyPartial = WaitForOrderStatus(*buyer, 100, "PARTIALLY_FILLED");
    ASSERT_TRUE(buyPartial.has_value()) << "Buyer did not get PARTIALLY_FILLED status";
    EXPECT_EQ(buyPartial->quantity(), 5);
    EXPECT_DOUBLE_EQ(buyPartial->price(), 1000);

    ASSERT_TRUE(seller->Write(MakeLimitOrder(2, 300, "USDT", false, 5, 995)));
    auto sell2Accepted = WaitForOrderStatus(*seller, 300, "ACCEPTED");
    ASSERT_TRUE(sell2Accepted.has_value()) << "Timeout waiting for ACCEPTED status";
    EXPECT_EQ(sell2Accepted->quantity(), 5);

    auto sell2Filled = WaitForOrderStatus(*seller, 300, "FILLED");
    ASSERT_TRUE(sell2Filled.has_value()) << "Timeout waiting for FILLED status";
    EXPECT_EQ(sell2Filled->clientid(), 2);
    EXPECT_EQ(sell2Filled->quantity(), 5);
    EXPECT_DOUBLE_EQ(sell2Filled->price(), 1000);

    auto buyFilled = WaitForOrderStatus(*buyer, 100, "FILLED");
    ASSERT_TRUE(buyFilled.has_value()) << "Timeout waiting for ACCEPTED status";
    EXPECT_EQ(buyFilled->quantity(), 5);
    EXPECT_DOUBLE_EQ(buyFilled->price(), 1000);
}

TEST_F(TradeStreamIntegrationTest, CancelPreventsFill)
{
    auto buyer = std::make_unique<TestTradeClient>(address);
    auto seller = std::make_unique<TestTradeClient>(address);

    ASSERT_TRUE(buyer->Write(MakeLimitOrder(1, 100, "USDT", true, 10, 1000)));
    auto buyAccepted = WaitForOrderStatus(*buyer, 100, "ACCEPTED");
    ASSERT_TRUE(buyAccepted.has_value()) << "Timeout waiting for ACCEPTED status";
    EXPECT_EQ(buyAccepted->quantity(), 10);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    ASSERT_TRUE(buyer->Write(MakeCancel(1, 100, "USDT", 1000)));
    auto cancelled = WaitForOrderStatus(*buyer, 100, "CANCELLED");
    ASSERT_TRUE(cancelled.has_value()) << "Timeout waiting for CANCEL status";
    EXPECT_EQ(cancelled->orderid(), 100);

    ASSERT_TRUE(seller->Write(MakeLimitOrder(2, 200, "USDT", false, 5, 990)));
    auto sellAccepted = WaitForOrderStatus(*seller, 200, "ACCEPTED");
    ASSERT_TRUE(sellAccepted.has_value()) << "Timeout waiting for ACCEPTED status";
    EXPECT_EQ(sellAccepted->quantity(), 5);

    auto maybeExtraBuyer = buyer->ReadOne(std::chrono::milliseconds(300));
    ASSERT_FALSE(maybeExtraBuyer.has_value());
    auto maybeExtraSeller = seller->ReadOne(std::chrono::milliseconds(300));
    ASSERT_FALSE(maybeExtraSeller.has_value());
}

#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "AsyncMarketServer.h"
#include "MatchingEngine.h"

#include "TestTradeClient.h"

class TradeStreamIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MatchingEngine::Instance().ResetForTesting();

        server = std::make_unique<AsyncMarketServer>();
        server->Run(address);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override
    {
        buyer.reset();
        seller.reset();

        if (server)
        {
            server->Shutdown();
            server.reset();
        }

        MatchingEngine::Instance().ResetForTesting();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

protected:
    const std::string address = "127.0.0.1:50056";

    std::unique_ptr<AsyncMarketServer> server;

    std::unique_ptr<TestTradeClient> buyer;
    std::unique_ptr<TestTradeClient> seller;
};

#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <spdlog/spdlog.h>

#include "../common/Config.h"
#include "PriceClient.h"
#include "TradeClient.h"

#include <chrono>
#include <condition_variable>
#include <grpcpp/grpcpp.h>
#include <mutex>
#include <thread>

bool waitChennel(std::shared_ptr<grpc::Channel> channel, int timeout)
{
    std::mutex mtx;
    std::condition_variable cv;
    bool connected{false};
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(timeout);

    std::jthread checker([&](std::stop_token st) {
        grpc_connectivity_state chState;
        while (!st.stop_requested() && (chState = channel->GetState(true)) != GRPC_CHANNEL_READY)
        {
            if (!channel->WaitForStateChange(chState, deadline))
                break;
        }

        std::lock_guard<std::mutex> locker(mtx);
        if (chState == GRPC_CHANNEL_READY)
            connected = true;

        cv.notify_one();
    });

    std::unique_lock<std::mutex> locker(mtx);

    for (int elapsed{1}; elapsed < timeout; elapsed++)
    {
        cv.wait_for(locker, std::chrono::seconds(1), [&] { return connected; });
        if (connected)
            break;
        spdlog::info("Connecting... [{} / {}] s", elapsed, timeout);
    }

    return connected;
}

int main()
{
    using namespace market::v1;
    const std::string hostAddress = std::string(CLIENT_ADDRESS).append(":").append(SERVER_PORT);

    auto marketChannel = grpc::CreateChannel(hostAddress, grpc::InsecureChannelCredentials());

    if (!waitChennel(marketChannel, 20))
    {
        spdlog::error("Server at [{}] is not available.", hostAddress);
        return 1;
    }

    PriceClient client{marketChannel};
    client.Subscribe("ETH");

    TradeClient tradeClient{marketChannel};
    tradeClient.Run("ETH");

    spdlog::info("Client completed. Press any cay to exit...");
    std::cin.get();

    return 0;
}

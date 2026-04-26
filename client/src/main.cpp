#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <spdlog/spdlog.h>

#include "../common/Config.h"
#include "PriceClient.h"
#include "TradeClient.h"

int main()
{
    using namespace market::v1;
    const std::string hostAddress = std::string(CLIENT_ADDRESS).append(":").append(SERVER_PORT);

    auto marketChannel = grpc::CreateChannel(hostAddress, grpc::InsecureChannelCredentials());

    PriceClient client{marketChannel};
    client.Subscribe("ETH");

    TradeClient tradeClient{marketChannel};
    tradeClient.Run("ETH");

    std::cin.get();

    return 0;
}
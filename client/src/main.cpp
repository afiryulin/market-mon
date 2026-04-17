#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <spdlog/spdlog.h>

#include "market/v1/market.grpc.pb.h"
#include "../common/Config.h"
#include "PriceClient.h"

int main()
{
    using namespace market::v1;
    const std::string hostAddress = std::string(CLIENT_ADDRESS).append(":").append(SERVER_PORT);

    auto marketChannel = grpc::CreateChannel(hostAddress, grpc::InsecureChannelCredentials());

    PriceClient client{marketChannel};

    client.Subscribe("ETH");

    return 0;
}
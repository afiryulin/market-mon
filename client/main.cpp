#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <spdlog/spdlog.h>

#include "market/v1/market.grpc.pb.h"
#include "common/Config.h"

int main()
{
    using namespace market::v1;
    const std::string hostAddress = std::string(CLIENT_ADDRESS).append(":").append(SERVER_PORT);

    auto marketChannel = grpc::CreateChannel(hostAddress, grpc::InsecureChannelCredentials());
    auto stub = MarketService::NewStub(marketChannel);

    grpc::ClientContext context;
    SubscribeRequest request;
    request.set_symbol("ETH");
    auto reader = stub->SubscribePrices(&context, request);

    PriceUpdate response;
    while (reader->Read(&response))
    {
        spdlog::info("Update: {}", response.symbol());
    }

    return 0;
}
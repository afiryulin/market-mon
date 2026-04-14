#include <spdlog/spdlog.h>
#include "../include/AsyncMarketServer.h"

int main()
{
    const std::string address("0.0.0.0:50050");
    AsyncMarketServer server;
    server.Run(address);

    server.Shutdown();
    return 0;
}
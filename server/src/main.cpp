#include "../include/AsyncMarketServer.h"
#include "common/Config.h"
#include <spdlog/spdlog.h>

int main()
{
    const std::string address = std::string(SERVICE_HOST_ADDRESS).append(":").append(SERVER_PORT);
    AsyncMarketServer server;
    server.Run(address);

    // Wait for shutdown signal (e.g., Ctrl+C)
    // For testing, you can use st::cin.get() to wait for Enter
    std::cin.get();

    server.Shutdown();
    return 0;
}

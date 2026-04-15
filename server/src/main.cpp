#include <spdlog/spdlog.h>
#include "../include/AsyncMarketServer.h"

int main()
{
    const std::string address("0.0.0.0:50051");
    AsyncMarketServer server;
    server.Run(address);

    // Wait for shutdown signal (e.g., Ctrl+C)
    // For testing, you can use std::cin.get() to wait for Enter
    std::cout << "Press Enter to shutdown..." << std::endl;
    std::cin.get();

    server.Shutdown();
    return 0;
}
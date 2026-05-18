#include "AsyncMarketServer.h"
#include "common/Config.h"
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

void SetupAsyncLogging()
{
    spdlog::init_thread_pool(8192, 1);

    auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/market_server.log", true);

    std::vector<spdlog::sink_ptr> sinks{stdout_sink, file_sink};
    auto logger =
        std::make_shared<spdlog::async_logger>("market_logger", sinks.begin(), sinks.end(), spdlog::thread_pool(),
                                               spdlog::async_overflow_policy::overrun_oldest);

    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");
}

int main()
{
    SetupAsyncLogging();
    const std::string address = std::string(SERVICE_HOST_ADDRESS).append(":").append(SERVER_PORT);
    AsyncMarketServer server;
    server.Run(address);

    // Wait for shutdown signal (e.g., Ctrl+C)
    // For testing, you can use st::cin.get() to wait for Enter
    std::cin.get();

    server.Shutdown();
    return 0;
}

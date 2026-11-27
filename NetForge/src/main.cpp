/**
 * NetForge — main.cpp  (cross-platform)
 *
 * Usage:
 *   ./netforge [--port <n>] [--workers <n>] [--log-level debug|info|warn|error]
 */

#include "netforge/Logger.hpp"
#include "netforge/Platform.hpp"
#include "netforge/TcpServer.hpp"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>

static netforge::TcpServer* g_server = nullptr;

static void sigHandler(int /*sig*/) {
    if (g_server) g_server->stop();
}

static void printHelp() {
    std::cout <<
        "NetForge — High-Performance Concurrent TCP Server\n\n"
        "Usage: netforge [options]\n"
        "  --port      <n>  Listening port            (default: 8080)\n"
        "  --workers   <n>  Thread pool worker count  (default: 4)\n"
        "  --log-level <l>  debug|info|warn|error     (default: info)\n"
        "  --help           Show this message\n";
}

int main(int argc, char* argv[]) {
    netforge::platformInit();     // Winsock startup on Windows, no-op on Linux

    netforge::ServerConfig cfg;
    auto& log = netforge::Logger::instance();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            printHelp(); return 0;
        } else if (arg == "--port" && i + 1 < argc) {
            cfg.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--workers" && i + 1 < argc) {
            cfg.workerThreads = static_cast<std::size_t>(std::stoi(argv[++i]));
        } else if (arg == "--log-level" && i + 1 < argc) {
            std::string level = argv[++i];
            if      (level == "debug") log.setLevel(netforge::LogLevel::DEBUG);
            else if (level == "info")  log.setLevel(netforge::LogLevel::INFO);
            else if (level == "warn")  log.setLevel(netforge::LogLevel::WARN);
            else if (level == "error") log.setLevel(netforge::LogLevel::ERR);
            else { std::cerr << "Unknown log level: " << level << '\n'; return 1; }
        } else {
            std::cerr << "Unknown argument: " << arg << " (try --help)\n";
            return 1;
        }
    }

    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    try {
        netforge::TcpServer server(cfg);
        g_server = &server;
        server.run();
    } catch (const std::exception& ex) {
        NF_ERROR("Fatal: ", ex.what());
        netforge::platformCleanup();
        return 1;
    }

    NF_INFO("NetForge shut down cleanly.");
    netforge::platformCleanup();
    return 0;
}

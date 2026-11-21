/**
 * NetForge — client.cpp  (cross-platform)
 *
 * Usage:  netforge_client [--host <ip>] [--port <n>]
 * Commands: PING  ECHO <msg>  TIME  STATS  QUIT
 */

#include "netforge/Platform.hpp"

#include <iostream>
#include <string>
#include <cstdio>

int main(int argc, char* argv[]) {
    netforge::platformInit();

    std::string host = "127.0.0.1";
    uint16_t    port = 8080;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "--host" && i+1<argc) host = argv[++i];
        else if (a == "--port" && i+1<argc) port = static_cast<uint16_t>(std::stoi(argv[++i]));
    }

#ifdef _WIN32
    SOCKET fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == INVALID_SOCKET) { std::cerr << "socket failed\n"; return 1; }
#else
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) { perror("socket"); return 1; }
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    ::inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

#ifdef _WIN32
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "connect failed: " << netforge::lastNetError() << '\n';
        ::closesocket(fd); return 1;
    }
#else
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        perror("connect"); ::close(fd); return 1;
    }
#endif

    std::cout << "Connected to " << host << ':' << port
              << "  (PING / ECHO <msg> / TIME / STATS / HELP / QUIT)\n> " << std::flush;

    char respBuf[4096];
    std::string line;
    while (std::getline(std::cin, line)) {
        line += '\n';
#ifdef _WIN32
        ::send(fd, line.data(), static_cast<int>(line.size()), 0);
        int n = ::recv(fd, respBuf, sizeof(respBuf)-1, 0);
#else
        ::send(fd, line.data(), line.size(), MSG_NOSIGNAL);
        ssize_t n = ::recv(fd, respBuf, sizeof(respBuf)-1, 0);
#endif
        if (n <= 0) { std::cout << "[connection closed]\n"; break; }
        respBuf[n] = '\0';
        std::cout << respBuf;
        if (line.rfind("QUIT", 0) == 0) break;
        std::cout << "> " << std::flush;
    }

#ifdef _WIN32
    ::closesocket(fd);
#else
    ::close(fd);
#endif
    netforge::platformCleanup();
    return 0;
}

#pragma once
/**
 * NetForge — Platform.hpp
 *
 * Hides every OS-specific networking include and type alias behind one header.
 * Include this before any socket work.
 *
 * Windows: Winsock2 + ws2_32.lib
 * Linux:   POSIX sockets
 */

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>

  // POSIX-style aliases for Windows
  using ssize_t  = SSIZE_T;
  using socklen_t = int;

  // Map POSIX error names to Winsock equivalents
  #define EWOULDBLOCK WSAEWOULDBLOCK
  #define EAGAIN      WSAEWOULDBLOCK
  #define EINTR       WSAEINTR

  inline int netErrno() { return WSAGetLastError(); }

  // MSG_NOSIGNAL does not exist on Windows (no SIGPIPE either)
  #ifndef MSG_NOSIGNAL
    #define MSG_NOSIGNAL 0
  #endif

  // inet_ntop is in ws2_32 on Vista+, but the include pulls it in already
#else
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <sys/socket.h>
  #include <unistd.h>
  #include <cerrno>
  #include <cstring>

  inline int netErrno() { return errno; }
#endif

#include <string>

namespace netforge {

/// One-time Winsock initialisation on Windows; no-op on Linux.
inline void platformInit() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        throw std::runtime_error("WSAStartup failed");
#endif
}

inline void platformCleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

/// Close a socket handle (Winsock uses closesocket, POSIX uses close).
inline void closeSocket(int fd) {
#ifdef _WIN32
    ::closesocket(static_cast<SOCKET>(fd));
#else
    ::close(fd);
#endif
}

/// Returns a human-readable string for the last network error.
inline std::string lastNetError() {
#ifdef _WIN32
    char buf[256]{};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, WSAGetLastError(), 0, buf, sizeof(buf), nullptr);
    return buf;
#else
    return strerror(errno);
#endif
}

} // namespace netforge

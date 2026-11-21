#pragma once
/**
 * NetForge — Protocol.hpp
 *
 * Simple line-delimited text protocol.
 *
 * Request wire format  (client → server):
 *   COMMAND [space-separated args...]\n
 *
 * Supported commands:
 *   PING              → PONG\n
 *   ECHO <text>       → <text>\n
 *   TIME              → ISO-8601 timestamp\n
 *   STATS             → JSON worker stats snapshot\n
 *   QUIT              → BYE\n  (then server closes connection)
 *
 * Unknown command     → ERROR unknown command\n
 *
 * Design: pure data types + free functions; no I/O.
 * The TcpServer / ConnectionHandler own the socket; the Protocol just
 * translates strings, keeping it unit-testable without networking.
 */

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace netforge::protocol {

struct Request {
    std::string command;
    std::string args;
};

struct Response {
    std::string body;
    bool        closeAfter{false};  // set for QUIT
};

// Parse one '\n'-terminated line into a Request.
inline Request parse(std::string_view line) {
    // Strip trailing \r\n
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.remove_suffix(1);

    auto space = line.find(' ');
    if (space == std::string_view::npos)
        return {std::string(line), {}};

    return {
        std::string(line.substr(0, space)),
        std::string(line.substr(space + 1))
    };
}

inline std::string currentIso8601() {
    using namespace std::chrono;
    auto now   = system_clock::now();
    auto timer = system_clock::to_time_t(now);
    std::tm bt{};
#ifdef _WIN32
    gmtime_s(&bt, &timer);
#else
    gmtime_r(&timer, &bt);
#endif
    std::ostringstream ss;
    ss << std::put_time(&bt, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// Dispatch a request to a response (stateless; server state injected as arg).
// statsJson is provided by the server so Protocol stays pure.
inline Response dispatch(const Request& req,
                         const std::string& statsJson = "{}") {
    if (req.command == "PING") {
        return {"PONG\n"};
    }
    if (req.command == "ECHO") {
        return {req.args + "\n"};
    }
    if (req.command == "TIME") {
        return {currentIso8601() + "\n"};
    }
    if (req.command == "STATS") {
        return {statsJson + "\n"};
    }
    if (req.command == "QUIT") {
        return {"BYE\n", true};
    }
    return {"ERROR unknown command: " + req.command + "\n"};
}

} // namespace netforge::protocol

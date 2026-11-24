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
 *   PING                → PONG\n
 *   ECHO <text>         → <text>\n
 *   TIME                → ISO-8601 timestamp\n
 *   STATS               → JSON worker stats snapshot\n
 *   VERSION             → NetForge 1.0.0\n
 *   UPTIME              → seconds since server started\n
 *   CONNECTIONS         → current active connection count\n
 *   WHOAMI              → client IP:port\n
 *   REPEAT <n> <msg>    → msg repeated n times\n
 *   HELP                → list of commands\n
 *   QUIT                → BYE\n  (then server closes connection)
 *
 * Unknown command       → ERROR unknown command\n
 *
 * Design: pure data types + free functions; no I/O.
 * The TcpServer / ConnectionHandler own the socket; the Protocol just
 * translates strings, keeping it unit-testable without networking.
 */

#include <cctype>
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

// Extra server-side context injected into dispatch.
struct ServerContext {
    std::string statsJson     = "{}";
    std::string clientAddr    = "unknown";   // "ip:port" of the connected client
    std::size_t connections   = 0;           // current active connection count
    uint64_t    uptimeSeconds = 0;           // seconds since server started
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

// Dispatch a request to a response.
inline Response dispatch(const Request& req,
                         const ServerContext& ctx = {}) {
    // Normalise to uppercase so "ping", "Ping", "PING" all work
    std::string cmd = req.command;
    for (auto& c : cmd) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (cmd == "PING") {
        return {"PONG\n"};
    }
    if (cmd == "ECHO") {
        return {req.args + "\n"};
    }
    if (cmd == "TIME") {
        return {currentIso8601() + "\n"};
    }
    if (cmd == "STATS") {
        return {ctx.statsJson + "\n"};
    }
    if (cmd == "VERSION") {
        return {"NetForge 1.0.0\n"};
    }
    if (cmd == "UPTIME") {
        auto h = ctx.uptimeSeconds / 3600;
        auto m = (ctx.uptimeSeconds % 3600) / 60;
        auto s = ctx.uptimeSeconds % 60;
        std::ostringstream oss;
        oss << h << "h " << m << "m " << s << "s\n";
        return {oss.str()};
    }
    if (cmd == "CONNECTIONS") {
        return {std::to_string(ctx.connections) + "\n"};
    }
    if (cmd == "WHOAMI") {
        return {ctx.clientAddr + "\n"};
    }
    if (cmd == "REPEAT") {
        // Format: REPEAT <n> <msg>
        auto space = req.args.find(' ');
        if (space == std::string::npos)
            return {"ERROR usage: REPEAT <n> <msg>\n"};
        int n = 0;
        try { n = std::stoi(req.args.substr(0, space)); }
        catch (...) { return {"ERROR <n> must be a number\n"}; }
        if (n <= 0 || n > 100)
            return {"ERROR <n> must be between 1 and 100\n"};
        std::string msg = req.args.substr(space + 1);
        std::string result;
        for (int i = 0; i < n; ++i) result += msg + "\n";
        return {result};
    }
    if (cmd == "HELP") {
        return {"Commands: PING | ECHO <msg> | TIME | STATS | VERSION | "
                "UPTIME | CONNECTIONS | WHOAMI | REPEAT <n> <msg> | QUIT\n"};
    }
    if (cmd == "QUIT") {
        return {"BYE\n", true};
    }
    return {"ERROR unknown command: " + req.command + "\n"};
}

} // namespace netforge::protocol

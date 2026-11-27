/**
 * Tests for the wire protocol parser and dispatcher.
 * No networking involved — pure string logic.
 */

#include "netforge/Protocol.hpp"

#include <gtest/gtest.h>
#include <string>

using namespace netforge::protocol;

// ── Parsing ──────────────────────────────────────────────────────────────────

TEST(Protocol, ParsePing) {
    auto r = parse("PING\n");
    EXPECT_EQ(r.command, "PING");
    EXPECT_EQ(r.args,    "");
}

TEST(Protocol, ParseEchoWithArgs) {
    auto r = parse("ECHO hello world\n");
    EXPECT_EQ(r.command, "ECHO");
    EXPECT_EQ(r.args,    "hello world");
}

TEST(Protocol, ParseStripsCarriageReturn) {
    auto r = parse("PING\r\n");
    EXPECT_EQ(r.command, "PING");
}

TEST(Protocol, ParseNoArgs) {
    auto r = parse("QUIT\n");
    EXPECT_EQ(r.command, "QUIT");
    EXPECT_EQ(r.args,    "");
}

// ── Dispatch — existing commands ─────────────────────────────────────────────

TEST(Protocol, PingReturnsPong) {
    auto resp = dispatch({"PING", {}});
    EXPECT_EQ(resp.body, "PONG\n");
    EXPECT_FALSE(resp.closeAfter);
}

TEST(Protocol, EchoReturnsArgs) {
    auto resp = dispatch({"ECHO", "hello"});
    EXPECT_EQ(resp.body, "hello\n");
}

TEST(Protocol, TimeReturnsNonEmpty) {
    auto resp = dispatch({"TIME", {}});
    EXPECT_FALSE(resp.body.empty());
    EXPECT_NE(resp.body.find('T'), std::string::npos);
}

TEST(Protocol, StatsReturnsJson) {
    std::string json = R"({"workers":4})";
    ServerContext ctx;
    ctx.statsJson = json;
    auto resp = dispatch({"STATS", {}}, ctx);
    EXPECT_EQ(resp.body, json + "\n");
}

TEST(Protocol, QuitSetsCloseAfter) {
    auto resp = dispatch({"QUIT", {}});
    EXPECT_EQ(resp.body, "BYE\n");
    EXPECT_TRUE(resp.closeAfter);
}

TEST(Protocol, UnknownCommandReturnsError) {
    auto resp = dispatch({"FOOBAR", {}});
    EXPECT_EQ(resp.body.substr(0, 5), "ERROR");
}

// ── Case-insensitive dispatch ─────────────────────────────────────────────────

TEST(Protocol, PingLowercaseWorks) {
    auto resp = dispatch({"ping", {}});
    EXPECT_EQ(resp.body, "PONG\n");
}

TEST(Protocol, EchoMixedCaseWorks) {
    auto resp = dispatch({"Echo", "hi"});
    EXPECT_EQ(resp.body, "hi\n");
}

TEST(Protocol, QuitLowercaseSetsCloseAfter) {
    auto resp = dispatch({"quit", {}});
    EXPECT_TRUE(resp.closeAfter);
    EXPECT_EQ(resp.body, "BYE\n");
}

// ── VERSION ───────────────────────────────────────────────────────────────────

TEST(Protocol, VersionReturnsString) {
    auto resp = dispatch({"VERSION", {}});
    EXPECT_EQ(resp.body, "NetForge 1.0.0\n");
    EXPECT_FALSE(resp.closeAfter);
}

TEST(Protocol, VersionLowercaseWorks) {
    auto resp = dispatch({"version", {}});
    EXPECT_EQ(resp.body, "NetForge 1.0.0\n");
}

// ── UPTIME ────────────────────────────────────────────────────────────────────

TEST(Protocol, UptimeReturnsFormattedString) {
    ServerContext ctx;
    ctx.uptimeSeconds = 3661; // 1h 1m 1s
    auto resp = dispatch({"UPTIME", {}}, ctx);
    EXPECT_EQ(resp.body, "1h 1m 1s\n");
}

TEST(Protocol, UptimeZeroSeconds) {
    ServerContext ctx;
    ctx.uptimeSeconds = 0;
    auto resp = dispatch({"UPTIME", {}}, ctx);
    EXPECT_EQ(resp.body, "0h 0m 0s\n");
}

TEST(Protocol, UptimeLargeValue) {
    ServerContext ctx;
    ctx.uptimeSeconds = 86400; // exactly 24h
    auto resp = dispatch({"UPTIME", {}}, ctx);
    EXPECT_EQ(resp.body, "24h 0m 0s\n");
}

// ── CONNECTIONS ───────────────────────────────────────────────────────────────

TEST(Protocol, ConnectionsReturnsCount) {
    ServerContext ctx;
    ctx.connections = 7;
    auto resp = dispatch({"CONNECTIONS", {}}, ctx);
    EXPECT_EQ(resp.body, "7\n");
}

TEST(Protocol, ConnectionsZero) {
    ServerContext ctx;
    ctx.connections = 0;
    auto resp = dispatch({"CONNECTIONS", {}}, ctx);
    EXPECT_EQ(resp.body, "0\n");
}

// ── WHOAMI ────────────────────────────────────────────────────────────────────

TEST(Protocol, WhoamiReturnsClientAddr) {
    ServerContext ctx;
    ctx.clientAddr = "192.168.1.1:54321";
    auto resp = dispatch({"WHOAMI", {}}, ctx);
    EXPECT_EQ(resp.body, "192.168.1.1:54321\n");
}

TEST(Protocol, WhoamiDefaultUnknown) {
    auto resp = dispatch({"WHOAMI", {}});
    EXPECT_EQ(resp.body, "unknown\n");
}

// ── REPEAT ────────────────────────────────────────────────────────────────────

TEST(Protocol, RepeatOnce) {
    auto resp = dispatch({"REPEAT", "1 hello"});
    EXPECT_EQ(resp.body, "hello\n");
}

TEST(Protocol, RepeatMultiple) {
    auto resp = dispatch({"REPEAT", "3 hi"});
    EXPECT_EQ(resp.body, "hi\nhi\nhi\n");
}

TEST(Protocol, RepeatNoSpaceReturnsError) {
    auto resp = dispatch({"REPEAT", "3"});
    EXPECT_EQ(resp.body.substr(0, 5), "ERROR");
}

TEST(Protocol, RepeatZeroReturnsError) {
    auto resp = dispatch({"REPEAT", "0 hello"});
    EXPECT_EQ(resp.body.substr(0, 5), "ERROR");
}

TEST(Protocol, RepeatNegativeReturnsError) {
    auto resp = dispatch({"REPEAT", "-1 hello"});
    EXPECT_EQ(resp.body.substr(0, 5), "ERROR");
}

TEST(Protocol, RepeatTooLargeReturnsError) {
    auto resp = dispatch({"REPEAT", "101 hello"});
    EXPECT_EQ(resp.body.substr(0, 5), "ERROR");
}

TEST(Protocol, RepeatNonNumericReturnsError) {
    auto resp = dispatch({"REPEAT", "abc hello"});
    EXPECT_EQ(resp.body.substr(0, 5), "ERROR");
}

TEST(Protocol, RepeatMaxAllowed) {
    auto resp = dispatch({"REPEAT", "100 x"});
    // Should have exactly 100 lines
    int count = 0;
    for (char c : resp.body) if (c == '\n') count++;
    EXPECT_EQ(count, 100);
}

// ── HELP ──────────────────────────────────────────────────────────────────────

TEST(Protocol, HelpContainsAllCommands) {
    auto resp = dispatch({"HELP", {}});
    EXPECT_NE(resp.body.find("PING"),        std::string::npos);
    EXPECT_NE(resp.body.find("ECHO"),        std::string::npos);
    EXPECT_NE(resp.body.find("TIME"),        std::string::npos);
    EXPECT_NE(resp.body.find("STATS"),       std::string::npos);
    EXPECT_NE(resp.body.find("VERSION"),     std::string::npos);
    EXPECT_NE(resp.body.find("UPTIME"),      std::string::npos);
    EXPECT_NE(resp.body.find("CONNECTIONS"), std::string::npos);
    EXPECT_NE(resp.body.find("WHOAMI"),      std::string::npos);
    EXPECT_NE(resp.body.find("REPEAT"),      std::string::npos);
    EXPECT_NE(resp.body.find("HELP"),        std::string::npos);
    EXPECT_NE(resp.body.find("QUIT"),        std::string::npos);
}

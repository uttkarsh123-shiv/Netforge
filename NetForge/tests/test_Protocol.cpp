/**
 * Tests for the wire protocol parser and dispatcher.
 * No networking involved — pure string logic.
 */

#include "netforge/Protocol.hpp"

#include <gtest/gtest.h>
#include <string>

using namespace netforge::protocol;

// ── Parsing ─────────────────────────────────────────────────────────────────

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

// ── Dispatch ─────────────────────────────────────────────────────────────────

TEST(Protocol, PingReturnsPong) {
    auto resp = dispatch({"PING", {}});
    EXPECT_EQ(resp.body,       "PONG\n");
    EXPECT_FALSE(resp.closeAfter);
}

TEST(Protocol, EchoReturnsArgs) {
    auto resp = dispatch({"ECHO", "hello"});
    EXPECT_EQ(resp.body, "hello\n");
}

TEST(Protocol, TimeReturnsNonEmpty) {
    auto resp = dispatch({"TIME", {}});
    EXPECT_FALSE(resp.body.empty());
    // ISO-8601 always contains 'T'
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

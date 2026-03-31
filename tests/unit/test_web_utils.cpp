#include "elero_web/elero_web_utils.h"
#include <gtest/gtest.h>

using namespace esphome::elero::web_utils;

// --- json_escape ---

TEST(JsonEscape, EmptyString) { EXPECT_EQ(json_escape(""), ""); }
TEST(JsonEscape, PlainAscii) { EXPECT_EQ(json_escape("hello world"), "hello world"); }
TEST(JsonEscape, Quotes) { EXPECT_EQ(json_escape("say \"hi\""), "say \\\"hi\\\""); }
TEST(JsonEscape, Backslash) { EXPECT_EQ(json_escape("path\\to"), "path\\\\to"); }
TEST(JsonEscape, Newline) { EXPECT_EQ(json_escape("line1\nline2"), "line1\\nline2"); }
TEST(JsonEscape, Tab) { EXPECT_EQ(json_escape("col1\tcol2"), "col1\\tcol2"); }
TEST(JsonEscape, CarriageReturn) { EXPECT_EQ(json_escape("a\rb"), "a\\rb"); }

TEST(JsonEscape, ControlChar) {
  std::string input(1, '\x01');  // SOH
  EXPECT_EQ(json_escape(input), "\\u0001");
}

TEST(JsonEscape, NullByte) {
  std::string input(1, '\x00');
  EXPECT_EQ(json_escape(input), "\\u0000");
}

TEST(JsonEscape, AllControlChars) {
  for (int c = 0; c < 0x20; c++) {
    if (c == '\n' || c == '\r' || c == '\t') continue;  // These have special escapes
    std::string input(1, static_cast<char>(c));
    std::string result = json_escape(input);
    EXPECT_EQ(result.substr(0, 2), "\\u") << "Control char 0x" << std::hex << c << " not escaped as \\u";
    EXPECT_EQ(result.size(), 6u) << "Control char 0x" << std::hex << c << " escape wrong length";
  }
}

TEST(JsonEscape, NonAsciiPassthrough) {
  std::string input = "\xC0\xFF";  // High bytes pass through unchanged
  EXPECT_EQ(json_escape(input), input);
}

TEST(JsonEscape, MixedContent) {
  EXPECT_EQ(json_escape("Name: \"Test\"\nValue: 42"), "Name: \\\"Test\\\"\\nValue: 42");
}

// --- parse_addr_url ---

TEST(ParseAddrUrl, ValidCoverCommand) {
  uint32_t addr;
  std::string action;
  EXPECT_TRUE(parse_addr_url("/elero/api/covers/0xa831e5/command", "covers", addr, action));
  EXPECT_EQ(addr, 0xa831e5u);
  EXPECT_EQ(action, "command");
}

TEST(ParseAddrUrl, ValidLightCommand) {
  uint32_t addr;
  std::string action;
  EXPECT_TRUE(parse_addr_url("/elero/api/lights/0xc41a2b/status", "lights", addr, action));
  EXPECT_EQ(addr, 0xc41a2bu);
  EXPECT_EQ(action, "status");
}

TEST(ParseAddrUrl, NoAction) {
  uint32_t addr;
  std::string action;
  EXPECT_TRUE(parse_addr_url("/elero/api/covers/0xABCDEF", "covers", addr, action));
  EXPECT_EQ(addr, 0xABCDEFu);
  EXPECT_EQ(action, "");
}

TEST(ParseAddrUrl, MaxAddress) {
  uint32_t addr;
  std::string action;
  EXPECT_TRUE(parse_addr_url("/elero/api/covers/0xFFFFFF/cmd", "covers", addr, action));
  EXPECT_EQ(addr, 0xFFFFFFu);
}

TEST(ParseAddrUrl, ZeroAddress) {
  uint32_t addr;
  std::string action;
  EXPECT_TRUE(parse_addr_url("/elero/api/covers/0x0/cmd", "covers", addr, action));
  EXPECT_EQ(addr, 0x0u);
}

TEST(ParseAddrUrl, DecimalAddress) {
  uint32_t addr;
  std::string action;
  // strtoul with base 0 auto-detects decimal
  EXPECT_TRUE(parse_addr_url("/elero/api/covers/12345/cmd", "covers", addr, action));
  EXPECT_EQ(addr, 12345u);
}

TEST(ParseAddrUrl, WrongPrefix) {
  uint32_t addr;
  std::string action;
  EXPECT_FALSE(parse_addr_url("/wrong/api/covers/0x123/cmd", "covers", addr, action));
}

TEST(ParseAddrUrl, TooShort) {
  uint32_t addr;
  std::string action;
  EXPECT_FALSE(parse_addr_url("/elero/api/covers/", "covers", addr, action));
}

TEST(ParseAddrUrl, OverflowAddress) {
  uint32_t addr;
  std::string action;
  EXPECT_FALSE(parse_addr_url("/elero/api/covers/0x1000000/cmd", "covers", addr, action));
}

TEST(ParseAddrUrl, InvalidHex) {
  uint32_t addr;
  std::string action;
  EXPECT_FALSE(parse_addr_url("/elero/api/covers/not_a_number/cmd", "covers", addr, action));
}

TEST(ParseAddrUrl, RuntimePrefix) {
  uint32_t addr;
  std::string action;
  EXPECT_TRUE(parse_addr_url("/elero/api/runtime/0x123456/command", "runtime", addr, action));
  EXPECT_EQ(addr, 0x123456u);
  EXPECT_EQ(action, "command");
}

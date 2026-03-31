#pragma once
// Elero web server utility functions — extracted as pure functions for testability.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace esphome {
namespace elero {
namespace web_utils {

inline std::string json_escape(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (unsigned char c : s) {
    if (c == '"') {
      out += "\\\"";
    } else if (c == '\\') {
      out += "\\\\";
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else if (c < 0x20) {
      char buf[8];
      snprintf(buf, sizeof(buf), "\\u%04x", c);
      out += buf;
    } else {
      out += static_cast<char>(c);
    }
  }
  return out;
}

inline bool parse_addr_url(const std::string &url, const char *prefix,
                           uint32_t &addr_out, std::string &action_out) {
  std::string base = std::string("/elero/api/") + prefix + "/";
  if (url.size() <= base.size()) return false;
  if (url.compare(0, base.size(), base) != 0) return false;

  size_t addr_start = base.size();
  size_t slash = url.find('/', addr_start);
  std::string addr_str;
  if (slash == std::string::npos) {
    addr_str = url.substr(addr_start);
    action_out = "";
  } else {
    addr_str = url.substr(addr_start, slash - addr_start);
    action_out = url.substr(slash + 1);
  }
  char *end;
  unsigned long v = strtoul(addr_str.c_str(), &end, 0);
  if (end == addr_str.c_str()) return false;
  if (v > 0xFFFFFF) return false;
  addr_out = (uint32_t) v;
  return true;
}

}  // namespace web_utils
}  // namespace elero
}  // namespace esphome

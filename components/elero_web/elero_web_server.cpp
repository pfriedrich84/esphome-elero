#include "elero_web_server.h"
#include "elero_web_ui.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cstdio>
#include <cstdlib>
#include <climits>

namespace esphome {
namespace elero {

static const char *const TAG = "elero.web_server";

// ─── JSON helpers ─────────────────────────────────────────────────────────────

static std::string json_escape(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if      (c == '"')  { out += "\\\""; }
    else if (c == '\\') { out += "\\\\"; }
    else if (c == '\n') { out += "\\n";  }
    else if (c == '\r') { out += "\\r";  }
    else if (c == '\t') { out += "\\t";  }
    else { out += c; }
  }
  return out;
}

// ─── Formatting helpers (tiny buffers, truncation impossible) ─────────────────

static std::string fmt_hex6(uint32_t v) { char b[12]; snprintf(b, sizeof(b), "0x%06x", v); return b; }
static std::string fmt_hex2(uint8_t v) { char b[8]; snprintf(b, sizeof(b), "0x%02x", v); return b; }
static std::string fmt_int(int v) { char b[16]; snprintf(b, sizeof(b), "%d", v); return b; }
static std::string fmt_uint(unsigned v) { char b[16]; snprintf(b, sizeof(b), "%u", v); return b; }
static std::string fmt_ulong(unsigned long v) { char b[24]; snprintf(b, sizeof(b), "%lu", v); return b; }
static std::string fmt_float1(float v) { char b[24]; snprintf(b, sizeof(b), "%.1f", v); return b; }
static std::string fmt_float2(float v) { char b[24]; snprintf(b, sizeof(b), "%.2f", v); return b; }

// ─── Helpers ─────────────────────────────────────────────────────────────────

void EleroWebServer::add_cors_headers(AsyncWebServerResponse *response) {
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

void EleroWebServer::send_json_error(AsyncWebServerRequest *request, int code, const char *message) {
  char buf[128];
  std::string esc_msg = json_escape(message);
  snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", esc_msg.c_str());
  AsyncWebServerResponse *response = request->beginResponse(code, "application/json", buf);
  this->add_cors_headers(response);
  request->send(response);
}

void EleroWebServer::handle_options(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(204, "text/plain", "");
  this->add_cors_headers(response);
  response->addHeader("Access-Control-Max-Age", "86400");
  request->send(response);
}

// Parse URLs of the form /elero/api/<prefix>/0xABCDEF/<action>
// e.g. prefix = "covers" → url "/elero/api/covers/0xa831e5/command"
bool EleroWebServer::parse_addr_url(const std::string &url, const char *prefix,
                                    uint32_t &addr_out, std::string &action_out) {
  // Build expected start: /elero/api/<prefix>/
  std::string base = std::string("/elero/api/") + prefix + "/";
  if (url.size() <= base.size()) return false;
  if (url.substr(0, base.size()) != base) return false;

  // Find next slash after the address
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
  if (v > UINT32_MAX) return false;
  addr_out = (uint32_t)v;
  return true;
}

// ─── Setup / config ───────────────────────────────────────────────────────────

void EleroWebServer::setup() {
  if (this->base_ == nullptr) {
    ESP_LOGE(TAG, "web_server_base not set, cannot start Elero Web UI");
    this->mark_failed();
    return;
  }
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Elero parent not set, cannot start Elero Web UI");
    this->mark_failed();
    return;
  }

  this->base_->init();

  auto *server = this->base_->get_server();
  if (server == nullptr) {
    ESP_LOGE(TAG, "Failed to get web server instance");
    this->mark_failed();
    return;
  }

  server->addHandler(this);
  ESP_LOGI(TAG, "Elero Web UI available at /elero");
}

void EleroWebServer::dump_config() {
  ESP_LOGCONFIG(TAG, "Elero Web Server:");
  ESP_LOGCONFIG(TAG, "  URL: /elero");
  ESP_LOGCONFIG(TAG, "  API: /elero/api/*");
  ESP_LOGCONFIG(TAG, "  Enabled: %s", this->enabled_ ? "yes" : "no");
}

// ─── Routing ──────────────────────────────────────────────────────────────────

bool EleroWebServer::canHandle(AsyncWebServerRequest *request) const {
  if (!this->enabled_) return false;
  const std::string &url = request->url();
  return url == "/" || (url.size() >= 6 && url.substr(0, 6) == "/elero");
}

void EleroWebServer::handleRequest(AsyncWebServerRequest *request) {
  const std::string url = request->url();
  const auto method = request->method();

  if (method == HTTP_OPTIONS) { handle_options(request); return; }

  // ── Redirect root to /elero ──
  if (url == "/" && method == HTTP_GET) { request->redirect("/elero"); return; }

  // ── Static index ──
  if (url == "/elero" && method == HTTP_GET) { handle_index(request); return; }

  // ── Scan ──
  if (url == "/elero/api/scan/start" && method == HTTP_POST) { handle_scan_start(request); return; }
  if (url == "/elero/api/scan/stop"  && method == HTTP_POST) { handle_scan_stop(request); return; }

  // ── Discovery ──
  if (url == "/elero/api/discovered" && method == HTTP_GET) { handle_get_discovered(request); return; }

  // ── Configured covers ──
  if (url == "/elero/api/configured" && method == HTTP_GET) { handle_get_configured(request); return; }

  // ── Cover command / settings by address ──
  {
    uint32_t addr; std::string action;
    if (parse_addr_url(url, "covers", addr, action)) {
      if (action == "command" && method == HTTP_POST) { handle_cover_command(request, addr); return; }
      if (action == "settings" && method == HTTP_POST) { handle_cover_settings(request, addr); return; }
    }
  }

  // ── Adopt discovered blind ──
  {
    uint32_t addr; std::string action;
    if (parse_addr_url(url, "discovered", addr, action)) {
      if (action == "adopt" && method == HTTP_POST) { handle_adopt_discovered(request, addr); return; }
    }
  }

  // ── Runtime adopted blinds ──
  if (url == "/elero/api/runtime" && method == HTTP_GET) { handle_get_runtime(request); return; }
  {
    uint32_t addr; std::string action;
    if (parse_addr_url(url, "runtime", addr, action)) {
      if (action == "command" && method == HTTP_POST) { handle_runtime_command(request, addr); return; }
      if (action == "settings" && method == HTTP_POST) { handle_runtime_settings(request, addr); return; }
      if (action.empty() && method == HTTP_DELETE) { handle_runtime_remove(request, addr); return; }
    }
  }

  // ── YAML ──
  if (url == "/elero/api/yaml" && method == HTTP_GET) { handle_get_yaml(request); return; }

  // ── Packet dump ──
  if (url == "/elero/api/dump/start"   && method == HTTP_POST) { handle_packet_dump_start(request); return; }
  if (url == "/elero/api/dump/stop"    && method == HTTP_POST) { handle_packet_dump_stop(request); return; }
  if (url == "/elero/api/packets"      && method == HTTP_GET)  { handle_get_packets(request); return; }
  if (url == "/elero/api/packets/clear"&& method == HTTP_POST) { handle_clear_packets(request); return; }

  // ── Frequency ──
  if (url == "/elero/api/frequency"     && method == HTTP_GET)  { handle_get_frequency(request); return; }
  if (url == "/elero/api/frequency/set" && method == HTTP_POST) { handle_set_frequency(request); return; }

  // ── Logs ──
  if (url == "/elero/api/logs"                && method == HTTP_GET)  { handle_get_logs(request); return; }
  if (url == "/elero/api/logs/clear"          && method == HTTP_POST) { handle_clear_logs(request); return; }
  if (url == "/elero/api/logs/capture/start"  && method == HTTP_POST) { handle_log_capture_start(request); return; }
  if (url == "/elero/api/logs/capture/stop"   && method == HTTP_POST) { handle_log_capture_stop(request); return; }
  if (url == "/elero/api/logs/status"         && method == HTTP_GET)  { handle_get_log_status(request); return; }

  // ── Web UI enable/disable (REST mirror of HA switch) ──
  if (url == "/elero/api/ui/status" && method == HTTP_GET)  { handle_webui_status(request); return; }
  if (url == "/elero/api/ui/enable" && method == HTTP_POST) { handle_webui_enable(request); return; }

  // ── Info ──
  if (url == "/elero/api/info" && method == HTTP_GET) { handle_get_info(request); return; }

  request->send(404, "text/plain", "Not Found");
}

// ─── Index ────────────────────────────────────────────────────────────────────

void EleroWebServer::handle_index(AsyncWebServerRequest *request) {
  request->send(200, "text/html", ELERO_WEB_UI_HTML);
}

// ─── Scan ─────────────────────────────────────────────────────────────────────

void EleroWebServer::handle_scan_start(AsyncWebServerRequest *request) {
  if (this->parent_->is_scanning()) {
    this->send_json_error(request, 409, "Scan already running");
    return;
  }
  this->parent_->clear_discovered();
  this->parent_->start_scan();
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", "{\"status\":\"scanning\"}");
  this->add_cors_headers(response);
  request->send(response);
}

void EleroWebServer::handle_scan_stop(AsyncWebServerRequest *request) {
  if (!this->parent_->is_scanning()) {
    this->send_json_error(request, 409, "No scan running");
    return;
  }
  this->parent_->stop_scan();
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", "{\"status\":\"stopped\"}");
  this->add_cors_headers(response);
  request->send(response);
}

// ─── Discovered blinds ────────────────────────────────────────────────────────

void EleroWebServer::handle_get_discovered(AsyncWebServerRequest *request) {
  std::string json;
  json.reserve(64 + this->parent_->get_discovered_count() * 640);
  json = "{\"scanning\":";
  json += this->parent_->is_scanning() ? "true" : "false";
  json += ",\"blinds\":[";

  const auto &blinds = this->parent_->get_discovered_blinds();
  bool first = true;
  for (const auto &blind : blinds) {
    if (!first) json += ",";
    first = false;

    json += "{\"blind_address\":\"" + fmt_hex6(blind.blind_address) + "\","
            "\"remote_address\":\"" + fmt_hex6(blind.remote_address) + "\","
            "\"channel\":" + fmt_int(blind.channel) + ","
            "\"rssi\":" + fmt_float1(blind.rssi) + ","
            "\"last_state\":\"" + elero_state_to_string(blind.last_state) + "\","
            "\"times_seen\":" + fmt_int(blind.times_seen) + ","
            "\"hop\":\"" + fmt_hex2(blind.hop) + "\","
            "\"payload_1\":\"" + fmt_hex2(blind.payload_1) + "\","
            "\"payload_2\":\"" + fmt_hex2(blind.payload_2) + "\","
            "\"pck_inf1\":\"" + fmt_hex2(blind.pck_inf[0]) + "\","
            "\"pck_inf2\":\"" + fmt_hex2(blind.pck_inf[1]) + "\","
            "\"last_seen_ms\":" + fmt_ulong((unsigned long)blind.last_seen) + ","
            "\"params_from_command\":" + (blind.params_from_command ? "true" : "false") + ","
            "\"already_configured\":" + (this->parent_->is_cover_configured(blind.blind_address) ? "true" : "false") + ","
            "\"already_adopted\":" + (this->parent_->is_blind_adopted(blind.blind_address) ? "true" : "false") + "}";
  }

  json += "]}";
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", json.c_str());
  this->add_cors_headers(response);
  request->send(response);
}

// ─── Configured covers ────────────────────────────────────────────────────────

void EleroWebServer::handle_get_configured(AsyncWebServerRequest *request) {
  size_t total = this->parent_->get_configured_covers().size() + this->parent_->get_runtime_blinds().size();
  std::string json;
  json.reserve(64 + total * 512);
  json = "{\"covers\":[";

  bool first = true;

  // ESPHome configured covers
  for (const auto &pair : this->parent_->get_configured_covers()) {
    if (!first) json += ",";
    first = false;
    auto *blind = pair.second;
    json += "{\"blind_address\":\"" + fmt_hex6(pair.first) + "\","
            "\"name\":\"" + json_escape(blind->get_blind_name()) + "\","
            "\"position\":" + fmt_float2(blind->get_cover_position()) + ","
            "\"operation\":\"" + blind->get_operation_str() + "\","
            "\"last_state\":\"" + elero_state_to_string(blind->get_last_state_raw()) + "\","
            "\"last_seen_ms\":" + fmt_ulong((unsigned long)blind->get_last_seen_ms()) + ","
            "\"rssi\":" + fmt_float1(blind->get_last_rssi()) + ","
            "\"channel\":" + fmt_int((int)blind->get_channel()) + ","
            "\"remote_address\":\"" + fmt_hex6(blind->get_remote_address()) + "\","
            "\"poll_interval_ms\":" + fmt_ulong((unsigned long)blind->get_poll_interval_ms()) + ","
            "\"open_duration_ms\":" + fmt_ulong((unsigned long)blind->get_open_duration_ms()) + ","
            "\"close_duration_ms\":" + fmt_ulong((unsigned long)blind->get_close_duration_ms()) + ","
            "\"supports_tilt\":" + (blind->get_supports_tilt() ? "true" : "false") + ","
            "\"adopted\":false}";
  }

  // Runtime adopted blinds (mixed in as "covers" with adopted=true)
  for (const auto &entry : this->parent_->get_runtime_blinds()) {
    const auto &rb = entry.second;
    if (!first) json += ",";
    first = false;
    json += "{\"blind_address\":\"" + fmt_hex6(rb.blind_address) + "\","
            "\"name\":\"" + json_escape(rb.name) + "\","
            "\"position\":null,"
            "\"operation\":\"idle\","
            "\"last_state\":\"" + elero_state_to_string(rb.last_state) + "\","
            "\"last_seen_ms\":" + fmt_ulong((unsigned long)rb.last_seen_ms) + ","
            "\"rssi\":" + fmt_float1(rb.last_rssi) + ","
            "\"channel\":" + fmt_int((int)rb.channel) + ","
            "\"remote_address\":\"" + fmt_hex6(rb.remote_address) + "\","
            "\"poll_interval_ms\":" + fmt_ulong((unsigned long)rb.poll_intvl_ms) + ","
            "\"open_duration_ms\":" + fmt_ulong((unsigned long)rb.open_duration_ms) + ","
            "\"close_duration_ms\":" + fmt_ulong((unsigned long)rb.close_duration_ms) + ","
            "\"supports_tilt\":false,"
            "\"adopted\":true}";
  }

  json += "]}";
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", json.c_str());
  this->add_cors_headers(response);
  request->send(response);
}

// ─── Cover command ────────────────────────────────────────────────────────────

void EleroWebServer::handle_cover_command(AsyncWebServerRequest *request, uint32_t addr) {
  if (!request->hasParam("cmd")) {
    this->send_json_error(request, 400, "Missing cmd parameter");
    return;
  }
  auto cmd_param = request->getParam("cmd");
  if (cmd_param == nullptr) {
    this->send_json_error(request, 400, "Missing cmd parameter");
    return;
  }
  const std::string cmd_str = cmd_param->value().c_str();

  // Map command string to byte
  auto get_cmd_byte = [&](EleroBlindBase *blind) -> int {
    if (cmd_str == "up"    || cmd_str == "open")  return 0x20;
    if (cmd_str == "down"  || cmd_str == "close") return 0x40;
    if (cmd_str == "stop")  return 0x10;
    if (cmd_str == "check") return 0x00;
    if (cmd_str == "tilt")  return 0x24;
    if (cmd_str == "int")   return 0x44;
    return -1;
  };

  // Try configured cover
  const auto &covers = this->parent_->get_configured_covers();
  auto it = covers.find(addr);
  if (it != covers.end()) {
    int cmd_byte = get_cmd_byte(it->second);
    if (cmd_byte < 0) { this->send_json_error(request, 400, "Unknown cmd"); return; }
    it->second->enqueue_command((uint8_t)cmd_byte);
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"status\":\"queued\",\"address\":\"0x%06x\",\"cmd\":\"%s\"}", addr, cmd_str.c_str());
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
    this->add_cors_headers(response);
    request->send(response);
    return;
  }

  // Try runtime blind
  int cmd_byte = -1;
  if (cmd_str == "up"    || cmd_str == "open")  cmd_byte = 0x20;
  else if (cmd_str == "down"  || cmd_str == "close") cmd_byte = 0x40;
  else if (cmd_str == "stop")  cmd_byte = 0x10;
  else if (cmd_str == "check") cmd_byte = 0x00;
  else if (cmd_str == "tilt")  cmd_byte = 0x24;
  else if (cmd_str == "int")   cmd_byte = 0x44;
  if (cmd_byte < 0) { this->send_json_error(request, 400, "Unknown cmd"); return; }

  if (this->parent_->send_runtime_command(addr, (uint8_t)cmd_byte)) {
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"status\":\"queued\",\"address\":\"0x%06x\",\"cmd\":\"%s\"}", addr, cmd_str.c_str());
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
    this->add_cors_headers(response);
    request->send(response);
  } else {
    this->send_json_error(request, 404, "Cover not found");
  }
}

// ─── Cover settings ───────────────────────────────────────────────────────────

void EleroWebServer::handle_cover_settings(AsyncWebServerRequest *request, uint32_t addr) {
  auto parse_u32 = [](AsyncWebServerRequest *req, const char *name, uint32_t &out) -> bool {
    if (!req->hasParam(name)) return false;
    auto param = req->getParam(name);
    if (param == nullptr) return false;
    char *end;
    unsigned long v = strtoul(param->value().c_str(), &end, 10);
    if (end == param->value().c_str()) return false;
    out = (uint32_t)v;
    return true;
  };

  // Try configured cover
  const auto &covers = this->parent_->get_configured_covers();
  auto it = covers.find(addr);
  if (it != covers.end()) {
    uint32_t open_dur = it->second->get_open_duration_ms();
    uint32_t close_dur = it->second->get_close_duration_ms();
    uint32_t poll_intvl = it->second->get_poll_interval_ms();
    parse_u32(request, "open_duration", open_dur);
    parse_u32(request, "close_duration", close_dur);
    parse_u32(request, "poll_interval", poll_intvl);
    it->second->apply_runtime_settings(open_dur, close_dur, poll_intvl);
    char buf[80];
    snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"address\":\"0x%06x\"}", addr);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
    this->add_cors_headers(response);
    request->send(response);
    return;
  }

  // Try runtime blind
  uint32_t open_dur = 0, close_dur = 0, poll_intvl = 300000;
  parse_u32(request, "open_duration", open_dur);
  parse_u32(request, "close_duration", close_dur);
  parse_u32(request, "poll_interval", poll_intvl);
  if (this->parent_->update_runtime_blind_settings(addr, open_dur, close_dur, poll_intvl)) {
    char buf[80];
    snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"address\":\"0x%06x\"}", addr);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
    this->add_cors_headers(response);
    request->send(response);
  } else {
    this->send_json_error(request, 404, "Cover not found");
  }
}

// ─── Adopt discovered blind ───────────────────────────────────────────────────

void EleroWebServer::handle_adopt_discovered(AsyncWebServerRequest *request, uint32_t addr) {
  std::string name;
  if (request->hasParam("name")) {
    auto name_param = request->getParam("name");
    if (name_param != nullptr)
      name = name_param->value().c_str();
  }

  const auto &blinds = this->parent_->get_discovered_blinds();
  for (const auto &blind : blinds) {
    if (blind.blind_address == addr) {
      if (!this->parent_->adopt_blind(blind, name)) {
        this->send_json_error(request, 409, "Already configured or adopted");
        return;
      }
      std::string display_name = name.empty() ? "Adopted" : name;
      std::string esc_name = json_escape(display_name);
      char buf[128];
      snprintf(buf, sizeof(buf), "{\"status\":\"adopted\",\"address\":\"0x%06x\",\"name\":\"%s\"}",
               addr, esc_name.c_str());
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
      this->add_cors_headers(response);
      request->send(response);
      return;
    }
  }
  this->send_json_error(request, 404, "Not in discovered list");
}

// ─── Runtime blinds ───────────────────────────────────────────────────────────

void EleroWebServer::handle_get_runtime(AsyncWebServerRequest *request) {
  std::string json;
  json.reserve(64 + this->parent_->get_runtime_blinds().size() * 384);
  json = "{\"blinds\":[";
  bool first = true;
  for (const auto &entry : this->parent_->get_runtime_blinds()) {
    const auto &rb = entry.second;
    if (!first) json += ",";
    first = false;
    json += "{\"blind_address\":\"" + fmt_hex6(rb.blind_address) + "\","
            "\"name\":\"" + json_escape(rb.name) + "\","
            "\"channel\":" + fmt_int((int)rb.channel) + ","
            "\"remote_address\":\"" + fmt_hex6(rb.remote_address) + "\","
            "\"rssi\":" + fmt_float1(rb.last_rssi) + ","
            "\"last_state\":\"" + elero_state_to_string(rb.last_state) + "\","
            "\"last_seen_ms\":" + fmt_ulong((unsigned long)rb.last_seen_ms) + ","
            "\"open_duration_ms\":" + fmt_ulong((unsigned long)rb.open_duration_ms) + ","
            "\"close_duration_ms\":" + fmt_ulong((unsigned long)rb.close_duration_ms) + ","
            "\"poll_interval_ms\":" + fmt_ulong((unsigned long)rb.poll_intvl_ms) + "}";
  }
  json += "]}";
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json.c_str());
  this->add_cors_headers(response);
  request->send(response);
}

void EleroWebServer::handle_runtime_command(AsyncWebServerRequest *request, uint32_t addr) {
  // Reuse cover command logic — send_runtime_command handles it
  handle_cover_command(request, addr);
}

void EleroWebServer::handle_runtime_settings(AsyncWebServerRequest *request, uint32_t addr) {
  handle_cover_settings(request, addr);
}

void EleroWebServer::handle_runtime_remove(AsyncWebServerRequest *request, uint32_t addr) {
  if (this->parent_->remove_runtime_blind(addr)) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"status\":\"removed\",\"address\":\"0x%06x\"}", addr);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
    this->add_cors_headers(response);
    request->send(response);
  } else {
    this->send_json_error(request, 404, "Runtime blind not found");
  }
}

// ─── YAML export ──────────────────────────────────────────────────────────────

void EleroWebServer::handle_get_yaml(AsyncWebServerRequest *request) {
  const auto &blinds = this->parent_->get_discovered_blinds();
  if (blinds.empty()) {
    AsyncWebServerResponse *response = request->beginResponse(
        200, "text/plain; charset=utf-8",
        "# No discovered blinds.\n# Start a scan and press buttons on your remote.\n");
    this->add_cors_headers(response);
    request->send(response);
    return;
  }

  std::string yaml = "# Auto-generated YAML from Elero RF discovery\n";
  yaml += "# Copy this into your ESPHome configuration.\n\n";
  yaml += "cover:\n";

  int idx = 0;
  for (const auto &blind : blinds) {
    if (this->parent_->is_cover_configured(blind.blind_address))
      continue;

    if (!blind.params_from_command) {
      yaml += "  # WARNING: params derived from status packet only — press a remote\n"
              "  # button during scan so command packets can be captured for reliable values.\n";
    }
    ++idx;
    yaml += "  - platform: elero\n"
            "    blind_address: " + fmt_hex6(blind.blind_address) + "\n"
            "    channel: " + fmt_int(blind.channel) + "\n"
            "    remote_address: " + fmt_hex6(blind.remote_address) + "\n"
            "    name: \"Discovered Blind " + fmt_int(idx) + "\"\n"
            "    # open_duration: 25s\n"
            "    # close_duration: 22s\n"
            "    hop: " + fmt_hex2(blind.hop) + "\n"
            "    payload_1: " + fmt_hex2(blind.payload_1) + "\n"
            "    payload_2: " + fmt_hex2(blind.payload_2) + "\n"
            "    pck_inf1: " + fmt_hex2(blind.pck_inf[0]) + "\n"
            "    pck_inf2: " + fmt_hex2(blind.pck_inf[1]) + "\n\n";
  }

  if (idx == 0)
    yaml += "  # All discovered blinds are already configured.\n";

  AsyncWebServerResponse *response =
      request->beginResponse(200, "text/plain; charset=utf-8", yaml.c_str());
  this->add_cors_headers(response);
  request->send(response);
}

// ─── Packet dump ──────────────────────────────────────────────────────────────

void EleroWebServer::handle_packet_dump_start(AsyncWebServerRequest *request) {
  if (this->parent_->is_packet_dump_active()) {
    this->send_json_error(request, 409, "Packet dump already running");
    return;
  }
  this->parent_->clear_raw_packets();
  this->parent_->start_packet_dump();
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", "{\"status\":\"dumping\"}");
  this->add_cors_headers(response);
  request->send(response);
}

void EleroWebServer::handle_packet_dump_stop(AsyncWebServerRequest *request) {
  if (!this->parent_->is_packet_dump_active()) {
    this->send_json_error(request, 409, "No packet dump running");
    return;
  }
  this->parent_->stop_packet_dump();
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", "{\"status\":\"stopped\"}");
  this->add_cors_headers(response);
  request->send(response);
}

void EleroWebServer::handle_get_packets(AsyncWebServerRequest *request) {
  const auto &packets = this->parent_->get_raw_packets();

  std::string json;
  json.reserve(64 + packets.size() * 320);
  json = "{\"dump_active\":";
  json += this->parent_->is_packet_dump_active() ? "true" : "false";
  json += ",\"count\":";
  char cnt_buf[12];
  snprintf(cnt_buf, sizeof(cnt_buf), "%d", (int)packets.size());
  json += cnt_buf;
  json += ",\"packets\":[";

  bool first = true;
  for (const auto &pkt : packets) {
    if (!first) json += ",";
    first = false;

    std::string hex_str;
    hex_str.reserve(pkt.fifo_len * 3);
    for (int i = 0; i < pkt.fifo_len && i < CC1101_FIFO_LENGTH; i++) {
      char byte_buf[4];
      snprintf(byte_buf, sizeof(byte_buf), i == 0 ? "%02x" : " %02x", pkt.data[i]);
      hex_str += byte_buf;
    }

    json += "{\"t\":" + fmt_ulong((unsigned long)pkt.timestamp_ms) + ","
            "\"len\":" + fmt_int(pkt.fifo_len) + ","
            "\"valid\":" + (pkt.valid ? "true" : "false") + ","
            "\"reason\":\"" + json_escape(pkt.reject_reason) + "\","
            "\"hex\":\"" + hex_str + "\"}";
  }

  json += "]}";
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", json.c_str());
  this->add_cors_headers(response);
  request->send(response);
}

void EleroWebServer::handle_clear_packets(AsyncWebServerRequest *request) {
  this->parent_->clear_raw_packets();
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", "{\"status\":\"cleared\"}");
  this->add_cors_headers(response);
  request->send(response);
}

// ─── Frequency ────────────────────────────────────────────────────────────────

void EleroWebServer::handle_get_frequency(AsyncWebServerRequest *request) {
  char buf[80];
  snprintf(buf, sizeof(buf),
    "{\"freq2\":\"0x%02x\",\"freq1\":\"0x%02x\",\"freq0\":\"0x%02x\"}",
    this->parent_->get_freq2(),
    this->parent_->get_freq1(),
    this->parent_->get_freq0());
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
  this->add_cors_headers(response);
  request->send(response);
}

void EleroWebServer::handle_set_frequency(AsyncWebServerRequest *request) {
  if (!request->hasParam("freq2") || !request->hasParam("freq1") || !request->hasParam("freq0")) {
    this->send_json_error(request, 400, "Missing freq2, freq1 or freq0 parameters");
    return;
  }
  auto parse_byte = [](const char *s, uint8_t &out) -> bool {
    char *end;
    unsigned long v = strtoul(s, &end, 0);
    if (end == s || v > 0xFF) return false;
    out = (uint8_t)v;
    return true;
  };
  uint8_t f2, f1, f0;
  auto freq2_param = request->getParam("freq2");
  auto freq1_param = request->getParam("freq1");
  auto freq0_param = request->getParam("freq0");
  if (freq2_param == nullptr || freq1_param == nullptr || freq0_param == nullptr ||
      !parse_byte(freq2_param->value().c_str(), f2) ||
      !parse_byte(freq1_param->value().c_str(), f1) ||
      !parse_byte(freq0_param->value().c_str(), f0)) {
    this->send_json_error(request, 400, "Invalid frequency value (0x00-0xFF)");
    return;
  }
  this->parent_->reinit_frequency(f2, f1, f0);
  char buf[96];
  snprintf(buf, sizeof(buf),
    "{\"status\":\"ok\",\"freq2\":\"0x%02x\",\"freq1\":\"0x%02x\",\"freq0\":\"0x%02x\"}",
    f2, f1, f0);
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
  this->add_cors_headers(response);
  request->send(response);
}

// ─── Logs ─────────────────────────────────────────────────────────────────────

void EleroWebServer::handle_get_logs(AsyncWebServerRequest *request) {
  // If persistent logging is enabled, serve from the persistent event log
  if (this->parent_->is_persistent_log_enabled()) {
    auto *log = this->parent_->get_event_log();
    if (log == nullptr || !log->is_ready()) {
      this->send_json_error(request, 500, "Event log not ready");
      return;
    }

    uint32_t since_seq = 0;
    if (request->hasParam("since")) {
      auto since_param = request->getParam("since");
      if (since_param != nullptr) {
        char *endptr;
        unsigned long v = strtoul(since_param->value().c_str(), &endptr, 10);
        if (endptr == since_param->value().c_str()) {
          this->send_json_error(request, 400, "Invalid 'since' parameter (expected integer)");
          return;
        }
        since_seq = (uint32_t)v;
      }
    }

    auto entries = (since_seq > 0) ? log->read_since(since_seq) : log->read_all();

    static const char *event_type_strs[] = {"unknown", "rf_received", "state_change", "command_sent", "system"};
    static const char *op_strs[] = {"idle", "opening", "closing"};

    std::string json = "{\"persistent\":true,\"entries\":[";
    bool first = true;
    for (const auto &e : entries) {
      if (!first) json += ",";
      first = false;

      uint8_t et = (e.event_type >= 1 && e.event_type <= 4) ? e.event_type : 0;
      const char *op_str = (e.operation <= 2) ? op_strs[e.operation] : "unknown";

      json += "{\"seq\":" + fmt_ulong((unsigned long)e.sequence) + ","
              "\"t\":" + fmt_ulong((unsigned long)e.timestamp_ms) + ","
              "\"type\":\"" + event_type_strs[et] + "\","
              "\"addr\":\"" + fmt_hex6((unsigned)e.blind_address) + "\","
              "\"data1\":" + fmt_uint(e.data1) + ","
              "\"data2\":" + fmt_uint(e.data2) + ","
              "\"rssi\":" + fmt_int((int)e.rssi) + ","
              "\"op\":\"" + op_str + "\","
              "\"pos\":" + fmt_uint(e.position_x100 != 0xFFFF ? (unsigned)(e.position_x100 / 100) : 0) + ","
              "\"msg\":\"" + json_escape(e.message) + "\"}";
    }
    json += "]}";

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json.c_str());
    this->add_cors_headers(response);
    request->send(response);
    return;
  }

  // Fallback: in-memory log capture (legacy behavior)
  uint32_t since_ms = 0;
  if (request->hasParam("since")) {
    auto since_param = request->getParam("since");
    if (since_param != nullptr) {
      char *endptr;
      unsigned long v = strtoul(since_param->value().c_str(), &endptr, 10);
      if (endptr == since_param->value().c_str()) {
        this->send_json_error(request, 400, "Invalid 'since' parameter (expected integer)");
        return;
      }
      since_ms = (uint32_t)v;
    }
  }

  const auto &entries = this->parent_->get_log_entries();

  std::string json = "{\"persistent\":false,\"capture_active\":";
  json += this->parent_->is_log_capture_active() ? "true" : "false";
  json += ",\"entries\":[";

  static const char *level_strs[] = {"", "error", "warn", "info", "debug", "verbose"};

  bool first = true;
  for (const auto &e : entries) {
    if (e.timestamp_ms <= since_ms) continue;
    if (!first) json += ",";
    first = false;

    uint8_t lv = (e.level >= 1 && e.level <= 5) ? e.level : 3;

    json += "{\"t\":" + fmt_ulong((unsigned long)e.timestamp_ms) + ","
            "\"level\":" + fmt_int(lv) + ","
            "\"level_str\":\"" + level_strs[lv] + "\","
            "\"tag\":\"" + json_escape(e.tag) + "\","
            "\"msg\":\"" + json_escape(e.message) + "\"}";
  }

  json += "]}";
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json.c_str());
  this->add_cors_headers(response);
  request->send(response);
}

void EleroWebServer::handle_clear_logs(AsyncWebServerRequest *request) {
  if (this->parent_->is_persistent_log_enabled()) {
    auto *log = this->parent_->get_event_log();
    if (log && log->is_ready()) {
      log->clear();
    }
  } else {
    this->parent_->clear_log_entries();
  }
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", "{\"status\":\"cleared\"}");
  this->add_cors_headers(response);
  request->send(response);
}

void EleroWebServer::handle_log_capture_start(AsyncWebServerRequest *request) {
  // When persistent logging is enabled, capture is always-on — this is a no-op
  if (this->parent_->is_persistent_log_enabled()) {
    AsyncWebServerResponse *response =
        request->beginResponse(200, "application/json", "{\"status\":\"persistent_active\"}");
    this->add_cors_headers(response);
    request->send(response);
    return;
  }
  this->parent_->set_log_capture(true);
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", "{\"status\":\"capturing\"}");
  this->add_cors_headers(response);
  request->send(response);
}

void EleroWebServer::handle_log_capture_stop(AsyncWebServerRequest *request) {
  // When persistent logging is enabled, capture is always-on — this is a no-op
  if (this->parent_->is_persistent_log_enabled()) {
    AsyncWebServerResponse *response =
        request->beginResponse(200, "application/json", "{\"status\":\"persistent_active\"}");
    this->add_cors_headers(response);
    request->send(response);
    return;
  }
  this->parent_->set_log_capture(false);
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", "{\"status\":\"stopped\"}");
  this->add_cors_headers(response);
  request->send(response);
}

void EleroWebServer::handle_get_log_status(AsyncWebServerRequest *request) {
  bool persistent = this->parent_->is_persistent_log_enabled();
  char buf[128];
  if (persistent) {
    auto *log = this->parent_->get_event_log();
    uint16_t entries = (log && log->is_ready()) ? log->get_entry_count() : 0;
    uint16_t max_entries = (log && log->is_ready()) ? log->get_max_entries() : 0;
    uint32_t total = (log && log->is_ready()) ? log->get_total_written() : 0;
    snprintf(buf, sizeof(buf),
             "{\"persistent\":true,\"entries\":%u,\"max\":%u,\"total_written\":%lu}",
             entries, max_entries, (unsigned long)total);
  } else {
    snprintf(buf, sizeof(buf), "{\"persistent\":false}");
  }
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
  this->add_cors_headers(response);
  request->send(response);
}

// ─── Web UI enable / disable ─────────────────────────────────────────────────

void EleroWebServer::handle_webui_status(AsyncWebServerRequest *request) {
  char buf[32];
  snprintf(buf, sizeof(buf), "{\"enabled\":%s}", this->enabled_ ? "true" : "false");
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
  this->add_cors_headers(response);
  request->send(response);
}

void EleroWebServer::handle_webui_enable(AsyncWebServerRequest *request) {
  bool en = true;
  if (request->hasParam("enabled")) {
    auto enabled_param = request->getParam("enabled");
    if (enabled_param != nullptr) {
      const std::string val = enabled_param->value().c_str();
      en = (val != "false" && val != "0");
    }
  }
  this->enabled_ = en;
  char buf[32];
  snprintf(buf, sizeof(buf), "{\"enabled\":%s}", this->enabled_ ? "true" : "false");
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
  this->add_cors_headers(response);
  request->send(response);
}

// ─── Info ─────────────────────────────────────────────────────────────────────

void EleroWebServer::handle_get_info(AsyncWebServerRequest *request) {
  std::string esc_app_name = json_escape(App.get_name());
  char buf[256];
  snprintf(buf, sizeof(buf),
    "{\"device_name\":\"%s\","
    "\"uptime_ms\":%lu,"
    "\"freq2\":\"0x%02x\","
    "\"freq1\":\"0x%02x\","
    "\"freq0\":\"0x%02x\","
    "\"configured_covers\":%d}",
    esc_app_name.c_str(),
    (unsigned long)millis(),
    this->parent_->get_freq2(),
    this->parent_->get_freq1(),
    this->parent_->get_freq0(),
    (int)this->parent_->get_configured_covers().size()
  );
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
  this->add_cors_headers(response);
  request->send(response);
}

}  // namespace elero
}  // namespace esphome

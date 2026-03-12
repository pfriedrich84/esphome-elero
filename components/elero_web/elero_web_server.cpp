#include "elero_web_server.h"
#include "elero_web_ui.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace esphome {
namespace elero {

static const char *const TAG = "elero.web_server";

// ─── JSON helpers ─────────────────────────────────────────────────────────────

static std::string json_escape(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (unsigned char c : s) {
    if      (c == '"')  { out += "\\\""; }
    else if (c == '\\') { out += "\\\\"; }
    else if (c == '\n') { out += "\\n";  }
    else if (c == '\r') { out += "\\r";  }
    else if (c == '\t') { out += "\\t";  }
    else if (c < 0x20)  {
      // Escape control characters as \u00XX for valid JSON
      char buf[8];
      snprintf(buf, sizeof(buf), "\\u%04x", c);
      out += buf;
    }
    else { out += static_cast<char>(c); }
  }
  return out;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

void EleroWebServer::add_cors_headers(AsyncWebServerResponse *response) {
  response->addHeader("Connection", "close");
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
  if (url.compare(0, base.size(), base) != 0) return false;

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
  if (v > 0xFFFFFF) return false;  // Elero addresses are 3 bytes max
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
  return url == "/" || (url.size() >= 6 && url.compare(0, 6, "/elero") == 0);
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

  // ── Combined status (single poll endpoint) ──
  if (url == "/elero/api/status" && method == HTTP_GET) { handle_get_status(request); return; }

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

  // ── Light command by address (reuses cover command dispatch) ──
  {
    uint32_t addr; std::string action;
    if (parse_addr_url(url, "lights", addr, action)) {
      if (action == "command" && method == HTTP_POST) { handle_cover_command(request, addr); return; }
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
  if (url == "/elero/api/packets"         && method == HTTP_GET)  { handle_get_packets(request); return; }
  if (url == "/elero/api/packets/clear"  && method == HTTP_POST) { handle_clear_packets(request); return; }
  if (url == "/elero/api/packets/download" && method == HTTP_GET) { handle_packets_download(request); return; }

  // ── Frequency ──
  if (url == "/elero/api/frequency"         && method == HTTP_GET)  { handle_get_frequency(request); return; }
  if (url == "/elero/api/frequency/set"     && method == HTTP_POST) { handle_set_frequency(request); return; }
  if (url == "/elero/api/frequency/set_mhz" && method == HTTP_POST) { handle_set_frequency_mhz(request); return; }

  // ── Logs ──
  if (url == "/elero/api/logs"                && method == HTTP_GET)  { handle_get_logs(request); return; }
  if (url == "/elero/api/logs/clear"          && method == HTTP_POST) { handle_clear_logs(request); return; }
  if (url == "/elero/api/logs/capture/start"  && method == HTTP_POST) { handle_log_capture_start(request); return; }
  if (url == "/elero/api/logs/capture/stop"   && method == HTTP_POST) { handle_log_capture_stop(request); return; }

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

void EleroWebServer::build_discovered_array_json_(std::string &out) {
  const auto &blinds = this->parent_->get_discovered_blinds();
  bool first = true;
  for (const auto &blind : blinds) {
    if (!first) out += ",";
    first = false;

    char buf[640];
    snprintf(buf, sizeof(buf),
      "{\"blind_address\":\"0x%06x\","
      "\"remote_address\":\"0x%06x\","
      "\"channel\":%d,"
      "\"rssi\":%.1f,"
      "\"last_state\":\"%s\","
      "\"times_seen\":%d,"
      "\"hop\":\"0x%02x\","
      "\"payload_1\":\"0x%02x\","
      "\"payload_2\":\"0x%02x\","
      "\"pck_inf1\":\"0x%02x\","
      "\"pck_inf2\":\"0x%02x\","
      "\"last_seen_ms\":%lu,"
      "\"params_from_command\":%s,"
      "\"already_configured\":%s,"
      "\"configured_as\":\"%s\","
      "\"already_adopted\":%s}",
      blind.blind_address,
      blind.remote_address,
      blind.channel,
      blind.rssi,
      elero_state_to_string(blind.last_state),
      blind.times_seen,
      blind.hop,
      blind.payload_1,
      blind.payload_2,
      blind.pck_inf[0],
      blind.pck_inf[1],
      (unsigned long)blind.last_seen,
      blind.params_from_command ? "true" : "false",
      (this->parent_->is_cover_configured(blind.blind_address) ||
       this->parent_->is_light_configured(blind.blind_address)) ? "true" : "false",
      this->parent_->is_cover_configured(blind.blind_address) ? "cover" :
        (this->parent_->is_light_configured(blind.blind_address) ? "light" : "none"),
      this->parent_->is_blind_adopted(blind.blind_address) ? "true" : "false"
    );
    out += buf;
  }
}

void EleroWebServer::handle_get_discovered(AsyncWebServerRequest *request) {
  std::string json;
  json.reserve(512);
  json += "{\"scanning\":";
  json += this->parent_->is_scanning() ? "true" : "false";
  json += ",\"blinds\":[";
  this->build_discovered_array_json_(json);
  json += "]}";
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", json.c_str());
  this->add_cors_headers(response);
  request->send(response);
}

// ─── Configured covers ────────────────────────────────────────────────────────

void EleroWebServer::build_configured_json_(std::string &out) {
  out += "\"covers\":[";

  bool first = true;

  // ESPHome configured covers
  for (const auto &pair : this->parent_->get_configured_covers()) {
    if (!first) out += ",";
    first = false;
    auto *blind = pair.second;
    std::string esc_name = json_escape(blind->get_blind_name());
    char buf[512];
    snprintf(buf, sizeof(buf),
      "{\"blind_address\":\"0x%06x\","
      "\"name\":\"%s\","
      "\"position\":%.2f,"
      "\"operation\":\"%s\","
      "\"last_state\":\"%s\","
      "\"last_seen_ms\":%lu,"
      "\"rssi\":%.1f,"
      "\"channel\":%d,"
      "\"remote_address\":\"0x%06x\","
      "\"poll_interval_ms\":%lu,"
      "\"open_duration_ms\":%lu,"
      "\"close_duration_ms\":%lu,"
      "\"supports_tilt\":%s,"
      "\"device_type\":\"cover\","
      "\"adopted\":false}",
      pair.first,
      esc_name.c_str(),
      blind->get_cover_position(),
      blind->get_operation_str(),
      elero_state_to_string(blind->get_last_state_raw()),
      (unsigned long)blind->get_last_seen_ms(),
      blind->get_last_rssi(),
      (int)blind->get_channel(),
      blind->get_remote_address(),
      (unsigned long)blind->get_poll_interval_ms(),
      (unsigned long)blind->get_open_duration_ms(),
      (unsigned long)blind->get_close_duration_ms(),
      blind->get_supports_tilt() ? "true" : "false"
    );
    out += buf;
  }

  // Runtime adopted covers (device_type == COVER)
  for (const auto &entry : this->parent_->get_runtime_blinds()) {
    const auto &rb = entry.second;
    if (rb.device_type != DeviceType::COVER) continue;
    if (!first) out += ",";
    first = false;
    std::string esc_name = json_escape(rb.name);

    // Determine operation string from moving_direction
    const char *operation = "idle";
    if (rb.moving_direction > 0) operation = "opening";
    else if (rb.moving_direction < 0) operation = "closing";

    // Format position: null if unknown (-1.0), otherwise 0.00-1.00
    char pos_str[16];
    if (rb.position < 0.0f) {
      snprintf(pos_str, sizeof(pos_str), "null");
    } else {
      snprintf(pos_str, sizeof(pos_str), "%.2f", rb.position);
    }

    char buf[512];
    snprintf(buf, sizeof(buf),
      "{\"blind_address\":\"0x%06x\","
      "\"name\":\"%s\","
      "\"position\":%s,"
      "\"operation\":\"%s\","
      "\"last_state\":\"%s\","
      "\"last_seen_ms\":%lu,"
      "\"rssi\":%.1f,"
      "\"channel\":%d,"
      "\"remote_address\":\"0x%06x\","
      "\"poll_interval_ms\":%lu,"
      "\"open_duration_ms\":%lu,"
      "\"close_duration_ms\":%lu,"
      "\"supports_tilt\":false,"
      "\"device_type\":\"cover\","
      "\"adopted\":true}",
      rb.blind_address,
      esc_name.c_str(),
      pos_str,
      operation,
      elero_state_to_string(rb.last_state),
      (unsigned long)rb.last_seen_ms,
      rb.last_rssi,
      (int)rb.channel,
      rb.remote_address,
      (unsigned long)rb.poll_intvl_ms,
      (unsigned long)rb.open_duration_ms,
      (unsigned long)rb.close_duration_ms
    );
    out += buf;
  }

  out += "],\"lights\":[";

  first = true;

  // ESPHome configured lights
  for (const auto &pair : this->parent_->get_configured_lights()) {
    if (!first) out += ",";
    first = false;
    auto *light = pair.second;
    std::string esc_name = json_escape(light->get_light_name());
    char buf[512];
    snprintf(buf, sizeof(buf),
      "{\"blind_address\":\"0x%06x\","
      "\"name\":\"%s\","
      "\"is_on\":%s,"
      "\"brightness\":%.2f,"
      "\"operation\":\"%s\","
      "\"last_state\":\"%s\","
      "\"last_seen_ms\":%lu,"
      "\"rssi\":%.1f,"
      "\"channel\":%d,"
      "\"remote_address\":\"0x%06x\","
      "\"dim_duration_ms\":%lu,"
      "\"device_type\":\"light\","
      "\"adopted\":false}",
      pair.first,
      esc_name.c_str(),
      light->get_is_on() ? "true" : "false",
      light->get_brightness(),
      light->get_operation_str(),
      elero_state_to_string(light->get_last_state_raw()),
      (unsigned long)light->get_last_seen_ms(),
      light->get_last_rssi(),
      (int)light->get_channel(),
      light->get_remote_address(),
      (unsigned long)light->get_dim_duration_ms()
    );
    out += buf;
  }

  // Runtime adopted lights (device_type == LIGHT)
  for (const auto &entry : this->parent_->get_runtime_blinds()) {
    const auto &rb = entry.second;
    if (rb.device_type != DeviceType::LIGHT) continue;
    if (!first) out += ",";
    first = false;
    std::string esc_name = json_escape(rb.name);
    char buf[512];
    snprintf(buf, sizeof(buf),
      "{\"blind_address\":\"0x%06x\","
      "\"name\":\"%s\","
      "\"is_on\":false,"
      "\"brightness\":0.00,"
      "\"operation\":\"idle\","
      "\"last_state\":\"%s\","
      "\"last_seen_ms\":%lu,"
      "\"rssi\":%.1f,"
      "\"channel\":%d,"
      "\"remote_address\":\"0x%06x\","
      "\"dim_duration_ms\":%lu,"
      "\"device_type\":\"light\","
      "\"adopted\":true}",
      rb.blind_address,
      esc_name.c_str(),
      elero_state_to_string(rb.last_state),
      (unsigned long)rb.last_seen_ms,
      rb.last_rssi,
      (int)rb.channel,
      rb.remote_address,
      (unsigned long)rb.dim_duration_ms
    );
    out += buf;
  }

  out += "]";
}

void EleroWebServer::handle_get_configured(AsyncWebServerRequest *request) {
  std::string json;
  json.reserve(64 + this->parent_->get_configured_covers().size() * 400
                   + this->parent_->get_configured_lights().size() * 400
                   + this->parent_->get_runtime_blinds().size() * 400);
  json += "{";
  this->build_configured_json_(json);
  json += "}";
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

  // Map command string to byte — use configured values for covers, defaults for runtime blinds
  auto get_cmd_byte_for_cover = [&](EleroBlindBase *blind) -> int {
    if (cmd_str == "up"    || cmd_str == "open")  return blind->get_command_up();
    if (cmd_str == "down"  || cmd_str == "close") return blind->get_command_down();
    if (cmd_str == "stop")  return blind->get_command_stop();
    if (cmd_str == "check") return blind->get_command_check();
    if (cmd_str == "tilt")  return blind->get_command_tilt();
    if (cmd_str == "int")   return 0x44;
    return -1;
  };

  // Try configured cover
  const auto &covers = this->parent_->get_configured_covers();
  auto it = covers.find(addr);
  if (it != covers.end()) {
    int cmd_byte = get_cmd_byte_for_cover(it->second);
    if (cmd_byte < 0) { this->send_json_error(request, 400, "Unknown cmd"); return; }
    it->second->enqueue_command((uint8_t)cmd_byte);
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"status\":\"queued\",\"address\":\"0x%06x\",\"cmd\":\"%s\"}", addr, cmd_str.c_str());
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
    this->add_cors_headers(response);
    request->send(response);
    return;
  }

  // Try configured light
  {
    const auto &lights = this->parent_->get_configured_lights();
    auto lit = lights.find(addr);
    if (lit != lights.end()) {
      int cmd_byte = -1;
      if (cmd_str == "on"  || cmd_str == "up"   || cmd_str == "open")  cmd_byte = lit->second->get_command_on();
      if (cmd_str == "off" || cmd_str == "down"  || cmd_str == "close") cmd_byte = lit->second->get_command_off();
      if (cmd_str == "stop")  cmd_byte = lit->second->get_command_stop();
      if (cmd_str == "check") cmd_byte = lit->second->get_command_check();
      if (cmd_byte < 0) { this->send_json_error(request, 400, "Unknown cmd"); return; }
      lit->second->enqueue_command((uint8_t)cmd_byte);
      char buf[96];
      snprintf(buf, sizeof(buf), "{\"status\":\"queued\",\"address\":\"0x%06x\",\"cmd\":\"%s\"}", addr, cmd_str.c_str());
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
      this->add_cors_headers(response);
      request->send(response);
      return;
    }
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
  static const uint32_t MAX_DURATION_MS = 300000;  // 5 minutes max for open/close
  static const uint32_t MIN_POLL_INTERVAL_MS = 1000;  // 1 second minimum poll interval

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

    // Validate duration values
    if (open_dur > MAX_DURATION_MS) {
      this->send_json_error(request, 400, "open_duration exceeds maximum (300000 ms)");
      return;
    }
    if (close_dur > MAX_DURATION_MS) {
      this->send_json_error(request, 400, "close_duration exceeds maximum (300000 ms)");
      return;
    }
    // Both-or-neither: if one duration is set, the other must be too
    if ((open_dur > 0) != (close_dur > 0)) {
      this->send_json_error(request, 400, "open_duration and close_duration must both be set or both be zero");
      return;
    }
    // Validate poll interval: must be >= 1000ms or 0 (disabled)
    if (poll_intvl != 0 && poll_intvl < MIN_POLL_INTERVAL_MS) {
      this->send_json_error(request, 400, "poll_interval must be >= 1000 ms or 0 (disabled)");
      return;
    }

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

  // Validate duration values
  if (open_dur > MAX_DURATION_MS) {
    this->send_json_error(request, 400, "open_duration exceeds maximum (300000 ms)");
    return;
  }
  if (close_dur > MAX_DURATION_MS) {
    this->send_json_error(request, 400, "close_duration exceeds maximum (300000 ms)");
    return;
  }
  // Both-or-neither: if one duration is set, the other must be too
  if ((open_dur > 0) != (close_dur > 0)) {
    this->send_json_error(request, 400, "open_duration and close_duration must both be set or both be zero");
    return;
  }
  // Validate poll interval: must be >= 1000ms or 0 (disabled)
  if (poll_intvl != 0 && poll_intvl < MIN_POLL_INTERVAL_MS) {
    this->send_json_error(request, 400, "poll_interval must be >= 1000 ms or 0 (disabled)");
    return;
  }

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

  DeviceType dtype = DeviceType::COVER;
  if (request->hasParam("type")) {
    auto type_param = request->getParam("type");
    if (type_param != nullptr) {
      std::string type_str = type_param->value().c_str();
      if (type_str == "light") dtype = DeviceType::LIGHT;
    }
  }

  const auto &blinds = this->parent_->get_discovered_blinds();
  for (const auto &blind : blinds) {
    if (blind.blind_address == addr) {
      if (!this->parent_->adopt_blind(blind, name, dtype)) {
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
  std::string json = "{\"blinds\":[";
  bool first = true;
  for (const auto &entry : this->parent_->get_runtime_blinds()) {
    const auto &rb = entry.second;
    if (!first) json += ",";
    first = false;
    std::string esc_name = json_escape(rb.name);
    // Format position: null if unknown (-1.0), otherwise 0.00-1.00
    char pos_str[16];
    if (rb.position < 0.0f) {
      snprintf(pos_str, sizeof(pos_str), "null");
    } else {
      snprintf(pos_str, sizeof(pos_str), "%.2f", rb.position);
    }

    char buf[448];
    snprintf(buf, sizeof(buf),
      "{\"blind_address\":\"0x%06x\","
      "\"name\":\"%s\","
      "\"channel\":%d,"
      "\"remote_address\":\"0x%06x\","
      "\"rssi\":%.1f,"
      "\"last_state\":\"%s\","
      "\"last_seen_ms\":%lu,"
      "\"device_type\":\"%s\","
      "\"position\":%s,"
      "\"moving_direction\":%d,"
      "\"open_duration_ms\":%lu,"
      "\"close_duration_ms\":%lu,"
      "\"dim_duration_ms\":%lu,"
      "\"poll_interval_ms\":%lu}",
      rb.blind_address, esc_name.c_str(), (int)rb.channel,
      rb.remote_address, rb.last_rssi,
      elero_state_to_string(rb.last_state),
      (unsigned long)rb.last_seen_ms,
      rb.device_type == DeviceType::LIGHT ? "light" : "cover",
      pos_str,
      (int)rb.moving_direction,
      (unsigned long)rb.open_duration_ms,
      (unsigned long)rb.close_duration_ms,
      (unsigned long)rb.dim_duration_ms,
      (unsigned long)rb.poll_intvl_ms
    );
    json += buf;
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

  int cover_idx = 0;
  for (const auto &blind : blinds) {
    if (this->parent_->is_cover_configured(blind.blind_address))
      continue;
    if (this->parent_->is_light_configured(blind.blind_address))
      continue;
    // Skip devices that reported ON/OFF state (likely lights)
    if (blind.last_state == ELERO_STATE_ON || blind.last_state == ELERO_STATE_OFF)
      continue;

    char buf[640];
    snprintf(buf, sizeof(buf),
      "%s"
      "  - platform: elero\n"
      "    blind_address: 0x%06x\n"
      "    channel: %d\n"
      "    remote_address: 0x%06x\n"
      "    name: \"Discovered Blind %d\"\n"
      "    # open_duration: 25s\n"
      "    # close_duration: 22s\n"
      "    hop: 0x%02x\n"
      "    payload_1: 0x%02x\n"
      "    payload_2: 0x%02x\n"
      "    pck_inf1: 0x%02x\n"
      "    pck_inf2: 0x%02x\n"
      "\n",
      blind.params_from_command
        ? ""
        : "  # WARNING: params derived from status packet only — press a remote\n"
          "  # button during scan so command packets can be captured for reliable values.\n",
      blind.blind_address, blind.channel, blind.remote_address, ++cover_idx,
      blind.hop, blind.payload_1, blind.payload_2, blind.pck_inf[0], blind.pck_inf[1]
    );
    yaml += buf;
  }

  if (cover_idx == 0)
    yaml += "  # All discovered blinds are already configured.\n";

  // Light section — devices that reported ON/OFF state
  int light_idx = 0;
  for (const auto &blind : blinds) {
    if (this->parent_->is_cover_configured(blind.blind_address))
      continue;
    if (this->parent_->is_light_configured(blind.blind_address))
      continue;
    if (blind.last_state != ELERO_STATE_ON && blind.last_state != ELERO_STATE_OFF)
      continue;
    if (light_idx == 0) {
      yaml += "\n# Potential lights (reported ON/OFF state during scan)\n";
      yaml += "light:\n";
    }
    char buf[640];
    snprintf(buf, sizeof(buf),
      "%s"
      "  - platform: elero\n"
      "    blind_address: 0x%06x\n"
      "    channel: %d\n"
      "    remote_address: 0x%06x\n"
      "    name: \"Discovered Light %d\"\n"
      "    # dim_duration: 0s\n"
      "    hop: 0x%02x\n"
      "    payload_1: 0x%02x\n"
      "    payload_2: 0x%02x\n"
      "    pck_inf1: 0x%02x\n"
      "    pck_inf2: 0x%02x\n"
      "\n",
      blind.params_from_command
        ? ""
        : "  # WARNING: params derived from status packet only — press a remote\n"
          "  # button during scan so command packets can be captured for reliable values.\n",
      blind.blind_address, blind.channel, blind.remote_address, ++light_idx,
      blind.hop, blind.payload_1, blind.payload_2, blind.pck_inf[0], blind.pck_inf[1]
    );
    yaml += buf;
  }

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

void EleroWebServer::build_packets_array_json_(std::string &out) {
  const auto &packets = this->parent_->get_raw_packets();
  bool first = true;
  for (const auto &pkt : packets) {
    if (!first) out += ",";
    first = false;

    char hex_buf[CC1101_FIFO_LENGTH * 3 + 1];
    int hex_pos = 0;
    for (int i = 0; i < pkt.fifo_len && i < CC1101_FIFO_LENGTH; i++) {
      int remaining = (int)sizeof(hex_buf) - hex_pos;
      if (remaining <= 0) break;
      hex_pos += snprintf(hex_buf + hex_pos, remaining, i == 0 ? "%02x" : " %02x", pkt.data[i]);
    }
    hex_buf[sizeof(hex_buf) - 1] = '\0';

    char entry_buf[320];
    snprintf(entry_buf, sizeof(entry_buf),
      "{\"t\":%lu,\"len\":%d,\"valid\":%s,\"reason\":\"%s\",\"hex\":\"%s\"}",
      (unsigned long)pkt.timestamp_ms,
      pkt.fifo_len,
      pkt.valid ? "true" : "false",
      pkt.reject_reason,
      hex_buf
    );
    out += entry_buf;
  }
}

void EleroWebServer::handle_get_packets(AsyncWebServerRequest *request) {
  const auto &packets = this->parent_->get_raw_packets();

  std::string json = "{\"dump_active\":";
  json += this->parent_->is_packet_dump_active() ? "true" : "false";
  json += ",\"count\":";
  char cnt_buf[12];
  snprintf(cnt_buf, sizeof(cnt_buf), "%d", (int)packets.size());
  json += cnt_buf;
  json += ",\"packets\":[";
  this->build_packets_array_json_(json);
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

// ─── Packet dump download ─────────────────────────────────────────────────────

void EleroWebServer::handle_packets_download(AsyncWebServerRequest *request) {
  const auto &packets = this->parent_->get_raw_packets();

  std::string json;
  json.reserve(256 + packets.size() * 320);

  std::string esc_name = json_escape(App.get_name());
  char meta[256];
  snprintf(meta, sizeof(meta),
    "{\"device_name\":\"%s\","
    "\"uptime_ms\":%lu,"
    "\"freq2\":\"0x%02x\","
    "\"freq1\":\"0x%02x\","
    "\"freq0\":\"0x%02x\","
    "\"dump_active\":%s,"
    "\"exported_at_ms\":%lu,"
    "\"count\":%d,"
    "\"packets\":[",
    esc_name.c_str(),
    (unsigned long)millis(),
    this->parent_->get_freq2(),
    this->parent_->get_freq1(),
    this->parent_->get_freq0(),
    this->parent_->is_packet_dump_active() ? "true" : "false",
    (unsigned long)millis(),
    (int)packets.size());
  json += meta;

  bool first = true;
  for (const auto &pkt : packets) {
    if (!first) json += ",";
    first = false;

    char hex_buf[CC1101_FIFO_LENGTH * 3 + 1];
    int hex_pos = 0;
    for (int i = 0; i < pkt.fifo_len && i < CC1101_FIFO_LENGTH; i++) {
      int remaining = (int)sizeof(hex_buf) - hex_pos;
      if (remaining <= 0) break;
      hex_pos += snprintf(hex_buf + hex_pos, remaining, i == 0 ? "%02x" : " %02x", pkt.data[i]);
    }
    hex_buf[sizeof(hex_buf) - 1] = '\0';

    std::string reason_esc = json_escape(pkt.reject_reason);
    char entry_buf[384];
    snprintf(entry_buf, sizeof(entry_buf),
      "{\"t\":%lu,\"len\":%d,\"valid\":%s,\"reason\":\"%s\",\"hex\":\"%s\"}",
      (unsigned long)pkt.timestamp_ms,
      pkt.fifo_len,
      pkt.valid ? "true" : "false",
      reason_esc.c_str(),
      hex_buf);
    json += entry_buf;
  }
  json += "]}";

  char disp[64];
  snprintf(disp, sizeof(disp), "attachment; filename=\"elero_packets_%lu.json\"",
           (unsigned long)millis());

  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", json.c_str());
  response->addHeader("Content-Disposition", disp);
  this->add_cors_headers(response);
  request->send(response);
}

// ─── Frequency ────────────────────────────────────────────────────────────────

void EleroWebServer::handle_get_frequency(AsyncWebServerRequest *request) {
  float mhz = Elero::registers_to_mhz(this->parent_->get_freq2(),
                                        this->parent_->get_freq1(),
                                        this->parent_->get_freq0());
  char buf[120];
  snprintf(buf, sizeof(buf),
    "{\"freq2\":\"0x%02x\",\"freq1\":\"0x%02x\",\"freq0\":\"0x%02x\",\"mhz\":%.2f}",
    this->parent_->get_freq2(),
    this->parent_->get_freq1(),
    this->parent_->get_freq0(),
    mhz);
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
  float mhz = Elero::registers_to_mhz(f2, f1, f0);
  char buf[128];
  snprintf(buf, sizeof(buf),
    "{\"status\":\"ok\",\"freq2\":\"0x%02x\",\"freq1\":\"0x%02x\",\"freq0\":\"0x%02x\",\"mhz\":%.2f}",
    f2, f1, f0, mhz);
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
  this->add_cors_headers(response);
  request->send(response);
}

void EleroWebServer::handle_set_frequency_mhz(AsyncWebServerRequest *request) {
  if (!request->hasParam("mhz")) {
    this->send_json_error(request, 400, "Missing mhz parameter");
    return;
  }
  auto mhz_param = request->getParam("mhz");
  if (mhz_param == nullptr) {
    this->send_json_error(request, 400, "Missing mhz parameter");
    return;
  }
  char *end;
  float mhz = strtof(mhz_param->value().c_str(), &end);
  if (end == mhz_param->value().c_str() || mhz < 300.0f || mhz > 928.0f) {
    this->send_json_error(request, 400, "Invalid MHz value (300.0-928.0)");
    return;
  }
  if (!this->parent_->reinit_frequency_mhz(mhz)) {
    this->send_json_error(request, 400, "setFrequency failed — value may be outside CC1101 supported bands");
    return;
  }
  float actual_mhz = Elero::registers_to_mhz(this->parent_->get_freq2(),
                                               this->parent_->get_freq1(),
                                               this->parent_->get_freq0());
  char buf[160];
  snprintf(buf, sizeof(buf),
    "{\"status\":\"ok\",\"mhz\":%.2f,\"freq2\":\"0x%02x\",\"freq1\":\"0x%02x\",\"freq0\":\"0x%02x\"}",
    actual_mhz,
    this->parent_->get_freq2(),
    this->parent_->get_freq1(),
    this->parent_->get_freq0());
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
  this->add_cors_headers(response);
  request->send(response);
}

// ─── Logs ─────────────────────────────────────────────────────────────────────

void EleroWebServer::build_log_entries_array_json_(std::string &out, uint32_t since_ms) {
  const auto entries = this->parent_->get_log_entries_copy();
  static const char *level_strs[] = {"", "error", "warn", "info", "debug", "verbose"};

  bool first = true;
  for (const auto &e : entries) {
    if (e.timestamp_ms <= since_ms) continue;
    if (!first) out += ",";
    first = false;

    uint8_t lv = (e.level >= 1 && e.level <= 5) ? e.level : 3;

    std::string msg_esc = json_escape(e.message);
    char buf[512];
    snprintf(buf, sizeof(buf),
      "{\"t\":%lu,\"level\":%d,\"level_str\":\"%s\",\"tag\":\"%s\",\"msg\":\"%s\"}",
      (unsigned long)e.timestamp_ms, lv, level_strs[lv], e.tag, msg_esc.c_str());
    out += buf;
  }
}

void EleroWebServer::handle_get_logs(AsyncWebServerRequest *request) {
  uint32_t since_ms = 0;
  if (request->hasParam("since")) {
    auto since_param = request->getParam("since");
    if (since_param != nullptr)
      since_ms = (uint32_t)strtoul(since_param->value().c_str(), nullptr, 10);
  }

  std::string json = "{\"capture_active\":";
  json += this->parent_->is_log_capture_active() ? "true" : "false";
  json += ",\"entries\":[";
  this->build_log_entries_array_json_(json, since_ms);
  json += "]}";
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json.c_str());
  this->add_cors_headers(response);
  request->send(response);
}

void EleroWebServer::handle_clear_logs(AsyncWebServerRequest *request) {
  this->parent_->clear_log_entries();
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", "{\"status\":\"cleared\"}");
  this->add_cors_headers(response);
  request->send(response);
}

void EleroWebServer::handle_log_capture_start(AsyncWebServerRequest *request) {
  this->parent_->set_log_capture(true);
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", "{\"status\":\"capturing\"}");
  this->add_cors_headers(response);
  request->send(response);
}

void EleroWebServer::handle_log_capture_stop(AsyncWebServerRequest *request) {
  this->parent_->set_log_capture(false);
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", "{\"status\":\"stopped\"}");
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
  char buf[320];
  snprintf(buf, sizeof(buf),
    "{\"device_name\":\"%s\","
    "\"uptime_ms\":%lu,"
    "\"freq2\":\"0x%02x\","
    "\"freq1\":\"0x%02x\","
    "\"freq0\":\"0x%02x\","
    "\"configured_covers\":%d,"
    "\"configured_lights\":%d}",
    esc_app_name.c_str(),
    (unsigned long)millis(),
    this->parent_->get_freq2(),
    this->parent_->get_freq1(),
    this->parent_->get_freq0(),
    (int)this->parent_->get_configured_covers().size(),
    (int)this->parent_->get_configured_lights().size()
  );
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", buf);
  this->add_cors_headers(response);
  request->send(response);
}

// ─── Combined status (reduces poll requests to one per cycle) ─────────────────

void EleroWebServer::handle_get_status(AsyncWebServerRequest *request) {
  std::string tab = "devices";
  if (request->hasParam("tab")) {
    auto p = request->getParam("tab");
    if (p) tab = p->value().c_str();
  }

  std::string json;
  json.reserve(1024);
  json += "{";
  this->build_configured_json_(json);  // always: "covers":[...],"lights":[...]

  // Always include diagnostic counters for radio health monitoring
  char diag_buf[128];
  snprintf(diag_buf, sizeof(diag_buf),
    ",\"diagnostics\":{\"rx_count\":%lu,\"tx_count\":%lu,\"watchdog_recovery_count\":%lu}",
    (unsigned long)this->parent_->get_rx_count(),
    (unsigned long)this->parent_->get_tx_count(),
    (unsigned long)this->parent_->get_watchdog_recovery_count());
  json += diag_buf;

  if (tab == "discovery") {
    json += ",\"scanning\":";
    json += this->parent_->is_scanning() ? "true" : "false";
    json += ",\"discovered\":[";
    this->build_discovered_array_json_(json);
    json += "]";
  } else if (tab == "log") {
    uint32_t since_ms = 0;
    if (request->hasParam("since")) {
      auto p = request->getParam("since");
      if (p) since_ms = (uint32_t)strtoul(p->value().c_str(), nullptr, 10);
    }
    json += ",\"capture_active\":";
    json += this->parent_->is_log_capture_active() ? "true" : "false";
    json += ",\"log_entries\":[";
    this->build_log_entries_array_json_(json, since_ms);
    json += "]";
  } else if (tab == "config") {
    json += ",\"dump_active\":";
    json += this->parent_->is_packet_dump_active() ? "true" : "false";
    json += ",\"packets\":[";
    this->build_packets_array_json_(json);
    json += "]";
  }

  json += "}";
  auto *response = request->beginResponse(200, "application/json", json.c_str());
  this->add_cors_headers(response);
  request->send(response);
}

}  // namespace elero
}  // namespace esphome

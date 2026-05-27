#include "api_adapter_glink.h"

#ifdef ESP32

#include "esphome/components/network/util.h"
#include "esphome/components/storage/store.h"
#include "esphome/components/storage/util.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <esp_system.h>
#include <mbedtls/md.h>

namespace esphome {
namespace api_adapter_glink {

static const char *const TAG = "api_adapter_glink";

namespace {

const char *radiation_mode_to_api(storage::RadiationMode mode) {
  switch (mode) {
    case storage::RadiationMode::MIN:
      return "min";
    case storage::RadiationMode::STD:
      return "std";
    case storage::RadiationMode::MAX:
      return "max";
    case storage::RadiationMode::ON:
      return "on";
    case storage::RadiationMode::OFF:
    default:
      return "off";
  }
}

bool json_object_or_empty(JsonObject source, JsonDocument *empty_doc, JsonObject *out) {
  if (source["body"].is<JsonObject>()) {
    *out = source["body"].as<JsonObject>();
    return true;
  }
  *out = empty_doc->to<JsonObject>();
  return false;
}

std::string json_error(JsonObject response, const char *fallback) {
  const char *error = response["error"] | "";
  if (error[0] != 0)
    return error;
  return fallback;
}

}  // namespace

void ApiAdapterGLink::setup() {
  if (this->core_ == nullptr || storage::store == nullptr) {
    ESP_LOGE(TAG, "API core or storage is not available");
    this->mark_failed();
    return;
  }
  if (this->url_.empty() || this->key_id_.empty() || this->secret_.empty()) {
    ESP_LOGE(TAG, "G-Link url, key_id and secret are required");
    this->mark_failed();
    return;
  }

  if (!this->parse_url_(&this->parsed_)) {
    ESP_LOGE(TAG, "Invalid G-Link url: %s", this->url_.c_str());
    this->mark_failed();
    return;
  }
  this->parsed_url_ = true;
  this->next_connect_ms_ = millis() + 15000;

  storage::store->add_on_radiation_applied(
      [this](storage::RadiationMode mode, storage::RadiationSource source) { this->send_radiation_event_(mode, source); });
  ESP_LOGI(TAG, "G-Link adapter enabled; first connect is delayed until WiFi is stable");
}

void ApiAdapterGLink::loop() {
  if (!this->parsed_url_)
    return;

  const uint32_t now = millis();
  if (!network::is_connected()) {
    if (this->started_)
      this->stop_("network disconnected");
    return;
  }

  if (!this->started_) {
    if (static_cast<int32_t>(now - this->next_connect_ms_) < 0)
      return;
    if (!this->probe_gateway_()) {
      this->next_connect_ms_ = now + this->reconnect_interval_ms_;
      return;
    }
    this->connect_();
    return;
  }

  this->websocket_.loop();

  if (!this->connected_ && this->connect_started_ms_ != 0 && now - this->connect_started_ms_ > 8000) {
    this->stop_("connect timeout");
    return;
  }

  if (this->authenticated_ && this->heartbeat_interval_ms_ > 0 &&
      now - this->last_heartbeat_ms_ >= this->heartbeat_interval_ms_) {
    this->send_heartbeat_();
    this->last_heartbeat_ms_ = now;
  }
}

void ApiAdapterGLink::dump_config() {
  ESP_LOGCONFIG(TAG, "G-Link API adapter:");
  ESP_LOGCONFIG(TAG, "  URL: %s", this->url_.c_str());
  ESP_LOGCONFIG(TAG, "  Key ID: %s", this->key_id_.c_str());
  ESP_LOGCONFIG(TAG, "  Heartbeat: %ums", this->heartbeat_interval_ms_);
}

void ApiAdapterGLink::connect_() {
  this->websocket_.onEvent([this](WStype_t type, uint8_t *payload, size_t length) {
    this->on_websocket_event_(type, payload, length);
  });
  this->websocket_.setReconnectInterval(this->reconnect_interval_ms_);

  ESP_LOGI(TAG, "Connecting to G-Link %s%s:%u%s", this->parsed_.secure ? "wss://" : "ws://", this->parsed_.host.c_str(),
           this->parsed_.port, this->parsed_.path.c_str());
  if (this->parsed_.secure) {
    this->websocket_.beginSSL(this->parsed_.host.c_str(), this->parsed_.port, this->parsed_.path.c_str());
  } else {
    this->websocket_.begin(this->parsed_.host.c_str(), this->parsed_.port, this->parsed_.path.c_str());
  }
  this->started_ = true;
  this->connect_started_ms_ = millis();
}

void ApiAdapterGLink::stop_(const char *reason) {
  ESP_LOGW(TAG, "G-Link stopped: %s", reason);
  this->websocket_.disconnect();
  this->started_ = false;
  this->connected_ = false;
  this->authenticated_ = false;
  this->connect_started_ms_ = 0;
  this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
}

bool ApiAdapterGLink::probe_gateway_() {
  WiFiClient probe;
  probe.setConnectionTimeout(750);
  const bool ok = probe.connect(this->parsed_.host.c_str(), this->parsed_.port, 750);
  probe.stop();
  if (!ok)
    ESP_LOGW(TAG, "G-Link gateway unavailable, retrying later");
  return ok;
}

bool ApiAdapterGLink::parse_url_(ParsedUrl *parsed) const {
  std::string rest;
  if (this->url_.rfind("ws://", 0) == 0) {
    parsed->secure = false;
    parsed->port = 80;
    rest = this->url_.substr(5);
  } else if (this->url_.rfind("wss://", 0) == 0) {
    parsed->secure = true;
    parsed->port = 443;
    rest = this->url_.substr(6);
  } else {
    return false;
  }

  const size_t slash = rest.find('/');
  std::string host_port = slash == std::string::npos ? rest : rest.substr(0, slash);
  parsed->path = slash == std::string::npos ? "/" : rest.substr(slash);
  const size_t colon = host_port.rfind(':');
  if (colon != std::string::npos) {
    parsed->host = host_port.substr(0, colon);
    parsed->port = static_cast<uint16_t>(std::atoi(host_port.substr(colon + 1).c_str()));
  } else {
    parsed->host = host_port;
  }

  return !parsed->host.empty() && parsed->port != 0 && !parsed->path.empty();
}

void ApiAdapterGLink::on_websocket_event_(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      this->connected_ = true;
      this->authenticated_ = false;
      this->connect_started_ms_ = 0;
      this->client_nonce_ = this->random_hex_(16);
      this->session_id_.clear();
      this->server_nonce_.clear();
      this->last_heartbeat_ms_ = millis();
      ESP_LOGI(TAG, "G-Link websocket connected");
      this->send_hello_();
      break;
    case WStype_DISCONNECTED:
      this->connected_ = false;
      this->authenticated_ = false;
      ESP_LOGW(TAG, "G-Link websocket disconnected");
      this->started_ = false;
      this->connect_started_ms_ = 0;
      this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
      break;
    case WStype_TEXT:
      this->handle_text_(std::string(reinterpret_cast<const char *>(payload), length));
      break;
    case WStype_ERROR:
      ESP_LOGW(TAG, "G-Link websocket error");
      break;
    default:
      break;
  }
}

void ApiAdapterGLink::handle_text_(const std::string &text) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, text);
  if (err) {
    ESP_LOGW(TAG, "Failed to decode G-Link frame: %s", err.c_str());
    return;
  }

  JsonObject frame = doc.as<JsonObject>();
  const char *type = frame["type"] | "";
  const char *ref_id = frame["id"] | "";
  JsonObject payload = frame["payload"].as<JsonObject>();

  if (strcmp(type, "challenge") == 0) {
    this->handle_challenge_(payload);
  } else if (strcmp(type, "command") == 0) {
    this->handle_command_(ref_id, payload);
  } else if (strcmp(type, "error") == 0) {
    const char *code = payload["code"] | "glink_error";
    ESP_LOGW(TAG, "G-Link error frame: %s", code);
  }
}

void ApiAdapterGLink::handle_challenge_(JsonObject payload) {
  this->session_id_ = payload["sessionId"].as<std::string>();
  this->server_nonce_ = payload["serverNonce"].as<std::string>();
  if (this->session_id_.empty() || this->server_nonce_.empty()) {
    ESP_LOGW(TAG, "Invalid G-Link challenge");
    return;
  }
  this->send_auth_();
  this->authenticated_ = true;
  ESP_LOGI(TAG, "G-Link device auth sent, session=%s", this->session_id_.c_str());
}

void ApiAdapterGLink::handle_command_(const std::string &ref_id, JsonObject payload) {
  const std::string command_id = payload["commandId"].as<std::string>();
  const std::string name = payload["name"].as<std::string>();
  if (ref_id.empty() || command_id.empty() || name.empty()) {
    ESP_LOGW(TAG, "Invalid G-Link command frame");
    return;
  }

  this->send_ack_(ref_id, command_id, "accepted");

  JsonDocument empty_body_doc;
  JsonObject body;
  json_object_or_empty(payload, &empty_body_doc, &body);

  JsonDocument response_doc;
  JsonObject response = response_doc.to<JsonObject>();
  std::string error = this->handle_gnode_command_(name, body, response);
  this->send_response_(ref_id, command_id, error.empty() ? "ok" : "error", response, error);
}

std::string ApiAdapterGLink::handle_gnode_command_(const std::string &name, JsonObject body, JsonObject response) {
  if (name == "demo.echo") {
    response["handledBy"] = this->device_serial_();
    response["name"] = name;
    response["ok"] = true;
    response["body"].set(body);
    return "";
  }

  if (name == "g-node.info.get") {
    this->core_->build_info(response);
  } else if (name == "g-node.status.get") {
    this->core_->build_status(response);
  } else if (name == "g-node.diagnostics.get") {
    this->core_->build_diagnostics(response);
  } else if (name == "g-node.consumption.get") {
    this->core_->build_consumption(response);
  } else if (name == "g-node.network.get") {
    this->core_->build_network(response);
  } else if (name == "g-node.network.mqtt.get") {
    this->core_->build_mqtt(response);
  } else if (name == "g-node.scheduler.get") {
    this->core_->build_scheduler(response);
  } else if (name == "g-node.region.get") {
    this->core_->build_region(response);
  } else if (name == "g-node.region.devices.get") {
    this->core_->build_region_devices(response);
  } else if (name == "g-node.settings.consumables.get") {
    this->core_->build_settings_consumables(response);
  } else if (name == "g-node.network.set") {
    const bool applied = this->core_->apply_network(body);
    this->core_->build_network(response);
    response["ok"] = true;
    response["applied"] = applied;
  } else if (name == "g-node.control.mode.set") {
    body["transport"] = "glink";
    body["causeKind"] = "mobile_api";
    if (!this->core_->handle_control_mode(body, response))
      return json_error(response, "command_failed");
  } else if (name == "g-node.control.identify.set") {
    this->core_->handle_identify(body, response);
  } else if (name == "g-node.control.restart.set") {
    this->core_->handle_restart(body, response);
  } else if (name == "g-node.control.service_ap.set") {
    this->core_->handle_service_ap(body, response);
  } else if (name == "g-node.scheduler.set") {
    const bool saved = this->core_->apply_scheduler(body);
    response["ok"] = true;
    response["saved"] = saved;
  } else if (name == "g-node.scheduler.state.set") {
    const bool saved = this->core_->apply_scheduler_state(body);
    response["ok"] = true;
    response["saved"] = saved;
  } else if (name == "g-node.region.set") {
    const bool saved = this->core_->apply_region(body);
    response["ok"] = true;
    response["saved"] = saved;
  } else if (name == "g-node.region.ping.set") {
    this->core_->handle_region_ping(body, response);
  } else if (name == "g-node.settings.consumables.set") {
    const bool saved = this->core_->apply_settings_consumables(body);
    response["ok"] = true;
    response["saved"] = saved;
  } else if (name == "g-node.control.factory_reset.set" || name == "g-node.control.clear_region.set" ||
             name == "g-node.control.clear_usage.set") {
    response["ok"] = false;
    response["message"] = "destructive command is excluded from G-Link firmware slice";
    response["command"] = name;
    return "unsupported";
  } else if (name.rfind("g-node.", 0) == 0) {
    response["ok"] = false;
    response["message"] = "unsupported G-Node command";
    response["command"] = name;
    return "unsupported";
  } else {
    response["ok"] = false;
    response["message"] = "unsupported command";
    response["command"] = name;
    return "unsupported";
  }

  return "";
}

void ApiAdapterGLink::send_hello_() {
  this->send_frame_("hello", "device", this->next_frame_id_("hello"), [this](JsonObject payload) {
    payload["serial"] = this->device_serial_();
    payload["mac"] = this->device_mac_();
    payload["model"] = this->device_model_();
    payload["keyId"] = this->key_id_;
    payload["clientNonce"] = this->client_nonce_;
  });
}

void ApiAdapterGLink::send_auth_() {
  const std::string input =
      this->device_serial_() + "|" + this->device_mac_() + "|" + this->client_nonce_ + "|" + this->server_nonce_ +
      "|" + this->session_id_;
  const std::string signature = this->hmac_sha256_hex_(input);
  this->send_frame_("auth", "device", this->next_frame_id_("auth"), [this, signature](JsonObject payload) {
    payload["keyId"] = this->key_id_;
    payload["signature"] = signature;
  });
}

void ApiAdapterGLink::send_heartbeat_() {
  this->send_frame_("heartbeat", "device", this->next_frame_id_("hb"), [](JsonObject payload) {
    payload["uptimeSec"] = millis() / 1000;
  });
}

void ApiAdapterGLink::send_ack_(const std::string &ref_id, const std::string &command_id, const char *status,
                                const std::string &error) {
  this->send_frame_("ack", "device", this->next_frame_id_("ack"),
                    [ref_id, command_id, status, error](JsonObject payload) {
                      payload["refId"] = ref_id;
                      payload["commandId"] = command_id;
                      payload["status"] = status;
                      if (!error.empty())
                        payload["error"] = error;
                    });
}

void ApiAdapterGLink::send_response_(const std::string &ref_id, const std::string &command_id, const char *status,
                                     JsonObject body, const std::string &error) {
  this->send_frame_("response", "device", this->next_frame_id_("response"),
                    [ref_id, command_id, status, body, error](JsonObject payload) {
                      payload["refId"] = ref_id;
                      payload["commandId"] = command_id;
                      payload["status"] = status;
                      payload["body"].set(body);
                      if (!error.empty())
                        payload["error"] = error;
                    });
}

void ApiAdapterGLink::send_radiation_event_(storage::RadiationMode mode, storage::RadiationSource source) {
  if (!this->authenticated_)
    return;

  const bool active = mode != storage::RadiationMode::OFF;
  this->send_frame_("event", "device", this->next_frame_id_("event"), [this, active, mode, source](JsonObject payload) {
    payload["kind"] = active ? "radiation.started" : "radiation.stopped";
    JsonObject body = payload["body"].to<JsonObject>();
    body["state"] = active ? "on" : "off";
    body["mode"] = radiation_mode_to_api(mode);
    body["source"] = storage::radiationSourceToApi(source);
    body["serial"] = this->device_serial_();
    body["uptimeSec"] = millis() / 1000;
  });
}

bool ApiAdapterGLink::send_frame_(const char *type, const char *peer, const std::string &id,
                                  std::function<void(JsonObject)> builder) {
  if (!this->connected_)
    return false;

  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["v"] = 1;
  root["type"] = type;
  root["id"] = id;
  root["ts"] = static_cast<uint32_t>(time(nullptr));
  root["peer"] = peer;
  JsonObject payload = root["payload"].to<JsonObject>();
  builder(payload);

  std::string out;
  serializeJson(doc, out);
  return this->websocket_.sendTXT(out.c_str(), out.length());
}

std::string ApiAdapterGLink::device_serial_() const {
  if (storage::store != nullptr)
    return storage::store->get_serial();
  return get_mac_address().substr(6);
}

std::string ApiAdapterGLink::device_mac_() const {
  uint8_t mac[6];
  get_mac_address_raw(mac);
  return storage::convertMacToStr(mac);
}

std::string ApiAdapterGLink::device_model_() const {
  if (storage::store != nullptr)
    return storage::store->get_model();
  return App.get_name();
}

std::string ApiAdapterGLink::next_frame_id_(const char *prefix) {
  this->frame_seq_++;
  return str_sprintf("fw_%s_%u_%u", prefix, millis(), this->frame_seq_);
}

std::string ApiAdapterGLink::random_hex_(size_t bytes) const {
  static const char *hex = "0123456789abcdef";
  std::string out;
  out.reserve(bytes * 2);
  for (size_t i = 0; i < bytes; i++) {
    const uint8_t value = static_cast<uint8_t>(esp_random() & 0xff);
    out.push_back(hex[value >> 4]);
    out.push_back(hex[value & 0x0f]);
  }
  return out;
}

std::string ApiAdapterGLink::hmac_sha256_hex_(const std::string &input) const {
  uint8_t digest[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, reinterpret_cast<const unsigned char *>(this->secret_.c_str()), this->secret_.size());
  mbedtls_md_hmac_update(&ctx, reinterpret_cast<const unsigned char *>(input.c_str()), input.size());
  mbedtls_md_hmac_finish(&ctx, digest);
  mbedtls_md_free(&ctx);

  char hex[65];
  for (size_t i = 0; i < sizeof(digest); i++)
    std::snprintf(hex + (i * 2), 3, "%02x", digest[i]);
  hex[64] = 0;
  return hex;
}

}  // namespace api_adapter_glink
}  // namespace esphome

#endif  // ESP32

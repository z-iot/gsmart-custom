#include "gsmart_wifi_manager.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/components/network/ip_address.h"

#include <algorithm>
#include <cstring>

#ifdef USE_ESP32
#include <esp_err.h>
#include <esp_wifi.h>
#endif

#ifdef USE_ESP8266
#include <ESP8266WiFi.h>
#endif

namespace esphome {
namespace gsmart_wifi_manager {

static const char *const TAG = "gsmart_wifi_manager";
static constexpr uint32_t WIFI_SETTINGS_PREF_ID = 99991201UL;
static constexpr uint32_t WIFI_SETTINGS_MAGIC = 0x47535746UL;  // GSWF
static constexpr uint8_t WIFI_SETTINGS_VERSION = 1;
static constexpr uint32_t SCAN_INTERVAL_MS = 60000UL;
static constexpr uint32_t SCAN_POLL_TIMEOUT_MS = 12000UL;

GsmartWifiManager *global_gsmart_wifi_manager = nullptr;

GsmartWifiManager::GsmartWifiManager() { global_gsmart_wifi_manager = this; }

void GsmartWifiManager::setup() {
  ESP_LOGCONFIG(TAG, "Setting up GSmart Wi-Fi Manager...");
  this->load_settings();
#ifdef USE_WIFI_SCAN_RESULTS_LISTENERS
  if (wifi::global_wifi_component != nullptr)
    wifi::global_wifi_component->add_scan_results_listener(this);
#endif
  this->update_sta_priority();
  this->apply_wifi_state();
  this->reconnect_sta_();
}

void GsmartWifiManager::loop() {
  const uint32_t now = millis();

  if (this->should_periodic_scan_() && !this->scan_pending_ && now - this->last_scan_time_ > SCAN_INTERVAL_MS)
    this->start_scan(false);

  if (this->scan_pending_)
    this->check_scan_results();
}

void GsmartWifiManager::dump_config() {
  ESP_LOGCONFIG(TAG, "GSmart Wi-Fi Manager:");
  ESP_LOGCONFIG(TAG, "  Manufacture networks: %u", static_cast<unsigned>(this->manufacture_networks_.size()));
  ESP_LOGCONFIG(TAG, "  Service SSID: %s", this->settings_.service_ssid);
  ESP_LOGCONFIG(TAG, "  Customer Primary SSID: %s", this->settings_.customer_primary_ssid);
  ESP_LOGCONFIG(TAG, "  Customer Secondary SSID: %s", this->settings_.customer_secondary_ssid);
  ESP_LOGCONFIG(TAG, "  Service AP: %s (%s)", this->settings_.service_ap_ssid,
                this->settings_.service_ap_enabled ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Region AP: %s (%s, policy: %s)", this->settings_.region_ap_ssid,
                this->settings_.region_ap_enabled ? "enabled" : "disabled",
                this->settings_.region_ap_sta_policy == 1 ? "ap_only" : "apsta");
}

void GsmartWifiManager::add_manufacture_network(const std::string &ssid, const std::string &password) {
  if (!ssid.empty())
    this->manufacture_networks_.push_back({ssid, password});
}

void GsmartWifiManager::copy_string_(char *dest, size_t size, const std::string &value, bool keep_if_empty) {
  if (size == 0 || (keep_if_empty && value.empty()))
    return;
  std::strncpy(dest, value.c_str(), size - 1);
  dest[size - 1] = '\0';
}

void GsmartWifiManager::set_sta_service(const std::string &ssid, const std::string &password) {
  this->copy_string_(this->settings_.service_ssid, sizeof(this->settings_.service_ssid), ssid);
  this->copy_string_(this->settings_.service_password, sizeof(this->settings_.service_password), password, !ssid.empty());
  this->save_settings();
  this->update_sta_priority();
  this->reconnect_sta_();
}

void GsmartWifiManager::set_sta_customer_primary(const std::string &ssid, const std::string &password) {
  this->copy_string_(this->settings_.customer_primary_ssid, sizeof(this->settings_.customer_primary_ssid), ssid);
  this->copy_string_(this->settings_.customer_primary_password, sizeof(this->settings_.customer_primary_password), password,
                     !ssid.empty());
  this->save_settings();
  this->update_sta_priority();
  this->reconnect_sta_();
}

void GsmartWifiManager::set_sta_customer_secondary(const std::string &ssid, const std::string &password) {
  this->copy_string_(this->settings_.customer_secondary_ssid, sizeof(this->settings_.customer_secondary_ssid), ssid);
  this->copy_string_(this->settings_.customer_secondary_password, sizeof(this->settings_.customer_secondary_password), password,
                     !ssid.empty());
  this->save_settings();
  this->update_sta_priority();
  this->reconnect_sta_();
}

void GsmartWifiManager::set_service_ap(const std::string &ssid, const std::string &password, bool enabled) {
  this->copy_string_(this->settings_.service_ap_ssid, sizeof(this->settings_.service_ap_ssid), ssid, true);
  this->copy_string_(this->settings_.service_ap_password, sizeof(this->settings_.service_ap_password), password, true);
  this->settings_.service_ap_enabled = enabled;
  this->save_settings();
  this->update_sta_priority();
  this->apply_wifi_state();
}

void GsmartWifiManager::set_region_ap(const std::string &ssid, const std::string &password, bool enabled,
                                      uint8_t sta_policy) {
  this->copy_string_(this->settings_.region_ap_ssid, sizeof(this->settings_.region_ap_ssid), ssid, true);
  this->copy_string_(this->settings_.region_ap_password, sizeof(this->settings_.region_ap_password), password, true);
  this->settings_.region_ap_enabled = enabled;
  this->settings_.region_ap_sta_policy = sta_policy == 1 ? 1 : 0;
  this->save_settings();
  this->update_sta_priority();
  this->apply_wifi_state();
  this->reconnect_sta_();
}

bool GsmartWifiManager::is_connected() const {
  return wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_connected();
}

std::string GsmartWifiManager::get_active_ssid() const {
  if (wifi::global_wifi_component == nullptr)
    return "";
  char ssid_buf[wifi::SSID_BUFFER_SIZE];
  return wifi::global_wifi_component->wifi_ssid_to(ssid_buf);
}

std::string GsmartWifiManager::get_ip_address() const {
  if (wifi::global_wifi_component == nullptr)
    return "0.0.0.0";
  auto ips = wifi::global_wifi_component->get_ip_addresses();
  char ip_buf[network::IP_ADDRESS_BUFFER_SIZE];
  ips[0].str_to(ip_buf);
  return std::string(ip_buf);
}

std::string GsmartWifiManager::get_active_ap_profile() const {
  if (this->settings_.service_ap_enabled)
    return "service_ap";
  if (this->settings_.region_ap_enabled)
    return "region_ap";
  return "none";
}

bool GsmartWifiManager::is_ap_active() const { return this->current_ap_enabled_; }

void GsmartWifiManager::start_scan(bool manual) {
  if (this->scan_pending_)
    return;

  this->manual_scan_ = manual;
  this->scan_cache_valid_ = false;
  this->scan_pending_ = true;
  this->last_scan_time_ = millis();

#ifdef USE_ESP32
  wifi_scan_config_t scan_config = {};
  scan_config.show_hidden = false;
  esp_err_t err = esp_wifi_scan_start(&scan_config, false);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_wifi_scan_start failed: %s", esp_err_to_name(err));
    this->scan_pending_ = false;
  } else {
    ESP_LOGD(TAG, "Started ESP32 Wi-Fi scan");
  }
#elif defined(USE_ESP8266)
  int complete = WiFi.scanComplete();
  if (complete == WIFI_SCAN_RUNNING) {
    return;
  }
  if (complete >= 0)
    WiFi.scanDelete();
  WiFi.scanNetworksAsync(nullptr, true);
  ESP_LOGD(TAG, "Started ESP8266 async Wi-Fi scan");
#else
  this->scan_pending_ = false;
#endif
}

void GsmartWifiManager::load_settings() {
  this->pref_ = global_preferences->make_preference<WifiSettings>(WIFI_SETTINGS_PREF_ID);
  if (!this->pref_.load(&this->settings_) || this->settings_.magic != WIFI_SETTINGS_MAGIC ||
      this->settings_.version != WIFI_SETTINGS_VERSION) {
    ESP_LOGI(TAG, "Initializing default Wi-Fi settings");
    std::memset(&this->settings_, 0, sizeof(WifiSettings));
    this->settings_.magic = WIFI_SETTINGS_MAGIC;
    this->settings_.version = WIFI_SETTINGS_VERSION;

    this->copy_string_(this->settings_.service_ssid, sizeof(this->settings_.service_ssid), "GsmartServiceHS");
    this->copy_string_(this->settings_.service_password, sizeof(this->settings_.service_password), "smart8888");

    std::string default_ap_ssid = "Gsmart-" + get_mac_address().substr(6);
    this->copy_string_(this->settings_.service_ap_ssid, sizeof(this->settings_.service_ap_ssid), default_ap_ssid);
    this->copy_string_(this->settings_.service_ap_password, sizeof(this->settings_.service_ap_password), "12345678");
    this->settings_.service_ap_enabled = false;
    this->settings_.region_ap_enabled = false;
    this->settings_.region_ap_sta_policy = 0;

    this->save_settings();
  }
}

void GsmartWifiManager::save_settings() {
  this->pref_.save(&this->settings_);
  global_preferences->sync();
}

void GsmartWifiManager::update_sta_priority() {
  auto *wifi = wifi::global_wifi_component;
  if (wifi == nullptr)
    return;

  wifi->clear_sta();

  if (this->settings_.region_ap_enabled && this->settings_.region_ap_sta_policy == 1 &&
      !this->settings_.service_ap_enabled) {
    ESP_LOGI(TAG, "Region AP_ONLY policy active; STA list disabled");
    wifi->init_sta(0);
    return;
  }

  size_t count = this->manufacture_networks_.size();
  if (this->settings_.service_ssid[0] != '\0')
    count++;
  if (this->settings_.customer_primary_ssid[0] != '\0')
    count++;
  if (this->settings_.customer_secondary_ssid[0] != '\0')
    count++;

  wifi->init_sta(count);

  for (const auto &net : this->manufacture_networks_) {
    wifi::WiFiAP ap;
    ap.set_ssid(net.ssid);
    ap.set_password(net.password);
    ap.set_priority(100);
    wifi->add_sta(ap);
  }

  if (this->settings_.service_ssid[0] != '\0') {
    wifi::WiFiAP ap;
    ap.set_ssid(this->settings_.service_ssid);
    ap.set_password(this->settings_.service_password);
    ap.set_priority(50);
    wifi->add_sta(ap);
  }

  if (this->settings_.customer_primary_ssid[0] != '\0') {
    wifi::WiFiAP ap;
    ap.set_ssid(this->settings_.customer_primary_ssid);
    ap.set_password(this->settings_.customer_primary_password);
    ap.set_priority(10);
    wifi->add_sta(ap);
  }

  if (this->settings_.customer_secondary_ssid[0] != '\0') {
    wifi::WiFiAP ap;
    ap.set_ssid(this->settings_.customer_secondary_ssid);
    ap.set_password(this->settings_.customer_secondary_password);
    ap.set_priority(5);
    wifi->add_sta(ap);
  }
}

void GsmartWifiManager::apply_wifi_state() {
  std::string ssid;
  std::string password;
  bool active = false;
  bool ap_only = false;

  if (this->settings_.service_ap_enabled) {
    ssid = this->settings_.service_ap_ssid;
    password = this->settings_.service_ap_password;
    active = true;
  } else if (this->settings_.region_ap_enabled) {
    ssid = this->settings_.region_ap_ssid;
    password = this->settings_.region_ap_password;
    active = true;
    ap_only = this->settings_.region_ap_sta_policy == 1;
  }

  this->apply_soft_ap_(active, ssid, password, ap_only);
  if (active)
    this->start_scan(true);
}

void GsmartWifiManager::apply_soft_ap_(bool active, const std::string &ssid, const std::string &password, bool ap_only) {
  if (wifi::global_wifi_component != nullptr) {
    wifi::WiFiAP ap;
    ap.set_ssid(ssid);
    ap.set_password(password);
    wifi::global_wifi_component->set_ap(ap);
  }

#ifdef USE_ESP32
  wifi_mode_t current_mode = WIFI_MODE_NULL;
  esp_err_t err = esp_wifi_get_mode(&current_mode);
  if (err != ESP_OK)
    current_mode = WIFI_MODE_NULL;

  bool sta_enabled = current_mode == WIFI_MODE_STA || current_mode == WIFI_MODE_APSTA;
  bool ap_enabled = current_mode == WIFI_MODE_AP || current_mode == WIFI_MODE_APSTA;

  if (active) {
    if (ssid.empty()) {
      ESP_LOGW(TAG, "SoftAP requested without SSID");
      return;
    }

    wifi_config_t conf = {};
    std::memcpy(reinterpret_cast<char *>(conf.ap.ssid), ssid.c_str(), std::min<size_t>(ssid.size(), 32));
    conf.ap.ssid_len = std::min<size_t>(ssid.size(), 32);
    conf.ap.channel = 1;
    conf.ap.ssid_hidden = 0;
    conf.ap.max_connection = 5;
    conf.ap.beacon_interval = 100;
    conf.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
    if (password.empty()) {
      conf.ap.authmode = WIFI_AUTH_OPEN;
    } else {
      conf.ap.authmode = WIFI_AUTH_WPA2_PSK;
      std::memcpy(reinterpret_cast<char *>(conf.ap.password), password.c_str(), std::min<size_t>(password.size(), 64));
    }

    wifi_mode_t new_mode = ap_only ? WIFI_MODE_AP : WIFI_MODE_APSTA;
    err = esp_wifi_set_mode(new_mode);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_wifi_set_mode(%d) failed: %s", new_mode, esp_err_to_name(err));
      return;
    }
    err = esp_wifi_set_config(WIFI_IF_AP, &conf);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_wifi_set_config(WIFI_IF_AP) failed: %s", esp_err_to_name(err));
      return;
    }
  } else if (ap_enabled) {
    wifi_mode_t new_mode = sta_enabled ? WIFI_MODE_STA : WIFI_MODE_NULL;
    err = esp_wifi_set_mode(new_mode);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_wifi_set_mode(%d) failed: %s", new_mode, esp_err_to_name(err));
      return;
    }
  }
#elif defined(USE_ESP8266)
  if (active) {
    if (ssid.empty()) {
      ESP_LOGW(TAG, "SoftAP requested without SSID");
      return;
    }
    WiFi.softAP(ssid.c_str(), password.empty() ? nullptr : password.c_str());
    WiFi.mode(ap_only ? WIFI_AP : WIFI_AP_STA);
  } else {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
  }
#endif

  this->current_ap_enabled_ = active;
  this->current_ap_ssid_ = active ? ssid : "";
  this->current_ap_password_ = active ? password : "";
  ESP_LOGI(TAG, "SoftAP %s%s%s", active ? "active: " : "disabled", active ? ssid.c_str() : "", ap_only ? " (AP only)" : "");
}

void GsmartWifiManager::reconnect_sta_() {
  auto *wifi = wifi::global_wifi_component;
  if (wifi == nullptr || !wifi->has_sta())
    return;
  this->defer([wifi]() {
    if (wifi->has_sta())
      wifi->start_scanning();
  });
}

void GsmartWifiManager::trigger_reconnect_() {
#ifdef USE_ESP32
  esp_wifi_disconnect();
#elif defined(USE_ESP8266)
  WiFi.disconnect();
#endif
  this->reconnect_sta_();
}

uint8_t GsmartWifiManager::priority_for_ssid_(const std::string &ssid) const {
  if (ssid.empty())
    return 0;
  for (const auto &net : this->manufacture_networks_) {
    if (net.ssid == ssid)
      return 100;
  }
  if (ssid == this->settings_.service_ssid)
    return 50;
  if (ssid == this->settings_.customer_primary_ssid)
    return 10;
  if (ssid == this->settings_.customer_secondary_ssid)
    return 5;
  return 0;
}

uint8_t GsmartWifiManager::current_sta_priority_() const { return this->priority_for_ssid_(this->get_active_ssid()); }

uint8_t GsmartWifiManager::highest_configured_sta_priority_() const {
  if (!this->manufacture_networks_.empty())
    return 100;
  if (this->settings_.service_ssid[0] != '\0')
    return 50;
  if (this->settings_.customer_primary_ssid[0] != '\0')
    return 10;
  if (this->settings_.customer_secondary_ssid[0] != '\0')
    return 5;
  return 0;
}

bool GsmartWifiManager::should_periodic_scan_() const {
  if (this->is_ap_active())
    return true;

  auto *wifi = wifi::global_wifi_component;
  if (wifi == nullptr || !wifi->has_sta() || !this->is_connected())
    return false;

  return this->current_sta_priority_() < this->highest_configured_sta_priority_();
}

void GsmartWifiManager::cache_scan_result_(const std::string &ssid, int8_t rssi, uint8_t channel, bool secure) {
  if (ssid.empty())
    return;
  auto it = std::find_if(this->scan_cache_.begin(), this->scan_cache_.end(),
                         [&ssid](const WifiScanCacheItem &item) { return item.ssid == ssid; });
  uint8_t priority = this->priority_for_ssid_(ssid);
  if (it == this->scan_cache_.end()) {
    this->scan_cache_.push_back({ssid, rssi, channel, secure, priority, priority > 0});
  } else if (rssi > it->rssi) {
    it->rssi = rssi;
    it->channel = channel;
    it->secure = secure;
    it->priority = priority;
    it->known = priority > 0;
  }
}

void GsmartWifiManager::on_wifi_scan_results(const wifi::wifi_scan_vector_t<wifi::WiFiScanResult> &results) {
  this->scan_cache_.clear();
  for (const auto &result : results)
    this->cache_scan_result_(result.get_ssid().str(), result.get_rssi(), result.get_channel(), result.get_with_auth());

  this->process_scan_results_();
  this->scan_pending_ = false;
  this->scan_cache_valid_ = true;
}

void GsmartWifiManager::check_scan_results() {
#ifdef USE_ESP8266
  int complete = WiFi.scanComplete();
  if (complete == WIFI_SCAN_RUNNING) {
    if (millis() - this->last_scan_time_ > SCAN_POLL_TIMEOUT_MS) {
      ESP_LOGW(TAG, "ESP8266 Wi-Fi scan timed out");
      WiFi.scanDelete();
      this->scan_pending_ = false;
    }
    return;
  }
  if (complete < 0)
    return;

  this->scan_cache_.clear();
  for (int i = 0; i < complete; i++) {
    this->cache_scan_result_(WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i), WiFi.encryptionType(i) != ENC_TYPE_NONE);
  }
  WiFi.scanDelete();
  this->process_scan_results_();
  this->scan_pending_ = false;
  this->scan_cache_valid_ = true;
#else
  if (millis() - this->last_scan_time_ > SCAN_POLL_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Wi-Fi scan timed out");
    this->scan_pending_ = false;
  }
#endif
}

void GsmartWifiManager::process_scan_results_() {
  if (this->scan_cache_.empty())
    return;

  std::sort(this->scan_cache_.begin(), this->scan_cache_.end(), [](const WifiScanCacheItem &a, const WifiScanCacheItem &b) {
    if (a.priority != b.priority)
      return a.priority > b.priority;
    return a.rssi > b.rssi;
  });

  const auto &best = this->scan_cache_.front();
  uint8_t current_priority = this->current_sta_priority_();
  if (best.known && best.priority > current_priority && this->is_connected()) {
    ESP_LOGI(TAG, "Higher priority Wi-Fi visible (%s, priority %u > %u), reconnecting", best.ssid.c_str(),
             best.priority, current_priority);
    this->trigger_reconnect_();
  }
}

}  // namespace gsmart_wifi_manager
}  // namespace esphome

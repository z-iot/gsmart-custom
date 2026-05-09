#include "gsmart_wifi_manager.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32
#include <esp_wifi.h>
#endif

namespace esphome {
namespace gsmart_wifi_manager {

static const char *const TAG = "gsmart_wifi_manager";

GsmartWifiManager *global_gsmart_wifi_manager = nullptr;

GsmartWifiManager::GsmartWifiManager() {
    global_gsmart_wifi_manager = this;
}

void GsmartWifiManager::setup() {
    ESP_LOGCONFIG(TAG, "Setting up GSmart Wi-Fi Manager...");
    this->load_settings();
    this->update_sta_priority();
    this->apply_wifi_state();
}

void GsmartWifiManager::loop() {
    uint32_t now = millis();
    
    // Scan logic when SoftAP is active
    if (this->is_ap_active() && (now - this->last_scan_time_ > 60000)) {
        this->start_scan();
    }
    
    if (this->scan_pending_) {
        this->check_scan_results();
    }
}

void GsmartWifiManager::dump_config() {
    ESP_LOGCONFIG(TAG, "GSmart Wi-Fi Manager:");
    ESP_LOGCONFIG(TAG, "  Service SSID: %s", settings_.service_ssid);
    ESP_LOGCONFIG(TAG, "  Customer Primary SSID: %s", settings_.customer_primary_ssid);
    ESP_LOGCONFIG(TAG, "  Customer Secondary SSID: %s", settings_.customer_secondary_ssid);
    ESP_LOGCONFIG(TAG, "  Service AP: %s (%s)", settings_.service_ap_ssid, settings_.service_ap_enabled ? "enabled" : "disabled");
    ESP_LOGCONFIG(TAG, "  Region AP: %s (%s, policy: %s)", settings_.region_ap_ssid, 
        settings_.region_ap_enabled ? "enabled" : "disabled",
        settings_.region_ap_sta_policy == 1 ? "ap_only" : "apsta");
}

void GsmartWifiManager::add_manufacture_network(const std::string &ssid, const std::string &password) {
    this->manufacture_networks_.push_back({ssid, password});
}

void GsmartWifiManager::set_sta_service(const std::string &ssid, const std::string &password) {
    strncpy(settings_.service_ssid, ssid.c_str(), 32);
    strncpy(settings_.service_password, password.c_str(), 64);
    this->save_settings();
    this->update_sta_priority();
}

void GsmartWifiManager::set_sta_customer_primary(const std::string &ssid, const std::string &password) {
    strncpy(settings_.customer_primary_ssid, ssid.c_str(), 32);
    strncpy(settings_.customer_primary_password, password.c_str(), 64);
    this->save_settings();
    this->update_sta_priority();
}

void GsmartWifiManager::set_sta_customer_secondary(const std::string &ssid, const std::string &password) {
    strncpy(settings_.customer_secondary_ssid, ssid.c_str(), 32);
    strncpy(settings_.customer_secondary_password, password.c_str(), 64);
    this->save_settings();
    this->update_sta_priority();
}

void GsmartWifiManager::set_service_ap(const std::string &ssid, const std::string &password, bool enabled) {
    if (!ssid.empty()) strncpy(settings_.service_ap_ssid, ssid.c_str(), 32);
    if (!password.empty()) strncpy(settings_.service_ap_password, password.c_str(), 64);
    settings_.service_ap_enabled = enabled;
    this->save_settings();
    this->apply_wifi_state();
}

void GsmartWifiManager::set_region_ap(const std::string &ssid, const std::string &password, bool enabled, uint8_t sta_policy) {
    if (!ssid.empty()) strncpy(settings_.region_ap_ssid, ssid.c_str(), 32);
    if (!password.empty()) strncpy(settings_.region_ap_password, password.c_str(), 64);
    settings_.region_ap_enabled = enabled;
    settings_.region_ap_sta_policy = sta_policy;
    this->save_settings();
    this->update_sta_priority(); // Policy might change STA connectivity
    this->apply_wifi_state();
}

bool GsmartWifiManager::is_connected() const {
    return wifi::global_wifi_component->is_connected();
}

std::string GsmartWifiManager::get_active_ssid() const {
    char ssid_buf[wifi::SSID_BUFFER_SIZE];
    return wifi::global_wifi_component->wifi_ssid_to(ssid_buf);
}

std::string GsmartWifiManager::get_ip_address() const {
    auto ips = wifi::global_wifi_component->get_ip_addresses();
    if (ips.empty()) return "0.0.0.0";
    char ip_buf[network::IP_ADDRESS_BUFFER_SIZE];
    ips[0].str_to(ip_buf);
    return std::string(ip_buf);
}

std::string GsmartWifiManager::get_active_ap_profile() const {
    if (settings_.service_ap_enabled) return "service_ap";
    if (settings_.region_ap_enabled) return "region_ap";
    return "none";
}

bool GsmartWifiManager::is_ap_active() const {
    return wifi::global_wifi_component->is_ap_active();
}

void GsmartWifiManager::start_scan(bool manual) {
    if (this->scan_pending_) return;
    
    ESP_LOGD(TAG, "Starting Wi-Fi scan...");
    this->manual_scan_ = manual;
    
#ifdef USE_ESP32
    wifi_scan_config_t scan_config = {};
    scan_config.show_hidden = true;
    esp_wifi_scan_start(&scan_config, false);
    this->scan_pending_ = true;
#elif defined(USE_ESP8266)
    wifi_scan_config_t scan_config = {};
    scan_config.show_hidden = true;
    // ESP8266 scan start is a bit different in ESPHome wrapper but let's try direct or wrapper
    wifi::global_wifi_component->start_scanning();
    this->scan_pending_ = true;
#endif
    this->last_scan_time_ = millis();
}

void GsmartWifiManager::load_settings() {
    this->pref_ = global_preferences->make_preference<WifiSettings>(99991201UL);
    if (!this->pref_.load(&this->settings_) || this->settings_.magic != 0x47534D54 || this->settings_.version != 1) {
        ESP_LOGI(TAG, "Initializing default Wi-Fi settings...");
        memset(&this->settings_, 0, sizeof(WifiSettings));
        this->settings_.magic = 0x47534D54;
        this->settings_.version = 1;
        
        // Defaults
        strncpy(this->settings_.service_ssid, "GsmartServiceHS", 32);
        strncpy(this->settings_.service_password, "smart8888", 64);
        
        std::string mac = get_mac_address().substr(6); // last 3 bytes
        std::string default_ap_ssid = "Gsmart-" + mac;
        strncpy(this->settings_.service_ap_ssid, default_ap_ssid.c_str(), 32);
        strncpy(this->settings_.service_ap_password, "12345678", 64); // Fallback password
        this->settings_.service_ap_enabled = false;
        
        this->settings_.region_ap_enabled = false;
        this->settings_.region_ap_sta_policy = 0; // apsta
        
        this->save_settings();
    }
}

void GsmartWifiManager::save_settings() {
    this->pref_.save(&this->settings_);
    global_preferences->sync();
}

void GsmartWifiManager::update_sta_priority() {
    auto *wifi = wifi::global_wifi_component;
    wifi->clear_sta();
    
    // Check if we are in ap_only mode
    if (settings_.region_ap_enabled && settings_.region_ap_sta_policy == 1 && !settings_.service_ap_enabled) {
        ESP_LOGI(TAG, "AP_ONLY policy active, disabling STA networks.");
        return;
    }

    // 1. Manufacture
    for (const auto &net : manufacture_networks_) {
        wifi::WiFiAP ap;
        ap.set_ssid(net.ssid);
        ap.set_password(net.password);
        ap.set_priority(100);
        wifi->add_sta(ap);
    }
    
    // 2. Service
    if (settings_.service_ssid[0] != 0) {
        wifi::WiFiAP ap;
        ap.set_ssid(settings_.service_ssid);
        ap.set_password(settings_.service_password);
        ap.set_priority(50);
        wifi->add_sta(ap);
    }
    
    // 3. Customer Primary
    if (settings_.customer_primary_ssid[0] != 0) {
        wifi::WiFiAP ap;
        ap.set_ssid(settings_.customer_primary_ssid);
        ap.set_password(settings_.customer_primary_password);
        ap.set_priority(10);
        wifi->add_sta(ap);
    }
    
    // 4. Customer Secondary
    if (settings_.customer_secondary_ssid[0] != 0) {
        wifi::WiFiAP ap;
        ap.set_ssid(settings_.customer_secondary_ssid);
        ap.set_password(settings_.customer_secondary_password);
        ap.set_priority(5);
        wifi->add_sta(ap);
    }
}

void GsmartWifiManager::apply_wifi_state() {
    auto *wifi = wifi::global_wifi_component;
    wifi::WiFiAP ap_cfg;
    bool ap_active = false;

    if (settings_.service_ap_enabled) {
        ap_cfg.set_ssid(settings_.service_ap_ssid);
        ap_cfg.set_password(settings_.service_ap_password);
        ap_active = true;
    } else if (settings_.region_ap_enabled) {
        ap_cfg.set_ssid(settings_.region_ap_ssid);
        ap_cfg.set_password(settings_.region_ap_password);
        ap_active = true;
    }

    if (ap_active) {
        ESP_LOGI(TAG, "Activating SoftAP: %s", ap_cfg.get_ssid().c_str());
        wifi->set_ap(ap_cfg);
        wifi->enableAp();
    } else {
        ESP_LOGI(TAG, "Deactivating SoftAP");
        wifi->disableAp();
    }
}

void GsmartWifiManager::check_scan_results() {
    // In ESPHome, the wifi component already handles scan results and populates its internal list.
    // We can check if a higher priority network is available.
    
#ifdef USE_ESP32
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) {
        // Scan might still be in progress or no networks found
        return; 
    }
    this->scan_pending_ = false;
#elif defined(USE_ESP8266)
    // ESP8266 scan finished check
    // For now let's assume it finished after some time or use the callback if we could register it
    this->scan_pending_ = false;
#endif

    // Logic to check if we should reconnect to a better network
    // ESPHome wifi component usually does this automatically if multiple networks are configured.
    // However, we might want to force a disconnect if we are connected to a lower priority network
    // and a higher one is now visible.
}

}  // namespace gsmart_wifi_manager
}  // namespace esphome

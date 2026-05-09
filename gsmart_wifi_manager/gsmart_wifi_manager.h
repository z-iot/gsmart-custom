#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/wifi/wifi_component.h"
#include <vector>
#include <string>

namespace esphome {
namespace gsmart_wifi_manager {

struct WifiNetwork {
    std::string ssid;
    std::string password;
};

struct WifiSettings {
    uint32_t magic;
    uint8_t version;
    char service_ssid[33];
    char service_password[65];
    char customer_primary_ssid[33];
    char customer_primary_password[65];
    char customer_secondary_ssid[33];
    char customer_secondary_password[65];
    char service_ap_ssid[33];
    char service_ap_password[65];
    bool service_ap_enabled;
    char region_ap_ssid[33];
    char region_ap_password[65];
    bool region_ap_enabled;
    uint8_t region_ap_sta_policy; // 0: apsta, 1: ap_only
};

class GsmartWifiManager : public Component {
public:
    GsmartWifiManager();
    void setup() override;
    void loop() override;
    void dump_config() override;
    float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

    void add_manufacture_network(const std::string &ssid, const std::string &password);
    
    // STA management
    void set_sta_service(const std::string &ssid, const std::string &password);
    void set_sta_customer_primary(const std::string &ssid, const std::string &password);
    void set_sta_customer_secondary(const std::string &ssid, const std::string &password);
    
    // SoftAP management
    void set_service_ap(const std::string &ssid, const std::string &password, bool enabled);
    void set_region_ap(const std::string &ssid, const std::string &password, bool enabled, uint8_t sta_policy);
    
    // Runtime
    bool is_connected() const;
    std::string get_active_ssid() const;
    std::string get_ip_address() const;
    std::string get_active_ap_profile() const;
    bool is_ap_active() const;
    
    void start_scan(bool manual = false);
    void save_settings();
    const WifiSettings& get_settings() const { return settings_; }

protected:
    void load_settings();
    void apply_wifi_state();
    void update_sta_priority();
    void perform_scan();
    void check_scan_results();

    std::vector<WifiNetwork> manufacture_networks_;
    WifiSettings settings_;
    ESPPreferenceObject pref_;
    
    uint32_t last_scan_time_ = 0;
    bool scan_pending_ = false;
    bool manual_scan_ = false;
    
    std::string current_ap_ssid_;
    std::string current_ap_password_;
    bool current_ap_enabled_ = false;
};

extern GsmartWifiManager *global_gsmart_wifi_manager;

}  // namespace gsmart_wifi_manager
}  // namespace esphome

#pragma once

#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include <string>
#include <vector>

namespace esphome {
namespace gsmart_wifi_manager {

struct WifiNetwork {
  std::string ssid;
  std::string password;
};

// sta_mode values: 0=off, 1=on, 2=periodic, 3=force
// cloud_mode values: 0=off, 1=service, 2=full, 3=stat, 4=periodic

struct WifiSettingsClient {
  uint32_t magic;
  uint8_t version;
  char customer_primary_ssid[33];      // default empty
  char customer_primary_password[65];  // default empty
  char customer_secondary_ssid[33];    // default empty
  char customer_secondary_password[65];// default empty
  uint8_t sta_mode;                    // 0=off, 1=on, 2=periodic, 3=force
  char service_ssid[33];               // default GSmartService-HS
  char service_password[65];           // default smart8888
  uint8_t service_mode;                // 0=off, 1=on
};

struct WifiSettingsAp {
  uint32_t magic;
  uint8_t version;
  char region_ap_ssid[33];             // default empty
  char region_ap_password[65];         // default empty
  uint8_t region_ap_channel;           // 0=auto, 1-14=specific WiFi channel
  uint8_t region_ap_mode;              // 0=off, 1=on
  char service_ap_password[65];        // default 12345678 (SSID always Gsmart-<serial>)
  uint8_t service_ap_mode;             // 0=off, 1=on
};

struct CloudSettings {
  uint32_t magic;
  uint8_t version;
  uint8_t cloud_mode; // 0=off, 1=service, 2=full, 3=stat, 4=periodic
};

struct WifiScanCacheItem {
  std::string ssid;
  int8_t rssi{0};
  uint8_t channel{0};
  bool secure{false};
  uint8_t priority{0};
  bool known{false};
};

class GsmartWifiManager : public Component,
                          public wifi::WiFiScanResultsListener {
public:
  GsmartWifiManager();
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override {
    return setup_priority::AFTER_WIFI;
  }
  void on_wifi_scan_results(
      const wifi::wifi_scan_vector_t<wifi::WiFiScanResult> &results) override;

  void add_manufacture_network(const std::string &ssid,
                               const std::string &password);

  // STA management
  void set_sta_service(const std::string &ssid, const std::string &password,
                       uint8_t mode);
  void set_sta_customer_primary(const std::string &ssid,
                                const std::string &password);
  void set_sta_customer_secondary(const std::string &ssid,
                                  const std::string &password);
  void set_sta_mode(uint8_t mode); // 0=off,1=on,2=periodic,3=force

  // SoftAP management
  void set_service_ap(const std::string &password, uint8_t mode);
  void set_region_ap(const std::string &ssid, const std::string &password,
                     uint8_t mode, uint8_t channel);

  // Cloud settings
  void set_cloud_mode(uint8_t mode); // 0=off,1=service,2=full,3=stat,4=periodic
  const CloudSettings &get_cloud_settings() const { return cloud_settings_; }
  void save_cloud_settings();

  // Runtime
  bool is_connected() const;
  std::string get_active_ssid() const;
  std::string get_ip_address() const;
  std::string get_active_ap_profile() const;
  bool is_ap_active() const;

  void start_scan(bool manual = false);
  bool has_scan_results() const { return this->scan_cache_valid_; }
  const std::vector<WifiScanCacheItem> &get_scan_results() const {
    return this->scan_cache_;
  }
  void save_client_settings();
  void save_ap_settings();
  const WifiSettingsClient &get_client_settings() const {
    return client_settings_;
  }
  const WifiSettingsAp &get_ap_settings() const { return ap_settings_; }

protected:
  void load_settings();
  void apply_wifi_state();
  void update_sta_priority();
  void reconnect_sta_();
  void trigger_reconnect_();
  void apply_soft_ap_(bool active, const std::string &ssid,
                      const std::string &password, bool ap_only,
                      uint8_t channel = 0);
  void check_scan_results();
  void process_scan_results_();
  void cache_scan_result_(const std::string &ssid, int8_t rssi, uint8_t channel,
                          bool secure);
  uint8_t priority_for_ssid_(const std::string &ssid) const;
  uint8_t current_sta_priority_() const;
  uint8_t highest_configured_sta_priority_() const;
  bool should_periodic_scan_() const;
  void copy_string_(char *dest, size_t size, const std::string &value,
                    bool keep_if_empty = false);

  std::vector<WifiNetwork> manufacture_networks_;
  std::vector<WifiScanCacheItem> scan_cache_;
  WifiSettingsClient client_settings_;
  WifiSettingsAp ap_settings_;
  CloudSettings cloud_settings_;
  ESPPreferenceObject client_pref_;
  ESPPreferenceObject ap_pref_;
  ESPPreferenceObject cloud_pref_;

  uint32_t last_scan_time_ = 0;
  bool scan_pending_ = false;
  bool manual_scan_ = false;
  bool scan_cache_valid_ = false;

  std::string current_ap_ssid_;
  std::string current_ap_password_;
  bool current_ap_enabled_ = false;
};

extern GsmartWifiManager *global_gsmart_wifi_manager;

} // namespace gsmart_wifi_manager
} // namespace esphome

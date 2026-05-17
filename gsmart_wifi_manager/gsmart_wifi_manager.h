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

struct WifiSettingsServiceAp {
  uint32_t magic;
  uint8_t version;
  int32_t startup_timeout_min;          // -1=do not start on boot, 0=permanent, >0=auto-off minutes
  int32_t manual_timeout_min;           // 0=permanent, >0=auto-off minutes
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
  static constexpr int32_t SERVICE_AP_TIMEOUT_USE_DEFAULT = -2147483647 - 1;

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
  void set_service_ap_timeouts(int32_t startup_timeout_min, int32_t manual_timeout_min);
  void save_service_ap_settings();
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
  bool is_service_ap_active() const;
  bool is_service_ap_auto_off_scheduled() const { return this->service_ap_auto_off_scheduled_; }
  uint32_t get_service_ap_auto_off_remaining_sec() const;
  std::string get_service_ap_ssid() const;
  void start_service_ap(const std::string &password = "", int32_t timeout_min = SERVICE_AP_TIMEOUT_USE_DEFAULT);
  void start_service_ap_for_duration(const std::string &password, uint32_t duration_sec);
  void stop_service_ap();
  void set_service_ap_runtime(bool active);
  bool toggle_service_ap();

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
  const WifiSettingsServiceAp &get_service_ap_settings() const { return service_ap_settings_; }

protected:
  void load_settings();
  void apply_service_ap_startup_policy_();
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
  bool is_soft_ap_running_() const;
  std::string get_soft_ap_runtime_ssid_() const;
  bool soft_ap_matches_expected_() const;
  void ensure_soft_ap_state_();
  uint32_t timeout_min_to_seconds_(int32_t timeout_min) const;
  void schedule_service_ap_auto_off_(uint32_t duration_sec);
  void cancel_service_ap_auto_off_();
  void copy_string_(char *dest, size_t size, const std::string &value,
                    bool keep_if_empty = false);

  std::vector<WifiNetwork> manufacture_networks_;
  std::vector<WifiScanCacheItem> scan_cache_;
  WifiSettingsClient client_settings_;
  WifiSettingsAp ap_settings_;
  WifiSettingsServiceAp service_ap_settings_;
  CloudSettings cloud_settings_;
  ESPPreferenceObject client_pref_;
  ESPPreferenceObject ap_pref_;
  ESPPreferenceObject service_ap_pref_;
  ESPPreferenceObject cloud_pref_;

  uint32_t last_scan_time_ = 0;
  bool scan_pending_ = false;
  bool manual_scan_ = false;
  bool scan_cache_valid_ = false;

  std::string current_ap_ssid_;
  std::string current_ap_password_;
  bool current_ap_enabled_ = false;
  bool service_ap_runtime_active_ = false;
  bool service_ap_auto_off_scheduled_ = false;
  uint32_t service_ap_auto_off_at_ = 0;
  uint32_t last_soft_ap_reapply_ms_ = 0;
};

extern GsmartWifiManager *global_gsmart_wifi_manager;

} // namespace gsmart_wifi_manager
} // namespace esphome

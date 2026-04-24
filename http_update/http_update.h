#pragma once

#include "esphome/components/socket/socket.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"

#include "esphome/components/json/json_util.h"
#include "esphome/core/automation.h"
#include <list>
#include <map>
#include <utility>
#include <memory>

#ifdef USE_ESP32
#include <HTTPClient.h>
#endif
#ifdef USE_ESP8266
#include <ESP8266HTTPClient.h>
#ifdef USE_HTTP_UPDATE_ESP8266_HTTPS
#include <WiFiClientSecure.h>
#endif
#endif

namespace esphome {
namespace http_update {

struct Header {
  const char *name;
  const char *value;
};

class HttpUpdateResponseTrigger;

enum HttpUpdateState { HTTPUPDATE_COMPLETED = 0, HTTPUPDATE_STARTED, HTTPUPDATE_IN_PROGRESS, HTTPUPDATE_ERROR };

class HttpUpdateComponent : public Component {
 public:
#ifdef USE_HTTPUPDATE_PASSWORD
  void set_auth_password(const std::string &password) { password_ = password; }
#endif 

  bool should_enter_safe_mode(uint8_t num_attempts, uint32_t enable_time);

  /// Set to true if the next startup will enter safe mode
  void set_safe_mode_pending(const bool &pending);
  bool get_safe_mode_pending();

#ifdef USE_HTTPUPDATE_STATE_CALLBACK
  void add_on_state_callback(std::function<void(HttpUpdateState, float, uint8_t)> &&callback);
#endif

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

  void clean_rtc();

  void on_safe_shutdown() override;


  void set_url(std::string url);
  void set_method(const char *method) { this->method_ = method; }
  void set_useragent(const char *useragent) { this->useragent_ = useragent; }
  void set_timeout_req(uint16_t timeout) { this->timeout_ = timeout; }
  void set_follow_redirects(bool follow_redirects) { this->follow_redirects_ = follow_redirects; }
  void set_redirect_limit(uint16_t limit) { this->redirect_limit_ = limit; }
  void set_headers(std::list<Header> headers) { this->headers_ = std::move(headers); }
  void flash(const std::vector<HttpUpdateResponseTrigger *> &response_triggers);
  void close();
  const char *get_string();
  void set_fwver(const std::string &fwver) { fwver_ = fwver; }

 protected:
  bool runUpdate(Stream &fw_stream, uint32_t ota_size, String md5);
  void write_rtc_(uint32_t val);
  uint32_t read_rtc_();

  void handle_();

  std::string fwver_;

#ifdef USE_HTTPUPDATE_PASSWORD
  std::string password_;
#endif

  // std::unique_ptr<socket::Socket> server_;
  // std::unique_ptr<socket::Socket> client_;

  bool has_safe_mode_{false};              ///< stores whether safe mode can be enabled.
  uint32_t safe_mode_start_time_;          ///< stores when safe mode was enabled.
  uint32_t safe_mode_enable_time_{60000};  ///< The time safe mode should be on for.
  uint32_t safe_mode_rtc_value_;
  uint8_t safe_mode_num_attempts_;
  ESPPreferenceObject rtc_;

  static const uint32_t ENTER_SAFE_MODE_MAGIC =
      0x5afe5afe;  ///< a magic number to indicate that safe mode should be entered on next boot

#ifdef USE_HTTPUPDATE_STATE_CALLBACK
  CallbackManager<void(HttpUpdateState, float, uint8_t)> state_callback_{};
#endif

  HTTPClient client_{};
  std::string url_;
  std::string last_url_;
  const char *method_;
  const char *useragent_{nullptr};
  bool secure_;
  bool follow_redirects_;
  uint16_t redirect_limit_;
  uint16_t timeout_{5000};
  std::list<Header> headers_;
#ifdef USE_ESP8266
  std::shared_ptr<WiFiClient> wifi_client_;
#ifdef USE_HTTP_UPDATE_ESP8266_HTTPS
  std::shared_ptr<BearSSL::WiFiClientSecure> wifi_client_secure_;
#endif
  std::shared_ptr<WiFiClient> get_wifi_client_();
#endif
};


template<typename... Ts> class HttpUpdateFlashAction : public Action<Ts...> {
 public:
  HttpUpdateFlashAction(HttpUpdateComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, url)
  TEMPLATABLE_VALUE(const char *, method)
  TEMPLATABLE_VALUE(const char *, useragent)
  TEMPLATABLE_VALUE(uint16_t, timeout)

  void add_header(const char *key, TemplatableValue<const char *, Ts...> value) { this->headers_.insert({key, value}); }

  void add_json(const char *key, TemplatableValue<std::string, Ts...> value) { this->json_.insert({key, value}); }

  void set_json(std::function<void(Ts..., JsonObject)> json_func) { this->json_func_ = json_func; }

  void register_response_trigger(HttpUpdateResponseTrigger *trigger) { this->response_triggers_.push_back(trigger); }

  void play(Ts... x) override {
    this->parent_->set_url(this->url_.value(x...));
    this->parent_->set_method(this->method_.value(x...));

    if (this->useragent_.has_value()) {
      this->parent_->set_useragent(this->useragent_.value(x...));
    }
    if (this->timeout_.has_value()) {
      this->parent_->set_timeout_req(this->timeout_.value(x...));
    }
    if (!this->headers_.empty()) {
      std::list<Header> headers;
      for (const auto &item : this->headers_) {
        auto val = item.second;
        Header header;
        header.name = item.first;
        header.value = val.value(x...);
        headers.push_back(header);
      }
      this->parent_->set_headers(headers);
    }
    this->parent_->flash(this->response_triggers_);
    this->parent_->close();
  }

 protected:
  void encode_json_(Ts... x, JsonObject root) {
    for (const auto &item : this->json_) {
      auto val = item.second;
      root[item.first] = val.value(x...);
    }
  }
  void encode_json_func_(Ts... x, JsonObject root) { this->json_func_(x..., root); }
  HttpUpdateComponent *parent_;
  std::map<const char *, TemplatableValue<const char *, Ts...>> headers_{};
  std::map<const char *, TemplatableValue<std::string, Ts...>> json_{};
  std::function<void(Ts..., JsonObject)> json_func_{nullptr};
  std::vector<HttpUpdateResponseTrigger *> response_triggers_;
};

class HttpUpdateResponseTrigger : public Trigger<int> {
 public:
  void process(int status_code) { this->trigger(status_code); }
};



}  
} 

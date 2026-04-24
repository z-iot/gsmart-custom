#include "http_update.h"

#include "ota_backend.h"
#include "ota_backend_arduino_esp32.h"
#include "ota_backend_arduino_esp8266.h"
#include "ota_backend_esp_idf.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/util.h"
#include "esphome/components/md5/md5.h"
#include "esphome/components/network/util.h"

#include <cerrno>
#include <cstdio>

namespace esphome
{
  namespace http_update
  {

    static const char *const TAG = "http_update";

    std::unique_ptr<OTABackend> make_ota_backend()
    {
#ifdef USE_ARDUINO
#ifdef USE_ESP8266
      return make_unique<ArduinoESP8266OTABackend>();
#endif // USE_ESP8266
#ifdef USE_ESP32
      return make_unique<ArduinoESP32OTABackend>();
#endif // USE_ESP32
#endif // USE_ARDUINO
#ifdef USE_ESP_IDF
      return make_unique<IDFOTABackend>();
#endif // USE_ESP_IDF
    }

    void HttpUpdateComponent::setup()
    {
      this->dump_config();
    }

    void HttpUpdateComponent::dump_config()
    {
      ESP_LOGCONFIG(TAG, "Http Updates:");
      ESP_LOGCONFIG(TAG, "  Address: %s", network::get_use_address().c_str());
      ESP_LOGCONFIG(TAG, "  Timeout: %ums", this->timeout_);
      ESP_LOGCONFIG(TAG, "  User-Agent: %s", this->useragent_);
      ESP_LOGCONFIG(TAG, "  Follow Redirects: %d", this->follow_redirects_);
      ESP_LOGCONFIG(TAG, "  Redirect limit: %d", this->redirect_limit_);

      if (this->has_safe_mode_ && this->safe_mode_rtc_value_ > 1 &&
          this->safe_mode_rtc_value_ != esphome::http_update::HttpUpdateComponent::ENTER_SAFE_MODE_MAGIC)
      {
        ESP_LOGW(TAG, "Last Boot was an unhandled reset, will proceed to safe mode in %d restarts",
                 this->safe_mode_num_attempts_ - this->safe_mode_rtc_value_);
      }
    }

    void HttpUpdateComponent::loop()
    {
      this->handle_();

      if (this->has_safe_mode_ && (millis() - this->safe_mode_start_time_) > this->safe_mode_enable_time_)
      {
        this->has_safe_mode_ = false;
        // successful boot, reset counter
        ESP_LOGI(TAG, "Boot seems successful, resetting boot loop counter.");
        this->clean_rtc();
      }
    }

    void HttpUpdateComponent::handle_()
    {

      //   ESP_LOGD(TAG, "Starting OTA Update from %s...", this->client_->getpeername().c_str());
      //   this->status_set_warning();
      // #ifdef USE_HTTPUPDATE_STATE_CALLBACK
      //   this->state_callback_.call(OTA_STARTED, 0.0f, 0);
      // #endif
      //   ESP_LOGV(TAG, "OTA size is %u bytes", ota_size);

      //   while (total < ota_size) {
      //         App.feed_wdt();
      //         delay(1);
      // #ifdef USE_HTTPUPDATE_STATE_CALLBACK
      //       this->state_callback_.call(OTA_IN_PROGRESS, percentage, 0);
      // #endif
      //       App.feed_wdt();
      //       yield();
      //   ESP_LOGI(TAG, "OTA update finished!");
      //   this->status_clear_warning();
      // #ifdef USE_HTTPUPDATE_STATE_CALLBACK
      //   this->state_callback_.call(OTA_COMPLETED, 100.0f, 0);
      // #endif
      //   delay(100);  // NOLINT
      //   App.safe_reboot();

      // error:
      //   this->status_momentary_error("onerror", 5000);
      // #ifdef USE_HTTPUPDATE_STATE_CALLBACK
      //   this->state_callback_.call(OTA_ERROR, 0.0f, static_cast<uint8_t>(error_code));
      // #endif
    }

    float HttpUpdateComponent::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

    void HttpUpdateComponent::set_safe_mode_pending(const bool &pending)
    {
      if (!this->has_safe_mode_)
        return;

      uint32_t current_rtc = this->read_rtc_();

      if (pending && current_rtc != esphome::http_update::HttpUpdateComponent::ENTER_SAFE_MODE_MAGIC)
      {
        ESP_LOGI(TAG, "Device will enter safe mode on next boot.");
        this->write_rtc_(esphome::http_update::HttpUpdateComponent::ENTER_SAFE_MODE_MAGIC);
      }

      if (!pending && current_rtc == esphome::http_update::HttpUpdateComponent::ENTER_SAFE_MODE_MAGIC)
      {
        ESP_LOGI(TAG, "Safe mode pending has been cleared");
        this->clean_rtc();
      }
    }
    bool HttpUpdateComponent::get_safe_mode_pending()
    {
      return this->has_safe_mode_ && this->read_rtc_() == esphome::http_update::HttpUpdateComponent::ENTER_SAFE_MODE_MAGIC;
    }

    bool HttpUpdateComponent::should_enter_safe_mode(uint8_t num_attempts, uint32_t enable_time)
    {
      this->has_safe_mode_ = true;
      this->safe_mode_start_time_ = millis();
      this->safe_mode_enable_time_ = enable_time;
      this->safe_mode_num_attempts_ = num_attempts;
      this->rtc_ = global_preferences->make_preference<uint32_t>(233825507UL, false);
      this->safe_mode_rtc_value_ = this->read_rtc_();

      bool is_manual_safe_mode = this->safe_mode_rtc_value_ == esphome::http_update::HttpUpdateComponent::ENTER_SAFE_MODE_MAGIC;

      if (is_manual_safe_mode)
      {
        ESP_LOGI(TAG, "Safe mode has been entered manually");
      }
      else
      {
        ESP_LOGCONFIG(TAG, "There have been %u suspected unsuccessful boot attempts.", this->safe_mode_rtc_value_);
      }

      if (this->safe_mode_rtc_value_ >= num_attempts || is_manual_safe_mode)
      {
        this->clean_rtc();

        if (!is_manual_safe_mode)
          ESP_LOGE(TAG, "Boot loop detected. Proceeding to safe mode.");

        this->status_set_error();
        this->set_timeout(enable_time, []()
                          {
      ESP_LOGE(TAG, "No HttpUpdate attempt made, restarting.");
      App.reboot(); });

        // Delay here to allow power to stabilise before Wi-Fi/Ethernet is initialised.
        delay(300); // NOLINT
        App.setup();

        ESP_LOGI(TAG, "Waiting for HttpUpdate attempt.");

        return true;
      }
      else
      {
        // increment counter
        this->write_rtc_(this->safe_mode_rtc_value_ + 1);
        return false;
      }
    }
    void HttpUpdateComponent::write_rtc_(uint32_t val)
    {
      this->rtc_.save(&val);
      global_preferences->sync();
    }
    uint32_t HttpUpdateComponent::read_rtc_()
    {
      uint32_t val;
      if (!this->rtc_.load(&val))
        return 0;
      return val;
    }
    void HttpUpdateComponent::clean_rtc() { this->write_rtc_(0); }
    void HttpUpdateComponent::on_safe_shutdown()
    {
      if (this->has_safe_mode_ && this->read_rtc_() != esphome::http_update::HttpUpdateComponent::ENTER_SAFE_MODE_MAGIC)
        this->clean_rtc();
    }

#ifdef USE_HTTPUPDATE_STATE_CALLBACK
    void HttpUpdateComponent::add_on_state_callback(std::function<void(HttpUpdateState, float, uint8_t)> &&callback)
    {
      this->state_callback_.add(std::move(callback));
    }
#endif

    void HttpUpdateComponent::set_url(std::string url)
    {
      this->url_ = std::move(url);
      this->secure_ = this->url_.compare(0, 6, "https:") == 0;

      if (!this->last_url_.empty() && this->url_ != this->last_url_)
      {
        // Close connection if url has been changed
        this->client_.setReuse(false);
        this->client_.end();
      }
      this->client_.setReuse(true);
    }

    void HttpUpdateComponent::flash(const std::vector<HttpUpdateResponseTrigger *> &response_triggers)
    {
      if (!network::is_connected())
      {
        this->client_.end();
        this->status_set_warning();
        ESP_LOGW(TAG, "HTTP update failed; Not connected to network");
        return;
      }

      bool begin_status = false;
      const String url = this->url_.c_str();
#if defined(USE_ESP32) || (defined(USE_ESP8266) && USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 6, 0))
#if defined(USE_ESP32) || USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 7, 0)
      if (this->follow_redirects_)
      {
        this->client_.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
      }
      else
      {
        this->client_.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
      }
#else
      this->client_.setFollowRedirects(this->follow_redirects_);
#endif
      this->client_.setRedirectLimit(this->redirect_limit_);
#endif
#if defined(USE_ESP32)
      begin_status = this->client_.begin(url);
#elif defined(USE_ESP8266)
      begin_status = this->client_.begin(*this->get_wifi_client_(), url);
#endif

      if (!begin_status)
      {
        this->client_.end();
        this->status_set_warning();
        ESP_LOGW(TAG, "HTTP update failed at the begin phase. Please check the configuration");
        return;
      }

      this->client_.setTimeout(this->timeout_);
      if (this->useragent_ != nullptr)
      {
        this->client_.setUserAgent(this->useragent_);
      }
      for (const auto &header : this->headers_)
      {
        this->client_.addHeader(header.name, header.value, false, true);
      }

      this->client_.addHeader("Cache-Control", "no-cache");
      // this->client_.addHeader("x-ESP32-STA-MAC", WiFi.macAddress()); //TODO pre esp8266
      // this->client_.addHeader("x-ESP32-AP-MAC", WiFi.softAPmacAddress()); //TODO pre esp8266
      this->client_.addHeader("x-ESP32-free-space", String(ESP.getFreeSketchSpace()));
      this->client_.addHeader("x-ESP32-sketch-size", String(ESP.getSketchSize()));
      String sketchMD5 = ESP.getSketchMD5();
      if (sketchMD5.length() != 0)
      {
        this->client_.addHeader("x-ESP32-sketch-md5", sketchMD5);
      }
      // Add also a SHA256
      // String sketchSHA256 = getSketchSHA256();
      // if(sketchSHA256.length() != 0) {
      //   this->client_.addHeader("x-ESP32-sketch-sha256", sketchSHA256);
      // }
      this->client_.addHeader("x-ESP32-chip-size", String(ESP.getFlashChipSize()));
      this->client_.addHeader("x-ESP32-sdk-version", ESP.getSdkVersion());

      String fwCode = String(App.get_compilation_time().c_str());
      this->client_.addHeader("x-fwcode", fwver_.c_str());
      this->client_.addHeader("x-CompTime", fwCode);
      this->client_.addHeader("x-fwver", fwver_.c_str());
      

      // if(currentVersion && currentVersion[0] != 0x00) {
      //     this->client_.addHeader("x-ESP32-version", currentVersion);
      // }

      int http_code = this->client_.GET();
      int http_len = this->client_.getSize();

      // if(code <= 0) {
      //     ESP_LOGW(TAG, "HTTP update failed; URL: %s; Error: %s", this->url_.c_str(), HTTPClient::errorToString(http_code).c_str());
      //     _lastError = code;
      //     this->client_.end();
      //     return HTTP_UPDATE_FAILED;
      // }

      // int http_code = this->client_.sendRequest(this->method_, "");
      for (auto *trigger : response_triggers)
        trigger->process(http_code);

      if (http_code < 0)
      {
        ESP_LOGW(TAG, "HTTP update failed; URL: %s; Error: %s", this->url_.c_str(),
                 HTTPClient::errorToString(http_code).c_str());
        this->status_set_warning();
        return;
      }

      if (http_code < 200 || http_code >= 300)
      {
        ESP_LOGW(TAG, "HTTP update failed; URL: %s; Code: %d", this->url_.c_str(), http_code);
        this->status_set_warning();
        return;
      }

      switch (http_code)
      {
      case HTTP_CODE_OK: ///< OK (Start Update)
        if (http_len > 0)
        {
          int sketchFreeSpace = ESP.getFreeSketchSpace();
          if (!sketchFreeSpace)
          {
            ESP_LOGW(TAG, "HTTP update failed; No partition");
            this->status_set_warning();
            return;
          }

          if (http_len > sketchFreeSpace)
          {
            ESP_LOGW(TAG, "HTTP update failed; FreeSketchSpace to low (%d) needed: %d", sketchFreeSpace, http_len);
            this->status_set_warning();
            return;
          }

          // process update ...
        }
        else
        {
          ESP_LOGW(TAG, "HTTP update Content-Length was 0 or wasn't set by Server?!");
          this->status_set_warning();
          return;
        }
        break;
      case HTTP_CODE_NOT_MODIFIED:
        ESP_LOGW(TAG, "HTTP update Not Modified (No updates)");
        this->status_set_warning();
        return;
      case HTTP_CODE_NOT_FOUND:
        ESP_LOGW(TAG, "HTTP update File Not Found");
        this->status_set_warning();
        return;
      case HTTP_CODE_FORBIDDEN:
        ESP_LOGW(TAG, "HTTP update Forbidden");
        this->status_set_warning();
        return;
      default:
        ESP_LOGW(TAG, "HTTP update failed; URL: %s; Wrong Code: %d", this->url_.c_str(), http_code);
        this->status_set_warning();
        return;
      }

      WiFiClient *tcp = this->client_.getStreamPtr();
      delay(100);

      // if (tcp->peek() != 0xE9)
      // {
      //   ESP_LOGW(TAG, "HTTP update failed; Magic header does not start with 0xE9");
      //   this->status_set_warning();
      //   return;
      // }

      if (runUpdate(*tcp, http_len, this->client_.header("x-MD5")))
      {
        this->status_clear_warning();
        ESP_LOGD(TAG, "HTTP update completed; FW update done");
        this->client_.end();
        ESP.restart();
      }
      else
      {
        ESP_LOGW(TAG, "HTTP update failed; FW update failed");
        this->status_set_warning();
        return;
      }

      this->status_clear_warning();
      ESP_LOGD(TAG, "HTTP update completed; URL: %s; Code: %d", this->url_.c_str(), http_code);
    }

#ifdef USE_ESP8266
    std::shared_ptr<WiFiClient> HttpUpdateComponent::get_wifi_client_()
    {
#ifdef USE_HTTP_UPDATE_ESP8266_HTTPS
      if (this->secure_)
      {
        if (this->wifi_client_secure_ == nullptr)
        {
          this->wifi_client_secure_ = std::make_shared<BearSSL::WiFiClientSecure>();
          this->wifi_client_secure_->setInsecure();
          this->wifi_client_secure_->setBufferSizes(512, 512);
        }
        return this->wifi_client_secure_;
      }
#endif

      if (this->wifi_client_ == nullptr)
      {
        this->wifi_client_ = std::make_shared<WiFiClient>();
      }
      return this->wifi_client_;
    }
#endif

    void HttpUpdateComponent::close()
    {
      this->last_url_ = this->url_;
      this->client_.end();
    }

    const char *HttpUpdateComponent::get_string()
    {
#if defined(ESP32)
      // The static variable is here because HTTPClient::getString() returns a String on ESP32,
      // and we need something to keep a buffer alive.
      static String str;
#else
      // However on ESP8266, HTTPClient::getString() returns a String& to a member variable.
      // Leaving this the default so that any new platform either doesn't copy, or encounters a compilation error.
      auto &
#endif
      str = this->client_.getString();
      return str.c_str();
    }

    bool HttpUpdateComponent::runUpdate(Stream &fw_stream, uint32_t ota_size, String md5)
    {
      std::unique_ptr<OTABackend> backend;
      backend = make_ota_backend();

      // StreamString error;

      // if (_cbProgress)
      // {
      //   Update.onProgress(_cbProgress);
      // }

      // if (!Update.begin(size, command, _ledPin, _ledOn))
      // {
      //   _lastError = Update.getError();
      //   Update.printError(error);
      //   error.trim(); // remove line ending
      //   log_e("Update.begin failed! (%s)\n", error.c_str());
      //   return false;
      // }

      // if (_cbProgress)
      // {
      //   _cbProgress(0, size);
      // }

      // if (md5.length())
      // {
      //   if (!Update.setMD5(md5.c_str()))
      //   {
      //     _lastError = HTTP_UE_SERVER_FAULTY_MD5;
      //     log_e("Update.setMD5 failed! (%s)\n", md5.c_str());
      //     return false;
      //   }
      // }

      // // To do: the SHA256 could be checked if the server sends it

      // if (Update.writeStream(in) != size)
      // {
      //   _lastError = Update.getError();
      //   Update.printError(error);
      //   error.trim(); // remove line ending
      //   log_e("Update.writeStream failed! (%s)\n", error.c_str());
      //   return false;
      // }

      // if (_cbProgress)
      // {
      //   _cbProgress(size, size);
      // }

      // if (!Update.end())
      // {
      //   _lastError = Update.getError();
      //   Update.printError(error);
      //   error.trim(); // remove line ending
      //   log_e("Update.end failed! (%s)\n", error.c_str());
      //   return false;
      // }

      // return true;

      OTAResponseTypes error_code = OTA_RESPONSE_ERROR_UNKNOWN;
      size_t total = 0;
      uint8_t buf[1024];
      uint32_t last_progress = 0;
      // char *sbuf = reinterpret_cast<char *>(buf);

      error_code = backend->begin(ota_size);
      if (error_code != OTA_RESPONSE_OK)
        goto error; // NOLINT(cppcoreguidelines-avoid-goto)
      ESP_LOGW(TAG, "Http Update begin. FwSize: %d", ota_size);

      // backend->set_update_md5(md5.c_str());

      while (total < ota_size)
      {
        // TODO: timeout check
        size_t requested = std::min(sizeof(buf), ota_size - total);
        // ssize_t read = fw_stream->read(buf, requested);

        // toRead = 0;
        // timeout_failures = 0;
        // while(!toRead) {
        //     toRead = data.readBytes(_buffer + _bufferLen,  bytesToRead);
        //     if(toRead == 0) {
        //         timeout_failures++;
        //         if (timeout_failures >= 300) {
        //             _abort(UPDATE_ERROR_STREAM);
        //             return written;
        //         }
        //         delay(100);
        //     }
        // }

        ssize_t read = fw_stream.readBytes(buf, requested);
        if (read == 0)
        {
          App.feed_wdt();
          delay(1);
          // TODO timeout!!!!!
          continue;
        }

        error_code = backend->write(buf, read);
        if (error_code != OTA_RESPONSE_OK)
        {
          ESP_LOGW(TAG, "Error writing binary data to flash! Error:%d, BufSize: %d, Total written: %d", error_code, read, total);
          goto error; // NOLINT(cppcoreguidelines-avoid-goto)
        }
        total += read;

        uint32_t now = millis();
        if (now - last_progress > 1000)
        {
          last_progress = now;
          float percentage = (total * 100.0f) / ota_size;
          ESP_LOGD(TAG, "OTA in progress: %0.1f%%", percentage);
#ifdef USE_HTTPUPDATE_STATE_CALLBACK
          this->state_callback_.call(OTA_IN_PROGRESS, percentage, 0);
#endif
          // feed watchdog and give other tasks a chance to run
          App.feed_wdt();
          yield();
        }
      }

      error_code = backend->end();
      if (error_code != OTA_RESPONSE_OK)
      {
        ESP_LOGW(TAG, "Error ending OTA!");
        goto error; // NOLINT(cppcoreguidelines-avoid-goto)
      }

      delay(10);
      ESP_LOGI(TAG, "OTA update finished!");
      this->status_clear_warning();
#ifdef USE_HTTPUPDATE_STATE_CALLBACK
      this->state_callback_.call(OTA_COMPLETED, 100.0f, 0);
#endif
      delay(100); // NOLINT
      App.safe_reboot();

    error:
      if (backend != nullptr)
      {
        backend->abort();
      }

      this->status_momentary_error("onerror", 5000);
#ifdef USE_HTTPUPDATE_STATE_CALLBACK
      this->state_callback_.call(OTA_ERROR, 0.0f, static_cast<uint8_t>(error_code));
#endif

      return false;
    }

  }
}

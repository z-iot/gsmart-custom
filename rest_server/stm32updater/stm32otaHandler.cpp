#include "stm32otaHandler.h"
#include "stm32Updater.h"
#include <StreamString.h>
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/component.h"

namespace esphome {
namespace stm32 {

static const char *const TAG = "stm32otaHandler";

void report_ota_error()
{
  StreamString ss;
  Stm32Updater.printError(ss);
  String progError = Stm32Updater._programmer->_error;
  ESP_LOGW(TAG, "STM OTA Update failed! Error: %s (%s)", ss.c_str(), progError.c_str());
}

void STM32OTARequestHandler::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index,
                                          uint8_t *data, size_t len, bool final)
{
  bool success;
  if (index == 0)
  {
    ESP_LOGI(TAG, "STM OTA Update Start: %s", filename.c_str());
    this->ota_read_length_ = 0;
    if (Stm32Updater.isRunning())
      Stm32Updater.abort();
    success = Stm32Updater.begin(&Serial, UPDATE_SIZE_UNKNOWN);
    if (!success)
    {
      report_ota_error();
      return;
    }
  }
  else if (Stm32Updater.hasError())
  {
    // don't spam logs with errors if something failed at start
    return;
  }

  success = Stm32Updater.write(data, len) == len;
  if (!success)
  {
    report_ota_error();
    return;
  }
  this->ota_read_length_ += len;

  const uint32_t now = millis();
  if (now - this->last_ota_progress_ > 500)
  {
    if (request->contentLength() != 0)
    {
      float percentage = (this->ota_read_length_ * 100.0f) / request->contentLength();
      ESP_LOGD(TAG, "STM OTA in progress: %0.1f%%", percentage);
    }
    else
    {
      ESP_LOGD(TAG, "STM OTA in progress: %u bytes read", this->ota_read_length_);
    }
    this->last_ota_progress_ = now;
  }

  if (final)
  {
    if (Stm32Updater.end(true))
    {
      ESP_LOGI(TAG, "STM OTA update successful!");
      // this->parent_->set_timeout(100, []() { App.safe_reboot(); });
    }
    else
    {
      report_ota_error();
    }
  }
}
void STM32OTARequestHandler::handleRequest(AsyncWebServerRequest *request)
{
  AsyncWebServerResponse *response;
  if (!Stm32Updater.hasError())
  {
    response = request->beginResponse(200, "text/plain", "STM Update Successful!");
  }
  else
  {
    StreamString ss;
    ss.print("STM Update Failed: ");
    Stm32Updater.printError(ss);
    response = request->beginResponse(200, "text/plain", ss);
  }
  response->addHeader("Connection", "close");
  request->send(response);
}

}
}
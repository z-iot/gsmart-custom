#pragma once

// #ifdef USE_ARDUINO

#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/controller.h"
#include "esphome/core/component.h"

// web_server::handle_index_request
//   stream->print(
//           F("<h2>OTA Update STM32</h2><form method=\"POST\" action=\"/updatestm\" enctype=\"multipart/form-data\"><input "
//             "type=\"file\" name=\"update\"><input type=\"submit\" value=\"Update\"></form>"));


namespace esphome
{
  namespace rest_server
  {

    class RestServer : public Component
    {
    public:
      RestServer(web_server_base::WebServerBase *base) : base_(base) {}

      void setup() override
      {
        this->base_->init();
        this->setupServer();
      }

      void setupServer();

      float get_setup_priority() const override
      {
        // After WiFi
        return setup_priority::WIFI - 1.0f;
      }

    protected:
      web_server_base::WebServerBase *base_;
    };

  }
}

// #endif  // USE_ARDUINO

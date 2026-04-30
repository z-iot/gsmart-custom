#pragma once

// Lightweight helper that mirrors the lambda-style `server->on(uri, method, fn)`
// API of the original ESPAsyncWebServer. This keeps the GSmart REST handlers
// independent of fork-level patches in web_server_idf.

#include "esphome/components/web_server_base/web_server_base.h"

#include <functional>
#include <string>
#include <utility>

namespace esphome {
namespace gsmart_server {

class AsyncCallbackWebHandler : public esphome::web_server_idf::AsyncWebHandler {
 public:
  using HandlerFn = std::function<void(esphome::web_server_idf::AsyncWebServerRequest *)>;

  AsyncCallbackWebHandler(std::string uri, http_method method, HandlerFn fn)
      : uri_(std::move(uri)), method_(method), fn_(std::move(fn)) {}

  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
    if (request->method() != this->method_) return false;
    char url_buf[esphome::web_server_idf::AsyncWebServerRequest::URL_BUF_SIZE];
    return request->url_to(url_buf) == this->uri_.c_str();
  }

  void handleRequest(esphome::web_server_idf::AsyncWebServerRequest *request) override { this->fn_(request); }

 protected:
  std::string uri_;
  http_method method_;
  HandlerFn fn_;
};

// Free helper — replaces fork-patched `server->on(uri, method, fn)`.
inline void on(const std::shared_ptr<esphome::web_server_idf::AsyncWebServer> &server, const char *uri,
               http_method method, AsyncCallbackWebHandler::HandlerFn fn) {
  server->addHandler(new AsyncCallbackWebHandler(uri, method, std::move(fn)));
}

}  // namespace gsmart_server
}  // namespace esphome

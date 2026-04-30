#pragma once

// Lightweight helper that mirrors the lambda-style `server->on(uri, method, fn)`
// API of the original ESPAsyncWebServer. This keeps the GSmart REST handlers
// independent of fork-level patches in web_server_idf.

#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/json/json_util.h"

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

// Handler that buffers a POST body and parses it as JSON before calling `fn(req, root)`.
class AsyncJsonPostHandler : public esphome::web_server_idf::AsyncWebHandler {
 public:
  using HandlerFn = std::function<void(esphome::web_server_idf::AsyncWebServerRequest *, JsonObject)>;

  AsyncJsonPostHandler(std::string uri, HandlerFn fn) : uri_(std::move(uri)), fn_(std::move(fn)) {}

  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
    if (request->method() != HTTP_POST) return false;
    char url_buf[esphome::web_server_idf::AsyncWebServerRequest::URL_BUF_SIZE];
    return request->url_to(url_buf) == this->uri_.c_str();
  }

  void handleRequest(esphome::web_server_idf::AsyncWebServerRequest *request) override {
    // Body assembled in handleBody; nothing to do here.
  }

  void handleBody(esphome::web_server_idf::AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index,
                  size_t total) override {
    if (index == 0) this->body_.clear();
    this->body_.append(reinterpret_cast<char *>(data), len);
    if (index + len == total) {
      auto fn = this->fn_;
      esphome::json::parse_json(this->body_, [fn, request](JsonObject root) {
        fn(request, root);
        return true;
      });
    }
  }

 protected:
  std::string uri_;
  HandlerFn fn_;
  std::string body_;
};

inline void on_post_json(const std::shared_ptr<esphome::web_server_idf::AsyncWebServer> &server, const char *uri,
                         AsyncJsonPostHandler::HandlerFn fn) {
  server->addHandler(new AsyncJsonPostHandler(uri, std::move(fn)));
}

}  // namespace gsmart_server
}  // namespace esphome

#include "control_server.h"

#include "WWWData.h"
#include "esphome/components/deck_server/payloads.h"
#include "esphome/components/deck_server/web_helpers.h"
#include "esphome/components/json/json_util.h"

namespace esphome {
namespace control_server {

namespace gs = esphome::deck_server;

namespace {

// GET handler that builds a JSON response via the deck_server payload helper.
void register_json_get(const std::shared_ptr<AsyncWebServer> &server, const char *uri,
                       std::function<void(JsonObject)> builder) {
  gs::on(server, uri, HTTP_GET, [builder](AsyncWebServerRequest *request) {
    std::string data = esphome::json::build_json([&builder](JsonObject root) { builder(root); });
    request->send(200, "application/json", data.c_str());
  });
}

// POST stub: parses no body, just returns `{"xxx":"XXXX"}` like the original
// mobile_api placeholders. Replace once a real writer exists.
void register_post_stub(const std::shared_ptr<AsyncWebServer> &server, const char *uri) {
  gs::on(server, uri, HTTP_POST, [](AsyncWebServerRequest *request) {
    std::string data = esphome::json::build_json([](JsonObject root) { root["xxx"] = "XXXX"; });
    request->send(200, "application/json", data.c_str());
  });
}

// POST handler that hands a parsed JsonObject to a writer helper, then
// responds with the same `{"xxx":"XXXX"}` ack the legacy clients expect.
void register_json_post(const std::shared_ptr<AsyncWebServer> &server, const char *uri,
                        std::function<void(JsonObject)> writer) {
  gs::on_post_json(server, uri, [writer](AsyncWebServerRequest *request, JsonObject root) {
    writer(root);
    std::string data = esphome::json::build_json([](JsonObject ack) { ack["xxx"] = "XXXX"; });
    request->send(200, "application/json", data.c_str());
  });
}

}  // namespace

void ControlServer::setup() {
  this->base_->init();

  std::shared_ptr<AsyncWebServer> server(this->base_->get_server(), [](AsyncWebServer *) {});

  // Static gsmart-deck SPA assets + SPA fallback.
  WWWData::registerRoutes(
      [server](const String &uri, const String &contentType, const uint8_t *content, size_t len) {
        std::function<void(AsyncWebServerRequest *)> requestHandler =
            [contentType, content, len](AsyncWebServerRequest *request) {
              AsyncWebServerResponse *response = request->beginResponse(200, contentType.c_str(), content, len);
              response->addHeader("Content-Encoding", "gzip");
              request->send(response);
            };

        if (uri.equals("/index.html")) {
          // SPA fallback: any unmatched GET serves index.html. OPTIONS get a 200.
          server->onNotFound([requestHandler](AsyncWebServerRequest *request) {
            if (request->method() == HTTP_GET) {
              requestHandler(request);
            } else if (request->method() == HTTP_OPTIONS) {
              request->send(200);
            } else {
              request->send(404);
            }
          });
          gs::on(server, uri.c_str(), HTTP_GET, std::move(requestHandler));
        } else {
          gs::on(server, uri.c_str(), HTTP_GET, std::move(requestHandler));
        }
      });

  // Legacy gsmart-deck REST endpoints. The actual JSON building / storage
  // wiring lives in deck_server::payloads so a future mobile_api can reuse
  // the same helpers under a clean /api/mobile/v1/* contract.
  register_json_get(server, "/inf/system", &gs::payloads::system_info_json);
  register_json_get(server, "/inf/neighborhood", &gs::payloads::neighborhood_json);
  register_json_get(server, "/rest/features", &gs::payloads::features_json);

  register_json_get(server, "/cfg/scheduller", &gs::payloads::scheduller_json);
  register_json_post(server, "/cfg/scheduller", &gs::payloads::scheduller_apply);

  register_json_get(server, "/cfg/config", &gs::payloads::config_data_json);
  register_post_stub(server, "/cfg/config");

  register_json_get(server, "/cfg/device", &gs::payloads::config_device_json);
  register_post_stub(server, "/cfg/device");

  register_json_get(server, "/cfg/mode", &gs::payloads::config_mode_json);
  register_post_stub(server, "/cfg/mode");

  register_json_get(server, "/cfg/treatment", &gs::payloads::config_treatment_json);
  register_post_stub(server, "/cfg/treatment");

  register_json_get(server, "/cfg/security", &gs::payloads::config_security_json);
  register_post_stub(server, "/cfg/security");

  register_json_get(server, "/cfg/consumable", &gs::payloads::config_consumable_json);
  register_post_stub(server, "/cfg/consumable");

  register_json_get(server, "/cfg/connect", &gs::payloads::config_connect_json);
  register_post_stub(server, "/cfg/connect");

  register_json_post(server, "/cfg/neighbor", &gs::payloads::neighbor_apply);

  // /cfg/def returns a hand-written long string, no builder needed.
  gs::on(server, "/cfg/def", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", gs::payloads::config_def_string());
  });
  register_post_stub(server, "/cfg/def");

#if defined(ENABLE_CORS)
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", CORS_ORIGIN);
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Accept, Content-Type, Authorization");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Credentials", "true");
#endif
}

}  // namespace control_server
}  // namespace esphome

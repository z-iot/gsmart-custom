#include "api_adapter_rest.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/deck_server/web_helpers.h"
#include "esphome/components/storage/store.h"

namespace esphome {
namespace api_adapter_rest {

using namespace esphome::web_server_idf;

namespace {
void send_json(AsyncWebServerRequest *request, std::function<void(JsonObject)> builder, int status = 200) {
  std::string data = esphome::json::build_json([&builder](JsonObject root) { builder(root); });
  request->send(status, "application/json", data.c_str());
}

void send_ok(AsyncWebServerRequest *request, std::function<void(JsonObject)> extra = nullptr) {
  send_json(request, [extra](JsonObject root) {
    root["ok"] = true;
    if (extra)
      extra(root);
  });
}
}  // namespace

void ApiAdapterRest::setup() {
  std::shared_ptr<AsyncWebServer> server(this->base_->get_server(), [](AsyncWebServer *) {});
  std::string base_path = "/api/mobile/" + this->core_->get_version_path();

  // GET Info
  deck_server::on(server, (base_path + "/info").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
    send_json(request, [this](JsonObject root) { this->core_->build_info(root); });
  });

  // GET Status
  deck_server::on(server, (base_path + "/status").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
    send_json(request, [this](JsonObject root) { this->core_->build_status(root); });
  });

  // GET Diagnostics
  deck_server::on(server, (base_path + "/diagnostics").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
    send_json(request, [this](JsonObject root) { this->core_->build_diagnostics(root); });
  });

  // GET Consumption
  deck_server::on(server, (base_path + "/consumption").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
    send_json(request, [this](JsonObject root) { this->core_->build_consumption(root); });
  });

  // GET/POST Network
  deck_server::on(server, (base_path + "/network").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
    send_json(request, [this](JsonObject root) { this->core_->build_network(root); });
  });
  deck_server::on_post_json(server, (base_path + "/network").c_str(), [this](AsyncWebServerRequest *request, JsonObject root) {
    bool applied = this->core_->apply_network(root);
    send_ok(request, [applied](JsonObject res) { res["applied"] = applied; });
  });

  // POST Control Mode
  deck_server::on_post_json(server, (base_path + "/control/mode").c_str(), [this](AsyncWebServerRequest *request, JsonObject root) {
    send_json(request, [this, root](JsonObject res) { this->core_->handle_control_mode(root, res); });
  });

  // POST Control Identify
  deck_server::on_post_json(server, (base_path + "/control/identify").c_str(), [this](AsyncWebServerRequest *request, JsonObject root) {
    send_json(request, [this, root](JsonObject res) { this->core_->handle_identify(root, res); });
  });

  // Scheduler
  deck_server::on(server, (base_path + "/scheduler").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
    send_json(request, [this](JsonObject root) { this->core_->build_scheduler(root); });
  });
  deck_server::on_post_json(server, (base_path + "/scheduler").c_str(), [this](AsyncWebServerRequest *request, JsonObject root) {
    bool ok = this->core_->apply_scheduler(root);
    send_ok(request, [ok](JsonObject res) { res["saved"] = ok; });
  });

  // Region
  deck_server::on(server, (base_path + "/region").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
    send_json(request, [this](JsonObject root) { this->core_->build_region(root); });
  });
  deck_server::on_post_json(server, (base_path + "/region").c_str(), [this](AsyncWebServerRequest *request, JsonObject root) {
    bool saved = this->core_->apply_region(root);
    send_ok(request, [saved](JsonObject res) { res["saved"] = saved; });
  });

  // Aliases (for backward compatibility)
  deck_server::on(server, "/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
    send_json(request, [this](JsonObject root) { this->core_->build_status(root); });
  });
  deck_server::on_post_json(server, "/api/config", [this](AsyncWebServerRequest *request, JsonObject root) {
    send_json(request, [this, root](JsonObject res) { this->core_->handle_api_config(root, res); });
  });
  deck_server::on_post_json(server, "/api/manual-control", [this](AsyncWebServerRequest *request, JsonObject root) {
    send_json(request, [this, root](JsonObject res) { this->core_->handle_api_manual_control(root, res); });
  });
}

}  // namespace api_adapter_rest
}  // namespace esphome

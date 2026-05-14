#include "api_adapter_rest.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/web_server_base/web_helpers.h"
#include "esphome/components/storage/store.h"

namespace esphome {
namespace api_adapter_rest {

using namespace esphome::web_server_base;

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

void send_empty_json(AsyncWebServerRequest *request) {
  send_json(request, [](JsonObject root) {});
}
}  // namespace

void ApiAdapterRest::setup() {
  this->base_->init();
  std::shared_ptr<AsyncWebServer> server(this->base_->get_server(), [](AsyncWebServer *) {});
  if (!server) {
    ESP_LOGE("api_adapter_rest", "Web server not initialized, skipping REST API setup");
    return;
  }

  auto register_routes = [this, server](const std::string &base_path) {
    // GET Info
    web_server_base::on(server, (base_path + "/info").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
      send_json(request, [this](JsonObject root) { this->core_->build_info(root); });
    });

    // GET Status
    web_server_base::on(server, (base_path + "/status").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
      send_json(request, [this](JsonObject root) { this->core_->build_status(root); });
    });

#ifdef GSMART_REX_BASIC_REST
    web_server_base::on(server, (base_path + "/diagnostics").c_str(), HTTP_GET,
                        [](AsyncWebServerRequest *request) { send_empty_json(request); });

    web_server_base::on(server, (base_path + "/consumption").c_str(), HTTP_GET,
                        [](AsyncWebServerRequest *request) { send_empty_json(request); });
#else
    // GET Diagnostics
    web_server_base::on(server, (base_path + "/diagnostics").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
      send_json(request, [this](JsonObject root) { this->core_->build_diagnostics(root); });
    });

    // GET Consumption
    web_server_base::on(server, (base_path + "/consumption").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
      send_json(request, [this](JsonObject root) { this->core_->build_consumption(root); });
    });
#endif

    // Network
    web_server_base::on(server, (base_path + "/network").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
      send_json(request, [this](JsonObject root) { this->core_->build_network(root); });
    });
    web_server_base::on_post_json(server, (base_path + "/network").c_str(), [this](AsyncWebServerRequest *request, JsonObject root) {
      bool applied = this->core_->apply_network(root);
      send_ok(request, [applied](JsonObject res) { res["applied"] = applied; });
    });
#ifdef GSMART_REX_BASIC_REST
    web_server_base::on(server, (base_path + "/network/scan").c_str(), HTTP_POST,
                        [](AsyncWebServerRequest *request) { send_empty_json(request); });
#else
    web_server_base::on(server, (base_path + "/network/scan").c_str(), HTTP_POST, [this](AsyncWebServerRequest *request) {
      send_json(request, [this](JsonObject res) { this->core_->build_network_scan(res); });
    });
#endif
    web_server_base::on(server, (base_path + "/network/mqtt").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
      send_json(request, [this](JsonObject root) { this->core_->build_mqtt(root); });
    });

    // Control
    web_server_base::on_post_json(server, (base_path + "/control/mode").c_str(), [this](AsyncWebServerRequest *request, JsonObject root) {
      send_json(request, [this, root](JsonObject res) { this->core_->handle_control_mode(root, res); });
    });
    web_server_base::on_post_json(server, (base_path + "/control/identify").c_str(), [this](AsyncWebServerRequest *request, JsonObject root) {
      send_json(request, [this, root](JsonObject res) { this->core_->handle_identify(root, res); });
    });

#ifdef GSMART_REX_BASIC_REST
    web_server_base::on(server, (base_path + "/scheduler").c_str(), HTTP_GET,
                        [](AsyncWebServerRequest *request) { send_empty_json(request); });
    web_server_base::on_post_json(server, (base_path + "/scheduler").c_str(),
                                  [](AsyncWebServerRequest *request, JsonObject root) { send_empty_json(request); });
    web_server_base::on_post_json(server, (base_path + "/scheduler/state").c_str(),
                                  [](AsyncWebServerRequest *request, JsonObject root) { send_empty_json(request); });
#else
    // Scheduler
    web_server_base::on(server, (base_path + "/scheduler").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
      send_json(request, [this](JsonObject root) { this->core_->build_scheduler(root); });
    });
    web_server_base::on_post_json(server, (base_path + "/scheduler").c_str(), [this](AsyncWebServerRequest *request, JsonObject root) {
      bool ok = this->core_->apply_scheduler(root);
      send_ok(request, [ok](JsonObject res) { res["saved"] = ok; });
    });
    web_server_base::on_post_json(server, (base_path + "/scheduler/state").c_str(), [this](AsyncWebServerRequest *request, JsonObject root) {
      bool ok = this->core_->apply_scheduler_state(root);
      send_ok(request, [ok](JsonObject res) { res["saved"] = ok; });
    });
#endif

    // Region
    web_server_base::on(server, (base_path + "/region").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
      send_json(request, [this](JsonObject root) { this->core_->build_region(root); });
    });
    web_server_base::on_post_json(server, (base_path + "/region").c_str(), [this](AsyncWebServerRequest *request, JsonObject root) {
      bool saved = this->core_->apply_region(root);
      send_ok(request, [saved](JsonObject res) { res["saved"] = saved; });
    });
    web_server_base::on(server, (base_path + "/region/devices").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
      send_json(request, [this](JsonObject root) { this->core_->build_region_devices(root); });
    });
    web_server_base::on_post_json(server, (base_path + "/region/ping").c_str(), [this](AsyncWebServerRequest *request, JsonObject root) {
      send_json(request, [this, root](JsonObject res) { this->core_->handle_region_ping(root, res); });
    });

#ifdef GSMART_REX_BASIC_REST
    web_server_base::on(server, (base_path + "/settings/consumables").c_str(), HTTP_GET,
                        [](AsyncWebServerRequest *request) { send_empty_json(request); });
    web_server_base::on_post_json(server, (base_path + "/settings/consumables").c_str(),
                                  [](AsyncWebServerRequest *request, JsonObject root) { send_empty_json(request); });

    web_server_base::on(server, (base_path + "/settings/modes").c_str(), HTTP_GET,
                        [](AsyncWebServerRequest *request) { send_empty_json(request); });
    web_server_base::on_post_json(server, (base_path + "/settings/modes").c_str(),
                                  [](AsyncWebServerRequest *request, JsonObject root) { send_empty_json(request); });
#else
    // Settings: Consumables (lamp hours, power, etc.)
    web_server_base::on(server, (base_path + "/settings/consumables").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
      send_json(request, [this](JsonObject root) { this->core_->build_settings_consumables(root); });
    });
    web_server_base::on_post_json(server, (base_path + "/settings/consumables").c_str(), [this](AsyncWebServerRequest *request, JsonObject root) {
      bool saved = this->core_->apply_settings_consumables(root);
      send_ok(request, [saved](JsonObject res) { res["saved"] = saved; });
    });

    // Settings: Modes (min/std/max timing config)
    web_server_base::on(server, (base_path + "/settings/modes").c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
      send_json(request, [this](JsonObject root) { this->core_->build_settings_modes(root); });
    });
    web_server_base::on_post_json(server, (base_path + "/settings/modes").c_str(), [this](AsyncWebServerRequest *request, JsonObject root) {
      bool saved = this->core_->apply_settings_modes(root);
      send_ok(request, [saved](JsonObject res) { res["saved"] = saved; });
    });
#endif
  };

  register_routes("/api/g-node/" + this->core_->get_version_path());

  // Legacy aliases kept temporarily for existing tools while the app migrates
  // to /api/g-node/v1.
  web_server_base::on(server, "/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
    send_json(request, [this](JsonObject root) { this->core_->build_status(root); });
  });
  web_server_base::on_post_json(server, "/api/config", [this](AsyncWebServerRequest *request, JsonObject root) {
    send_json(request, [this, root](JsonObject res) { this->core_->handle_api_config(root, res); });
  });
  web_server_base::on_post_json(server, "/api/manual-control", [this](AsyncWebServerRequest *request, JsonObject root) {
    send_json(request, [this, root](JsonObject res) { this->core_->handle_api_manual_control(root, res); });
  });
}

}  // namespace api_adapter_rest
}  // namespace esphome

#pragma once

// JSON payload builders and writers shared by every HTTP-facing GSmart
// component. control_server registers the legacy gsmart-deck routes and a
// future mobile_api will register a clean v1 route set on top of the same
// helpers.

#include "esphome/components/json/json_util.h"

namespace esphome {
namespace deck_server {
namespace payloads {

// --- Read helpers (build JSON from storage / runtime). ---

void system_info_json(JsonObject root);
void neighborhood_json(JsonObject root);
void features_json(JsonObject root);
void scheduller_json(JsonObject root);

// Placeholder handlers ported from the original mobile_api stubs.
// They return the same dummy payloads gsmart-deck has been consuming and
// will be replaced once the storage-backed implementations land.
void config_data_json(JsonObject root);
void config_device_json(JsonObject root);
void config_mode_json(JsonObject root);
void config_treatment_json(JsonObject root);
void config_security_json(JsonObject root);
void config_consumable_json(JsonObject root);
void config_connect_json(JsonObject root);
const char *config_def_string();

// --- Write helpers (apply JSON payload to storage). ---

void scheduller_apply(JsonObject root);
void neighbor_apply(JsonObject root);

}  // namespace payloads
}  // namespace deck_server
}  // namespace esphome

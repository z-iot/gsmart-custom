#pragma once

#include "esphome/components/json/json_util.h"

namespace esphome {
namespace api_core_v1 {
namespace payloads {

// --- Read helpers (build JSON from storage / runtime). ---

void system_info_json(JsonObject root);
void neighborhood_json(JsonObject root);
void features_json(JsonObject root);
void scheduller_json(JsonObject root);

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
}  // namespace api_core_v1
}  // namespace esphome

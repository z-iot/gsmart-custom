#include "api_adapter_glink.h"
#if defined(ESP32) || defined(ESP8266)
#include "esphome/components/storage/store.h"
#include "esphome/core/helpers.h"
#include <ctime>
#include <cstdio>
#ifdef ESP32
#include <esp_system.h>
#endif

namespace esphome { namespace api_adapter_glink {
static std::string history_hex(uint64_t value){char out[17];snprintf(out,sizeof(out),"%08x%08x",uint32_t(value>>32),uint32_t(value));return out;}
static const char *history_kind(uint8_t kind){
  switch(kind){case gsmart_history::BOOT:return "device.boot";case gsmart_history::START:return "radiation.started";
    case gsmart_history::STOP:return "radiation.stopped";case gsmart_history::MODE:return "radiation.mode.changed";
    case gsmart_history::ACTION:return "control.action";case gsmart_history::ERROR_START:return "device.error.started";
    case gsmart_history::ERROR_CLEAR:return "device.error.cleared";case gsmart_history::GAP:return "history.gap";
    case gsmart_history::CLOCK:return "device.clock";case gsmart_history::CONFIG:return "configuration.applied";default:return "history.gap";}
}
static const char *history_error(uint16_t code){
  switch(code){case 1:return "sbus_unavailable";case 2:return "fan_a_stalled";case 3:return "fan_b_stalled";
    case 4:return "fan_a_telemetry_stale";case 5:return "fan_b_telemetry_stale";
    case 10:return "journal_unavailable";case 11:return "journal_full";default:return "unclassified";}
}
static uint16_t history_reset_code(){
#ifdef ESP32
  return static_cast<uint16_t>(esp_reset_reason());
#else
  return ESP.getResetInfoPtr()->reason;
#endif
}
static const char *history_mode(storage::RadiationMode mode){
  switch(mode){case storage::RadiationMode::OFF:return "off";case storage::RadiationMode::MIN:return "min";
    case storage::RadiationMode::STD:return "std";case storage::RadiationMode::MAX:return "max";
    case storage::RadiationMode::ON:return "on";default:return "unknown";}
}

void ApiAdapterGLink::history_setup_(){
#ifdef GSMART_FEATURE_FILESYSTEM
#ifdef ESP8266
  // ESP8266 preferences are allocated by offset, not looked up by key. Append
  // the one-time migration marker only at LATE, after all pre-existing settings.
  // Allocating it in Store::setup would shift region and Wi-Fi preference slots.
  if(storage::fileSystem&&!storage::fileSystem->isReady())storage::fileSystem->setup();
  if(storage::fileSystem&&storage::fileSystem->isReady()){
    storage::store->settingsMode->loadFromFile();
    storage::store->settingsDevice->loadFromFile();
  }
#endif
  const auto random64=[](){return (uint64_t(random_uint32())<<32)|random_uint32();};
  history_millis_=millis();history_uptime_=history_millis_;
  const bool ready=history_.begin(random64(),random64());
  storage::store->setOperationalError(10,!ready,"Offline history storage unavailable");
  if(ready)history_boot_=history_record_(gsmart_history::BOOT,storage::store->global->radiation.activeMode);
  storage::store->add_on_radiation_action([this](storage::RadiationMode mode,storage::RadiationCause cause){
    this->history_record_(gsmart_history::ACTION,mode);
  });
  storage::store->add_on_operational_error([this](uint16_t code,bool active){
    this->history_record_(active?gsmart_history::ERROR_START:gsmart_history::ERROR_CLEAR,storage::store->global->radiation.activeMode,code);
    this->history_report_error_(code,active);
  });
  storage::store->add_on_emitter_output([this](bool active){
    this->history_record_(active?gsmart_history::START:gsmart_history::STOP, active?storage::RadiationMode::ON:storage::RadiationMode::OFF);
  });
  const auto &errors=storage::store->global->errors;
  for(uint16_t code=1;code<32;code++)if(errors.observedMask&(1U<<code))
    history_record_((errors.activeMask&(1U<<code))?gsmart_history::ERROR_START:gsmart_history::ERROR_CLEAR,storage::store->global->radiation.activeMode,code);
#endif
}

void ApiAdapterGLink::history_report_error_(uint16_t code,bool active){
  if(!authenticated_)return;
  this->send_frame_("event","device",next_frame_id_("fault"),[&](JsonObject payload){
    payload["kind"]=active?"device.error.started":"device.error.cleared";
    payload["level"]="basic";
    auto body=payload["body"].to<JsonObject>();body["errorCode"]=history_error(code);
  });
}
void ApiAdapterGLink::history_report_errors_(){
  const auto &errors=storage::store->global->errors;
  for(uint16_t code=1;code<32;code++)if(errors.observedMask&(1U<<code))
    history_report_error_(code,errors.activeMask&(1U<<code));
}

bool ApiAdapterGLink::history_record_(uint8_t kind,storage::RadiationMode mode,uint16_t code){
#ifdef GSMART_FEATURE_FILESYSTEM
  if(!history_.healthy())return false;
  const uint32_t now=millis();history_uptime_+=uint32_t(now-history_millis_);history_millis_=now;
  gsmart_history::Record record{};record.kind=kind;record.mode=static_cast<uint8_t>(mode);record.code=code;
  if(kind==gsmart_history::BOOT)record.code=history_reset_code();
#ifdef GSMART_EMITTER
  if(kind==gsmart_history::CLOCK||kind==gsmart_history::CONFIG){
    const auto&r=storage::store->global->radiation;
    record.mode=r.outputKnown?static_cast<uint8_t>(r.outputActive?storage::RadiationMode::ON:storage::RadiationMode::OFF):255;
  }
#endif
  const auto wall=time(nullptr);record.wall_ms=wall>=1577836800?uint64_t(wall)*1000:0;record.uptime_ms=history_uptime_;
#ifdef GSMART_FEATURE_REGION
  record.region=storage::store->region->layout.serial;record.config=storage::store->region->metadata.regionVersion;
#endif
  const auto &cause=storage::store->getLastRadiationCause();record.cause=static_cast<uint8_t>(cause.kind);
  memcpy(record.button,cause.detail,sizeof(record.button));memcpy(record.origin,cause.originSerial,sizeof(record.origin));
  memcpy(record.action,cause.actionId,sizeof(record.action));
  record.button[sizeof(record.button)-1]=0;record.origin[sizeof(record.origin)-1]=0;record.action[sizeof(record.action)-1]=0;
  // A malfunctioning input must not wear flash out. One gap marks a burst and
  // recording resumes with another boundary after the fixed hourly window.
  if(uint32_t(now-history_window_ms_)>=3600000){
    history_window_ms_=now;history_window_records_=0;
  }
  if(uint32_t(now-history_day_ms_)>=86400000){history_day_ms_=now;history_day_records_=0;}
#ifdef ESP8266
  constexpr uint16_t daily_limit=128;
#else
  constexpr uint16_t daily_limit=512;
#endif
  if(history_window_records_>=120||history_day_records_>=daily_limit){
    if(!history_rate_gap_){auto gap=record;gap.kind=gsmart_history::GAP;gap.code=3;history_.push(gap);history_rate_gap_=true;}
    return false;
  }
  if(history_rate_gap_){auto gap=record;gap.kind=gsmart_history::GAP;gap.code=4;if(!history_.push(gap))return false;history_rate_gap_=false;}
  if(history_.push(record)){history_window_records_++;history_day_records_++;return true;}
#endif
  return false;
}

void ApiAdapterGLink::history_poll_(){
#ifdef GSMART_FEATURE_FILESYSTEM
  const uint32_t now=millis();history_uptime_+=uint32_t(now-history_millis_);history_millis_=now;
  if(uint32_t(now-history_poll_ms_)<1000||!storage::store)return;history_poll_ms_=now;
  if(!history_.healthy()){storage::store->setOperationalError(10,true,"Offline history storage unavailable");return;}
  storage::store->setOperationalError(11,history_.blocked()||history_rate_gap_,"Offline history has a recorded gap");
  const auto mode=storage::store->global->radiation.activeMode;
  if(!history_boot_)history_boot_=history_record_(gsmart_history::BOOT,mode);
  if(!history_clock_&&time(nullptr)>=1577836800)history_clock_=history_record_(gsmart_history::CLOCK,mode);
  const auto wall=time(nullptr);
  if(wall>=1577836800){
    const uint64_t wall_ms=uint64_t(wall)*1000;
    if(history_wall_anchor_){
      const auto expected=history_wall_anchor_+(history_uptime_-history_uptime_anchor_);
      const auto delta=wall_ms>expected?wall_ms-expected:expected-wall_ms;
      if(delta>5000){
        // A clock correction must never silently turn into irradiation time.
        history_record_(gsmart_history::GAP,mode,5);
        history_record_(gsmart_history::CLOCK,mode);
      }
    }
    history_wall_anchor_=wall_ms;history_uptime_anchor_=history_uptime_;
  }
#ifdef GSMART_FEATURE_REGION
  const uint64_t region=storage::store->region->layout.serial;const auto version=storage::store->region->metadata.regionVersion;
  if(!history_config_||region!=history_region_||version!=history_version_){
    if(history_record_(gsmart_history::CONFIG,mode)){history_region_=region;history_version_=version;history_config_=true;}
  }
#endif
  // Resume even if no state changes after ACK has made space available.
  if(history_.blocked()||history_rate_gap_)history_record_(gsmart_history::CLOCK,mode);
#endif
}

void ApiAdapterGLink::history_send_(){
#ifdef GSMART_FEATURE_FILESYSTEM
  const auto now=millis();if(uint32_t(now-history_send_ms_)<history_send_delay_)return;history_send_ms_=now;
  history_send_delay_=5000+random_uint32()%3000;
#ifdef ESP8266
  if(ESP.getMaxFreeBlockSize()<6000)return;
  gsmart_history::Record records[2]{};
#else
  gsmart_history::Record records[4]{};
#endif
  const auto count=history_.peek(records,sizeof(records)/sizeof(records[0]));if(!count)return;
  history_sent_boot_=records[0].boot;history_sent_sequence_=records[count-1].sequence;
  const auto generation=history_hex(history_.generation()),boot=history_hex(history_sent_boot_);
  this->send_frame_("event","device",next_frame_id_("history"),[&](JsonObject payload){
    payload["kind"]="device.history.batch";payload["level"]="basic";
    JsonObject body=payload["body"].to<JsonObject>();body["v"]=1;body["generation"]=generation;body["bootId"]=boot;
    JsonArray entries=body["entries"].to<JsonArray>();
    for(size_t i=0;i<count;i++){
      const auto&r=records[i];auto entry=entries.add<JsonObject>();entry["sequence"]=r.sequence;entry["kind"]=history_kind(r.kind);
      entry["uptimeMs"]=r.uptime_ms;if(r.wall_ms)entry["occurredAtMs"]=r.wall_ms;else entry["occurredAtMs"]=nullptr;
      auto b=entry["body"].to<JsonObject>();b["regionId"]=history_hex(r.region);b["configVersion"]=r.config;
      if(r.kind!=gsmart_history::GAP&&r.kind!=gsmart_history::BOOT)b["mode"]=history_mode(static_cast<storage::RadiationMode>(r.mode));
      b["causeKind"]=storage::radiationCauseKindToApi(static_cast<storage::RadiationCauseKind>(r.cause));
      if(r.origin[0])b["originSerial"]=r.origin;if(r.button[0])b["buttonId"]=r.button;if(r.action[0])b["actionId"]=r.action;
      if(r.kind==gsmart_history::ERROR_START||r.kind==gsmart_history::ERROR_CLEAR)b["errorCode"]=history_error(r.code);
      if(r.kind==gsmart_history::GAP)b["reason"]=r.code==5?"clock_adjusted":(r.code==3||r.code==4?"flash_write_rate_limit":"journal_capacity");
      if(r.kind==gsmart_history::BOOT){
#ifdef ESP32
        b["resetPlatform"]="esp32";
#else
        b["resetPlatform"]="esp8266";
#endif
        b["resetReason"]=std::string("reset_")+std::to_string(r.code);
      }
    }
  });
#endif
}

void ApiAdapterGLink::history_ack_(JsonObject body){
#ifdef GSMART_FEATURE_FILESYSTEM
  if(!authenticated_||body["v"].as<int>()!=1||body["generation"].as<std::string>()!=history_hex(history_.generation())||
    body["bootId"].as<std::string>()!=history_hex(history_sent_boot_))return;
  const auto sequence=body["ackSequence"].as<uint32_t>();
  if(!sequence)return;
  // After reboot the server may already know later entries from this boot.
  // Reclaim only the prefix in our last batch and let idempotent replay drain.
  history_.ack(history_sent_boot_,std::min(sequence,history_sent_sequence_));
  history_send_ms_=millis();history_send_delay_=1000+random_uint32()%1000;
#endif
}
}} // namespace
#endif

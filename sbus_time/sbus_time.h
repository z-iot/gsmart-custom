#pragma once

#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/sbus/sbus.h"

namespace esphome
{
  namespace sbus_time
  {

    class SbusTimeComponent : public time::RealTimeClock
    {
    public:
      void setup() override;
      void update() override;
      void dump_config() override;
      float get_setup_priority() const override;
      void read_time();
      void write_time();

      void set_time_id(uint8_t time_id) { this->time_id_ = time_id; }
      void set_sbus_parent(sbus::Sbus *parent) { this->parent_ = parent; }

    protected:
      bool decodeTime(const std::vector<uint8_t> &data, ESPTime *rtc_time);
      std::vector<uint8_t> encodeTime(const ESPTime &rtc_time);

      ESPTime last_sbus_time_;

      sbus::Sbus *parent_;
      uint8_t time_id_{0};
    };

    template <typename... Ts>
    class WriteAction : public Action<Ts...>, public Parented<SbusTimeComponent>
    {
    public:
      void play(Ts... x) override { this->parent_->write_time(); }
    };

    template <typename... Ts>
    class ReadAction : public Action<Ts...>, public Parented<SbusTimeComponent>
    {
    public:
      void play(Ts... x) override { this->parent_->read_time(); }
    };
  } // namespace sbus_time
} // namespace esphome

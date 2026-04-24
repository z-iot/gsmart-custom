#include "sbus_time.h"
#include "esphome/core/log.h"

namespace esphome
{
  namespace sbus_time
  {

    static const char *const TAG = "sbus_time";

    void SbusTimeComponent::setup()
    {
      ESP_LOGCONFIG(TAG, "Setting up SbusTime...");

      this->parent_->register_listener(this->time_id_, [this](const sbus::SbusDatapoint &datapoint)
                                       {
        if (datapoint.type != sbus::SbusDatapointType::RAW)
          return;

        ESPTime rtc_time;
        if (!decodeTime(datapoint.value_raw, &rtc_time))
          return;
        std::string timeStr = rtc_time.strftime("%A, %B %d - %I:%M %p");
        ESP_LOGV(TAG, "Sbus reported time %u is: %s", this->time_id_, timeStr.c_str());

        last_sbus_time_ = rtc_time;
        if (!time::RealTimeClock::utcnow().is_valid())
        {
          ESP_LOGI(TAG, "Time updated from Sbus");
          time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
                                        } });
                                       

      this->parent_->add_on_initialized_callback([this]()
                                                 { this->read_time(); });
      read_time();
    }

    void SbusTimeComponent::update()
    {
      // this->read_time();
    }

    void SbusTimeComponent::dump_config()
    {
      ESP_LOGCONFIG(TAG, "SbusTime:");
      ESP_LOGCONFIG(TAG, "  Timezone: '%s'", this->timezone_.c_str());
    }

    float SbusTimeComponent::get_setup_priority() const { return setup_priority::DATA; }

    bool SbusTimeComponent::decodeTime(const std::vector<uint8_t> &data, ESPTime *rtc_time)
    {
      if (data.size() != 6)
        return false;
      rtc_time->year = 2000 + data[0];
      rtc_time->month = data[1];
      rtc_time->day_of_month = data[2];
      rtc_time->day_of_week = 1;
      rtc_time->day_of_year = 1;
      rtc_time->hour = data[3];
      rtc_time->minute = data[4];
      rtc_time->second = data[5];

      rtc_time->recalc_timestamp_utc(false);
      if (!rtc_time->is_valid())
      {
        ESP_LOGE(TAG, "Invalid RTC time, from Sbus.");
        return false;
      }
      return true;
    }

    std::vector<uint8_t> SbusTimeComponent::encodeTime(const ESPTime &rtc_time)
    {
      std::vector<uint8_t> data;
      data.push_back(rtc_time.year - 2000);
      data.push_back(rtc_time.month);
      data.push_back(rtc_time.day_of_month);
      data.push_back(rtc_time.hour);
      data.push_back(rtc_time.minute);
      data.push_back(rtc_time.second);
      return data;
    }

    void SbusTimeComponent::read_time()
    {
      this->parent_->query_datapoint(this->time_id_);
    }

    void SbusTimeComponent::write_time()
    {
      auto now = time::RealTimeClock::utcnow();
      if (!now.is_valid())
      {
        ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
        return;
      }

      this->parent_->set_raw_datapoint_value(this->time_id_, encodeTime(now));
    }

  } // namespace sbus_time
} // namespace esphome

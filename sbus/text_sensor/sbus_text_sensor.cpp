#include "esphome/core/log.h"
#include "sbus_text_sensor.h"

namespace esphome
{
  namespace sbus
  {

    static const char *const TAG = "sbus.text_sensor";

    void SbusTextSensor::setup()
    {
      this->parent_->register_listener(this->sensor_id_, [this](const SbusDatapoint &datapoint)
                                       {
    switch (datapoint.type) {
      case SbusDatapointType::STRING:
        ESP_LOGD(TAG, "MCU reported text sensor %u is: %s", datapoint.id, datapoint.value_string.c_str());
        this->publish_state(datapoint.value_string);
        break;
      case SbusDatapointType::RAW: {
        std::string data = format_hex_pretty(datapoint.value_raw);
        ESP_LOGD(TAG, "MCU reported text sensor %u is: %s", datapoint.id, data.c_str());
        this->publish_state(data);
        break;
      }
      default:
        ESP_LOGW(TAG, "Unsupported data type for sbus text sensor %u: %#02hhX", datapoint.id, static_cast<uint8_t>(datapoint.type));
        break;
    } });
    
    }
    void SbusTextSensor::update()
    {
      this->parent_->query_datapoint(sensor_id_);
    }

    void SbusTextSensor::dump_config()
    {
      ESP_LOGCONFIG(TAG, "Sbus Text Sensor:");
      ESP_LOGCONFIG(TAG, "  Text Sensor has datapoint ID %u", this->sensor_id_);
    }

  } // namespace sbus
} // namespace esphome

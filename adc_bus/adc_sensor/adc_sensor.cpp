#include "esphome/core/log.h"
#include "sbus_sensor.h"

namespace esphome
{
  namespace sbus
  {

    static const char *const TAG = "sbus.sensor";

    void SbusSensor::setup()
    {
      this->parent_->register_listener(this->sensor_id_, [this](const SbusDatapoint &datapoint)
                                       {
    if (datapoint.type == SbusDatapointType::BOOLEAN) {
      ESP_LOGV(TAG, "MCU reported sensor %u is: %s", datapoint.id, ONOFF(datapoint.value_bool));
      this->publish_state(datapoint.value_bool);
    } else if (datapoint.type == SbusDatapointType::INTEGER) {
      ESP_LOGV(TAG, "MCU reported sensor %u is: %d", datapoint.id, datapoint.value_int);
      this->publish_state(datapoint.value_int);
    } });
    }

    void SbusSensor::update()
    {
      this->parent_->query_datapoint(sensor_id_);
    }

    void SbusSensor::dump_config()
    {
      LOG_SENSOR("", "Sbus Sensor", this);
      ESP_LOGCONFIG(TAG, "  Sensor has datapoint ID %u", this->sensor_id_);
    }

  } // namespace sbus
} // namespace esphome

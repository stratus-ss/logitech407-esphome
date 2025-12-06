#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../z407_controller.h"

namespace esphome {
namespace z407_controller {

/**
 * @brief Sensor for Z407 RSSI (signal strength)
 * 
 * Reports the Bluetooth signal strength in dBm.
 */
class Z407Sensor : public sensor::Sensor, public Component {
 public:
  void set_parent(Z407Controller *parent) { this->parent_ = parent; }
  
  void setup() override {
    if (this->parent_ != nullptr) {
      this->parent_->register_sensor(this);
    }
  }
  
  void loop() override {
    // Update RSSI periodically when connected
    if (this->parent_ != nullptr && this->parent_->is_connected()) {
      uint32_t now = millis();
      if (now - this->last_update_ > this->update_interval_) {
        this->update_rssi_();
        this->last_update_ = now;
      }
    }
  }
  
  void dump_config() override {
    LOG_SENSOR("", "Z407 RSSI", this);
  }
  
  void set_update_interval(uint32_t interval) { this->update_interval_ = interval; }

 protected:
  void update_rssi_() {
    // TODO: RSSI access method not available in current ESPHome BLE client API
    // This feature is temporarily disabled until a compatible method is found
    ESP_LOGD("z407_sensor", "RSSI update requested but not implemented yet");
  }
  
  Z407Controller *parent_{nullptr};
  uint32_t last_update_{0};
  uint32_t update_interval_{60000};  // Default 60 seconds
};

}  // namespace z407_controller
}  // namespace esphome


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
    // Get RSSI from BLE client
    if (this->parent_->parent() != nullptr) {
      int rssi = this->parent_->parent()->get_rssi();
      if (rssi != 0) {
        this->publish_state(rssi);
      }
    }
  }
  
  Z407Controller *parent_{nullptr};
  uint32_t last_update_{0};
  uint32_t update_interval_{60000};  // Default 60 seconds
};

}  // namespace z407_controller
}  // namespace esphome


#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../z407_controller.h"

namespace esphome {
namespace z407_controller {

/**
 * @brief Text sensor for Z407 discovery results
 * 
 * In discovery mode, reports the MAC address of discovered Z407 devices.
 */
class Z407TextSensor : public text_sensor::TextSensor, public Component {
 public:
  void set_parent(Z407Controller *parent) { this->parent_ = parent; }
  
  void setup() override {
    if (this->parent_ != nullptr) {
      this->parent_->register_text_sensor(this);
    }
  }
  
  void dump_config() override {
    LOG_TEXT_SENSOR("", "Z407 Discovery", this);
  }
  
  void publish_discovery(const std::string &address) {
    this->publish_state(address);
  }

 protected:
  Z407Controller *parent_{nullptr};
};

}  // namespace z407_controller
}  // namespace esphome


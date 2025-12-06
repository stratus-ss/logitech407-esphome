#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../z407_controller.h"

namespace esphome {
namespace z407_controller {

/**
 * @brief Binary sensor for Z407 connection status
 * 
 * Reports whether the Z407 is connected and ready for commands.
 */
class Z407BinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void set_parent(Z407Controller *parent) { this->parent_ = parent; }
  
  void setup() override {
    if (this->parent_ != nullptr) {
      this->parent_->register_binary_sensor(this);
      
      // Subscribe to state changes
      this->parent_->add_on_state_callback([this](Z407State state) {
        this->update_from_parent_(state);
      });
      
      // Set initial state
      this->update_from_parent_(this->parent_->get_state());
    }
  }
  
  void dump_config() override {
    LOG_BINARY_SENSOR("", "Z407 Connection", this);
  }

 protected:
  void update_from_parent_(Z407State state) {
    bool connected = (state == Z407State::CONNECTED);
    if (this->state != connected) {
      this->publish_state(connected);
    }
  }
  
  Z407Controller *parent_{nullptr};
};

}  // namespace z407_controller
}  // namespace esphome


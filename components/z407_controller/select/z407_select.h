#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "../z407_controller.h"

namespace esphome {
namespace z407_controller {

/**
 * @brief Select component for Z407 input source
 * 
 * Allows selection of input source (Bluetooth, AUX, USB) and reflects
 * the current input when changed via other means (physical remote, etc.)
 */
class Z407Select : public select::Select, public Component {
 public:
  void set_parent(Z407Controller *parent) { this->parent_ = parent; }
  
  void setup() override {
    if (this->parent_ != nullptr) {
      this->parent_->register_select(this);
      
      // Subscribe to input changes
      this->parent_->add_on_input_callback([this](Z407Input input) {
        this->update_from_parent_(input);
      });
      
      // Set initial state
      this->update_from_parent_(this->parent_->get_input());
    }
  }
  
  void dump_config() override {
    LOG_SELECT("", "Z407 Input Select", this);
  }

 protected:
  void control(const std::string &value) override {
    if (this->parent_ == nullptr) {
      ESP_LOGW("z407_select", "Parent not set");
      return;
    }
    
    if (!this->parent_->is_ready()) {
      ESP_LOGW("z407_select", "Z407 not ready - cannot change input");
      return;
    }
    
    Z407Input input;
    if (value == "Bluetooth") {
      input = Z407Input::BLUETOOTH;
    } else if (value == "AUX") {
      input = Z407Input::AUX;
    } else if (value == "USB") {
      input = Z407Input::USB;
    } else {
      ESP_LOGW("z407_select", "Unknown input: %s", value.c_str());
      return;
    }
    
    if (this->parent_->set_input(input)) {
      // Optimistically update state
      this->publish_state(value);
    }
  }
  
  void update_from_parent_(Z407Input input) {
    std::string value = z407_input_to_string(input);
    if (this->current_option() != value) {
      this->publish_state(value);
    }
  }
  
  Z407Controller *parent_{nullptr};
};

}  // namespace z407_controller
}  // namespace esphome


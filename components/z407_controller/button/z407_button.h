#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "../z407_controller.h"

namespace esphome {
namespace z407_controller {

/**
 * @brief Button component for Z407 commands
 * 
 * Each button triggers a specific Z407 command when pressed.
 */
class Z407Button : public button::Button, public Component {
 public:
  void set_parent(Z407Controller *parent) { this->parent_ = parent; }
  void set_command(Z407Command cmd) { this->command_ = cmd; }
  
  void setup() override {
    if (this->parent_ != nullptr) {
      this->parent_->register_button(this);
    }
  }
  
  void dump_config() override {
    LOG_BUTTON("", "Z407 Button", this);
  }

 protected:
  void press_action() override {
    if (this->parent_ == nullptr) {
      ESP_LOGW("z407_button", "Parent not set");
      return;
    }
    
    if (!this->parent_->is_ready()) {
      ESP_LOGW("z407_button", "Z407 not ready - cannot send command");
      return;
    }
    
    this->parent_->send_command(this->command_);
  }
  
  Z407Controller *parent_{nullptr};
  Z407Command command_{Z407Command::VOLUME_UP};
};

}  // namespace z407_controller
}  // namespace esphome


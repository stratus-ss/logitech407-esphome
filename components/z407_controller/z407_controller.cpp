#include "z407_controller.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace z407_controller {

void Z407Controller::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Z407 Controller...");
  
  if (this->discovery_mode_) {
    ESP_LOGI(TAG, "Discovery mode enabled - will scan for Z407 devices");
    this->start_discovery_();
  } else {
    if (this->client_ == nullptr) {
      ESP_LOGE(TAG, "BLE client not set! Component will not work.");
      this->mark_failed();
      return;
    }
    ESP_LOGCONFIG(TAG, "Normal operation mode - MAC address: %s", 
                  this->client_->address_str());
  }
}

void Z407Controller::loop() {
  // Check for connection timeout during handshake
  if (this->state_ == Z407State::HANDSHAKING) {
    uint32_t now = millis();
    if (now - this->connection_start_time_ > this->connection_timeout_) {
      ESP_LOGW(TAG, "Handshake timeout");
      this->set_state_(Z407State::ERROR);
    }
  }
}

void Z407Controller::dump_config() {
  ESP_LOGCONFIG(TAG, "Z407 Controller:");
  ESP_LOGCONFIG(TAG, "  State: %s", z407_state_to_string(this->state_));
  ESP_LOGCONFIG(TAG, "  Current Input: %s", z407_input_to_string(this->current_input_));
  ESP_LOGCONFIG(TAG, "  Discovery Mode: %s", YESNO(this->discovery_mode_));
  if (!this->discovery_mode_) {
    ESP_LOGCONFIG(TAG, "  MAC Address: %s", this->client_->address_str());
    ESP_LOGCONFIG(TAG, "  Auto Reconnect: %s", YESNO(this->auto_reconnect_));
    ESP_LOGCONFIG(TAG, "  Connection Timeout: %u ms", this->connection_timeout_);
  } else {
    ESP_LOGCONFIG(TAG, "  Discovery Duration: %u ms", this->discovery_duration_);
  }
  ESP_LOGCONFIG(TAG, "  Registered Buttons: %d", this->buttons_.size());
  ESP_LOGCONFIG(TAG, "  Registered Selects: %d", this->selects_.size());
  ESP_LOGCONFIG(TAG, "  Registered Binary Sensors: %d", this->binary_sensors_.size());
  ESP_LOGCONFIG(TAG, "  Registered Sensors: %d", this->sensors_.size());
  ESP_LOGCONFIG(TAG, "  Registered Text Sensors: %d", this->text_sensors_.size());
}

void Z407Controller::gattc_event_handler(esp_gattc_cb_event_t event, 
                                        esp_gatt_if_t gattc_if,
                                        esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "BLE connection established");
        this->set_state_(Z407State::CONNECTING);
        this->connection_start_time_ = millis();
      } else {
        ESP_LOGW(TAG, "BLE connection failed, status=%d", param->open.status);
        this->set_state_(Z407State::ERROR);
      }
      break;
    }
    
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "BLE disconnected");
      this->handshake_step1_complete_ = false;
      this->handshake_step2_complete_ = false;
      this->command_handle_ = 0;
      this->response_handle_ = 0;
      this->set_state_(Z407State::DISCONNECTED);
      this->set_input_(Z407Input::UNKNOWN);
      break;
    }
    
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      ESP_LOGD(TAG, "Service discovery complete");
      // Find our characteristics by UUID
      this->start_handshake_();
      break;
    }
    
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      ESP_LOGI(TAG, "Registered for notifications (status=%d)", param->reg_for_notify.status);
      // Send handshake initiate after registering for notifications
      if (this->state_ == Z407State::HANDSHAKING) {
        ESP_LOGI(TAG, "Sending handshake initiate (0x8405)");
        this->send_raw_command_(0x84, 0x05);
      } else {
        ESP_LOGW(TAG, "Not in HANDSHAKING state, current state: %s", z407_state_to_string(this->state_));
      }
      break;
    }
    
    case ESP_GATTC_NOTIFY_EVT: {
      // Handle notifications from response characteristic
      if (param->notify.handle != this->response_handle_) {
        return;
      }
      
      std::vector<uint8_t> data(param->notify.value, 
                               param->notify.value + param->notify.value_len);
      
      if (data.size() < 3) {
        if (data.size() == 2) {
          ESP_LOGD(TAG, "Short notification (2 bytes): %02X %02X", data[0], data[1]);
        } else {
          ESP_LOGW(TAG, "Short notification len=%d", (int)data.size());
        }
        return;
      }
      
      ESP_LOGD(TAG, "Received notification: %02X %02X %02X", data[0], data[1], data[2]);
      
      if (this->state_ == Z407State::HANDSHAKING) {
        this->handle_handshake_response_(data);
      } else if (this->state_ == Z407State::CONNECTED) {
        // Check if it's a command confirmation or status update
        if (data.size() >= 2) {
          if (data[0] == 0xC0 || data[0] == 0xC1 || 
              data[0] == 0xC2 || data[0] == 0xC3 ||
              data[0] == 0xC5) {
            this->handle_command_confirmation_(data);
          } else if (data[0] == 0xCF) {
            this->handle_status_update_(data);
          }
        }
      }
      break;
    }
    
    default:
      break;
  }
}

void Z407Controller::start_handshake_() {
  ESP_LOGI(TAG, "Starting handshake sequence");
  this->set_state_(Z407State::HANDSHAKING);
  this->handshake_step1_complete_ = false;
  this->handshake_step2_complete_ = false;
  
  // Find characteristics by iterating through services
  auto *service = this->parent()->get_service(esp32_ble::ESPBTUUID::from_raw(SERVICE_UUID));
  if (service == nullptr) {
    ESP_LOGE(TAG, "Service not found!");
    this->set_state_(Z407State::ERROR);
    return;
  }
  
  auto *response_chr = service->get_characteristic(esp32_ble::ESPBTUUID::from_raw(RESPONSE_UUID));
  if (response_chr == nullptr) {
    ESP_LOGE(TAG, "Response characteristic not found!");
    this->set_state_(Z407State::ERROR);
    return;
  }
  
  auto *command_chr = service->get_characteristic(esp32_ble::ESPBTUUID::from_raw(COMMAND_UUID));
  if (command_chr == nullptr) {
    ESP_LOGE(TAG, "Command characteristic not found!");
    this->set_state_(Z407State::ERROR);
    return;
  }
  
  // Store handles for later use
  this->response_handle_ = response_chr->handle;
  this->command_handle_ = command_chr->handle;
  
  ESP_LOGI(TAG, "Found characteristics - Response: 0x%04X, Command: 0x%04X", 
           this->response_handle_, this->command_handle_);
  
  // Enable notifications on response characteristic
  auto status = esp_ble_gattc_register_for_notify(
      this->parent()->get_gattc_if(),
      this->parent()->get_remote_bda(),
      response_chr->handle);
      
  if (status != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register for notifications: %d", status);
    this->set_state_(Z407State::ERROR);
    return;
  }
  
  ESP_LOGI(TAG, "Registered for notifications, waiting for callback...");
}

void Z407Controller::handle_handshake_response_(const std::vector<uint8_t> &data) {
  ESP_LOGD(TAG, "Handshake response: %02X %02X %02X (step1=%d, step2=%d)", 
           data[0], data[1], data[2], this->handshake_step1_complete_, this->handshake_step2_complete_);
  
  // Check for 0xD4 0x05 0x01 (response to initiate)
  if (data[0] == 0xD4 && data[1] == 0x05 && data[2] == 0x01) {
    ESP_LOGI(TAG, "Handshake step 1 complete, sending acknowledge (0x8400)");
    this->handshake_step1_complete_ = true;
    this->send_raw_command_(0x84, 0x00);
  }
  // Check for 0xD4 0x00 0x01 (response to acknowledge)
  else if (data[0] == 0xD4 && data[1] == 0x00 && data[2] == 0x01) {
    ESP_LOGI(TAG, "Handshake step 2 complete");
    this->handshake_step2_complete_ = true;
  }
  // Check for 0xD4 0x00 0x03 (connection established)
  else if (data[0] == 0xD4 && data[1] == 0x00 && data[2] == 0x03) {
    ESP_LOGI(TAG, "Handshake complete - connection established!");
    // Mark step 2 as complete if we skip directly to connection established
    this->handshake_step2_complete_ = true;
    this->set_state_(Z407State::CONNECTED);
  }
  else {
    ESP_LOGW(TAG, "Unexpected handshake response: %02X %02X %02X", 
             data[0], data[1], data[2]);
  }
}

void Z407Controller::handle_command_confirmation_(const std::vector<uint8_t> &data) {
  if (data.size() < 2) return;
  
  uint16_t confirmation = (data[0] << 8) | data[1];
  ESP_LOGD(TAG, "Command confirmed: 0x%04X", confirmation);
  
  // Update input state based on confirmation
  switch (confirmation) {
    case 0xC101:
      this->set_input_(Z407Input::BLUETOOTH);
      break;
    case 0xC102:
      this->set_input_(Z407Input::AUX);
      break;
    case 0xC103:
      this->set_input_(Z407Input::USB);
      break;
    default:
      // Other command confirmations don't change state
      break;
  }
}

void Z407Controller::handle_status_update_(const std::vector<uint8_t> &data) {
  if (data.size() < 2) return;
  
  uint16_t status = (data[0] << 8) | data[1];
  ESP_LOGI(TAG, "Status update: 0x%04X", status);
  
  // Update input state based on status
  switch (status) {
    case 0xCF04:
      ESP_LOGI(TAG, "Input switched to Bluetooth");
      this->set_input_(Z407Input::BLUETOOTH);
      break;
    case 0xCF05:
      ESP_LOGI(TAG, "Input switched to AUX");
      this->set_input_(Z407Input::AUX);
      break;
    case 0xCF06:
      ESP_LOGI(TAG, "Input switched to USB");
      this->set_input_(Z407Input::USB);
      break;
    default:
      ESP_LOGD(TAG, "Unknown status update: 0x%04X", status);
      break;
  }
}

bool Z407Controller::send_command(Z407Command cmd) {
  uint16_t command = static_cast<uint16_t>(cmd);
  uint8_t hi = (command >> 8) & 0xFF;
  uint8_t lo = command & 0xFF;
  
  if (!this->can_send_command_()) {
    ESP_LOGW(TAG, "Cannot send command 0x%04X - not ready (state=%s, handle=0x%04X, hs1=%d, hs2=%d)", 
             command, z407_state_to_string(this->state_), this->command_handle_,
             this->handshake_step1_complete_, this->handshake_step2_complete_);
    return false;
  }
  
  ESP_LOGD(TAG, "Sending command: 0x%04X", command);
  return this->send_raw_command_(hi, lo);
}

bool Z407Controller::set_input(Z407Input input) {
  Z407Command cmd;
  switch (input) {
    case Z407Input::BLUETOOTH:
      cmd = Z407Command::INPUT_BLUETOOTH;
      break;
    case Z407Input::AUX:
      cmd = Z407Command::INPUT_AUX;
      break;
    case Z407Input::USB:
      cmd = Z407Command::INPUT_USB;
      break;
    default:
      ESP_LOGW(TAG, "Invalid input source");
      return false;
  }
  return this->send_command(cmd);
}

bool Z407Controller::send_raw_command_(uint8_t hi, uint8_t lo) {
  if (this->command_handle_ == 0) {
    ESP_LOGW(TAG, "Command characteristic not initialized");
    return false;
  }
  
  // Rate limiting
  uint32_t now = millis();
  if (now - this->last_command_time_ < COMMAND_DELAY_MS) {
    ESP_LOGD(TAG, "Rate limiting - waiting before sending command");
    delay(COMMAND_DELAY_MS - (now - this->last_command_time_));
  }
  
  std::vector<uint8_t> data{hi, lo};
  ESP_LOGD(TAG, "Write command %02X %02X to handle 0x%04X", hi, lo, this->command_handle_);
  
  auto status = esp_ble_gattc_write_char(
      this->parent()->get_gattc_if(),
      this->parent()->get_conn_id(),
      this->command_handle_,
      data.size(),
      data.data(),
      ESP_GATT_WRITE_TYPE_NO_RSP,
      ESP_GATT_AUTH_REQ_NONE);
  
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "Failed to send command: %d", status);
    return false;
  }
  
  this->last_command_time_ = millis();
  return true;
}

bool Z407Controller::can_send_command_() const {
  if (this->state_ != Z407State::CONNECTED) {
    return false;
  }
  if (this->command_handle_ == 0) {
    return false;
  }
  if (!this->handshake_step1_complete_ || !this->handshake_step2_complete_) {
    return false;
  }
  return true;
}

void Z407Controller::set_state_(Z407State state) {
  if (this->state_ != state) {
    ESP_LOGI(TAG, "State changed: %s -> %s", 
             z407_state_to_string(this->state_),
             z407_state_to_string(state));
    this->state_ = state;
    this->state_callbacks_.call(state);
  }
}

void Z407Controller::set_input_(Z407Input input) {
  if (this->current_input_ != input) {
    ESP_LOGI(TAG, "Input changed: %s -> %s",
             z407_input_to_string(this->current_input_),
             z407_input_to_string(input));
    this->current_input_ = input;
    this->input_callbacks_.call(input);
  }
}

void Z407Controller::schedule_reconnect_() {
  // BLE client handles auto-reconnect automatically
  ESP_LOGD(TAG, "Reconnect will be handled by BLE client auto-reconnect");
}

void Z407Controller::attempt_reconnect_() {
  // No longer needed - BLE client handles reconnection
  ESP_LOGD(TAG, "Reconnect is handled by BLE client");
}

void Z407Controller::start_discovery_() {
  ESP_LOGI(TAG, "Starting Z407 discovery for %u seconds", 
           this->discovery_duration_ / 1000);
  
  // Discovery implementation would use esp32_ble_tracker
  // This is a placeholder - actual implementation would scan for SERVICE_UUID
  ESP_LOGW(TAG, "Discovery mode not yet fully implemented");
  ESP_LOGW(TAG, "Please use esp32_ble_tracker to find devices advertising service:");
  ESP_LOGW(TAG, "  %s", SERVICE_UUID);
}

void Z407Controller::handle_discovery_result_(const std::string &address, 
                                              const std::string &name) {
  ESP_LOGI(TAG, "Discovered Z407 device:");
  ESP_LOGI(TAG, "  Name: %s", name.c_str());
  ESP_LOGI(TAG, "  Address: %s", address.c_str());
  
  // Update text sensors with discovery result
  // This would be implemented when text_sensor platform is added
}

// Helper functions
const char *z407_input_to_string(Z407Input input) {
  switch (input) {
    case Z407Input::BLUETOOTH: return "Bluetooth";
    case Z407Input::AUX: return "AUX";
    case Z407Input::USB: return "USB";
    case Z407Input::UNKNOWN: return "Unknown";
    default: return "Invalid";
  }
}

const char *z407_state_to_string(Z407State state) {
  switch (state) {
    case Z407State::DISCONNECTED: return "Disconnected";
    case Z407State::CONNECTING: return "Connecting";
    case Z407State::HANDSHAKING: return "Handshaking";
    case Z407State::CONNECTED: return "Connected";
    case Z407State::ERROR: return "Error";
    default: return "Invalid";
  }
}

}  // namespace z407_controller
}  // namespace esphome

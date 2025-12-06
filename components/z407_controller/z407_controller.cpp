#include "z407_controller.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace z407_controller {

static const char *const TAG = "z407_controller";

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
                  this->client_->address_str().c_str());
  }
}

void Z407Controller::loop() {
  // Handle reconnection attempts
  if (this->auto_reconnect_ && 
      this->state_ == Z407State::DISCONNECTED &&
      this->reconnect_attempts_ < MAX_RECONNECT_ATTEMPTS) {
    uint32_t now = millis();
    if (now - this->reconnect_attempt_time_ > this->reconnect_delay_) {
      this->attempt_reconnect_();
    }
  }
  
  // Check for connection timeout during handshake
  if (this->state_ == Z407State::HANDSHAKING) {
    uint32_t now = millis();
    if (now - this->connection_start_time_ > this->connection_timeout_) {
      ESP_LOGW(TAG, "Handshake timeout - disconnecting");
      this->set_state_(Z407State::ERROR);
      if (this->client_ && this->client_->get_connection_state() == 
          esp32_ble_tracker::ClientState::ESTABLISHED) {
        // Disconnect will trigger reconnect if auto_reconnect is enabled
        this->parent()->set_enabled(false);
        this->parent()->set_enabled(true);
      }
    }
  }
}

void Z407Controller::dump_config() {
  ESP_LOGCONFIG(TAG, "Z407 Controller:");
  ESP_LOGCONFIG(TAG, "  State: %s", z407_state_to_string(this->state_));
  ESP_LOGCONFIG(TAG, "  Current Input: %s", z407_input_to_string(this->current_input_));
  ESP_LOGCONFIG(TAG, "  Discovery Mode: %s", YESNO(this->discovery_mode_));
  if (!this->discovery_mode_) {
    ESP_LOGCONFIG(TAG, "  MAC Address: %s", this->client_->address_str().c_str());
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
        this->reconnect_attempts_ = 0;  // Reset reconnect counter on successful connection
      } else {
        ESP_LOGW(TAG, "BLE connection failed, status=%d", param->open.status);
        this->set_state_(Z407State::ERROR);
        this->schedule_reconnect_();
      }
      break;
    }
    
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "BLE disconnected");
      this->handshake_step1_complete_ = false;
      this->handshake_step2_complete_ = false;
      this->set_state_(Z407State::DISCONNECTED);
      this->set_input_(Z407Input::UNKNOWN);
      this->schedule_reconnect_();
      break;
    }
    
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      ESP_LOGD(TAG, "Service discovery complete");
      // Start handshake after service discovery
      this->start_handshake_();
      break;
    }
    
    case ESP_GATTC_NOTIFY_EVT: {
      // Handle notifications from response characteristic
      std::vector<uint8_t> data(param->notify.value, 
                               param->notify.value + param->notify.value_len);
      
      ESP_LOGD(TAG, "Received notification: %s", 
               format_hex_pretty(data.data(), data.size()).c_str());
      
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
  
  // Enable notifications on response characteristic
  auto *chr = this->parent()->get_characteristic(SERVICE_UUID, RESPONSE_UUID);
  if (chr == nullptr) {
    ESP_LOGE(TAG, "Response characteristic not found!");
    this->set_state_(Z407State::ERROR);
    return;
  }
  
  auto status = esp_ble_gattc_register_for_notify(
      this->parent()->get_gattc_if(),
      this->parent()->get_remote_bda(),
      chr->handle);
      
  if (status != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register for notifications: %d", status);
    this->set_state_(Z407State::ERROR);
    return;
  }
  
  // Send first handshake command: 0x8405
  ESP_LOGD(TAG, "Sending handshake initiate (0x8405)");
  this->send_raw_command_(0x84, 0x05);
}

void Z407Controller::handle_handshake_response_(const std::vector<uint8_t> &data) {
  if (data.size() < 3) {
    ESP_LOGW(TAG, "Handshake response too short: %d bytes", data.size());
    return;
  }
  
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
    ESP_LOGW(TAG, "Cannot send command 0x%04X - not ready", command);
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
  if (this->client_ == nullptr) {
    ESP_LOGW(TAG, "BLE client not set");
    return false;
  }
  
  // Rate limiting
  uint32_t now = millis();
  if (now - this->last_command_time_ < COMMAND_DELAY_MS) {
    ESP_LOGD(TAG, "Rate limiting - waiting before sending command");
    delay(COMMAND_DELAY_MS - (now - this->last_command_time_));
  }
  
  auto *chr = this->parent()->get_characteristic(SERVICE_UUID, COMMAND_UUID);
  if (chr == nullptr) {
    ESP_LOGE(TAG, "Command characteristic not found");
    return false;
  }
  
  std::vector<uint8_t> data{hi, lo};
  auto status = esp_ble_gattc_write_char(
      this->parent()->get_gattc_if(),
      this->parent()->get_conn_id(),
      chr->handle,
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
  if (this->client_ == nullptr) {
    return false;
  }
  if (this->client_->get_connection_state() != 
      esp32_ble_tracker::ClientState::ESTABLISHED) {
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
  if (!this->auto_reconnect_) {
    ESP_LOGD(TAG, "Auto-reconnect disabled");
    return;
  }
  
  if (this->reconnect_attempts_ >= MAX_RECONNECT_ATTEMPTS) {
    ESP_LOGW(TAG, "Max reconnect attempts reached (%d)", MAX_RECONNECT_ATTEMPTS);
    return;
  }
  
  this->reconnect_attempt_time_ = millis();
  this->reconnect_attempts_++;
  
  // Exponential backoff
  this->reconnect_delay_ = 5000 * (1 << std::min(this->reconnect_attempts_, (uint8_t)4));
  
  ESP_LOGI(TAG, "Will attempt reconnect in %u ms (attempt %d/%d)",
           this->reconnect_delay_, this->reconnect_attempts_, MAX_RECONNECT_ATTEMPTS);
}

void Z407Controller::attempt_reconnect_() {
  ESP_LOGI(TAG, "Attempting to reconnect...");
  
  if (this->client_ && this->client_->get_connection_state() == 
      esp32_ble_tracker::ClientState::IDLE) {
    this->parent()->set_enabled(true);
  }
  
  this->reconnect_attempt_time_ = millis();
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


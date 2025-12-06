#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/ble_client/ble_client.h"
#include <vector>
#include <string>

namespace esphome {
namespace z407_controller {

static const char *const TAG = "z407_controller";

// Z407 GATT UUIDs
static const char *SERVICE_UUID = "0000fdc2-0000-1000-8000-00805f9b34fb";
static const char *COMMAND_UUID = "c2e758b9-0e78-41e0-b0cb-98a593193fc5";
static const char *RESPONSE_UUID = "b84ac9c6-29c5-46d4-bba1-9d534784330f";

// Connection states
enum class Z407State {
  DISCONNECTED,
  CONNECTING,
  HANDSHAKING,
  CONNECTED,
  ERROR
};

// Input sources
enum class Z407Input {
  BLUETOOTH,
  AUX,
  USB,
  UNKNOWN
};

// Command definitions
enum class Z407Command : uint16_t {
  BASS_UP = 0x8000,
  BASS_DOWN = 0x8001,
  VOLUME_UP = 0x8002,
  VOLUME_DOWN = 0x8003,
  PLAY_PAUSE = 0x8004,
  NEXT_TRACK = 0x8005,
  PREVIOUS_TRACK = 0x8006,
  INPUT_BLUETOOTH = 0x8101,
  INPUT_AUX = 0x8102,
  INPUT_USB = 0x8103,
  BLUETOOTH_PAIR = 0x8200,
  FACTORY_RESET = 0x8300,
  PAIRING_ACKNOWLEDGE = 0x8400,
  PAIRING_INITIATE = 0x8405,
  SOUND_1 = 0x8501,
  SOUND_2 = 0x8502,
  SOUND_3 = 0x8503,
};

// Forward declarations for platform components
class Z407Button;
class Z407Select;
class Z407BinarySensor;
class Z407Sensor;
class Z407TextSensor;

/**
 * @brief Main controller component for Logitech Z407 speakers
 * 
 * This component manages the BLE connection, handshake sequence, and command
 * execution for the Z407 speakers. It provides callbacks for state changes
 * that platform components can subscribe to.
 */
class Z407Controller : public Component, public ble_client::BLEClientNode {
 public:
  Z407Controller() = default;

  // Component lifecycle
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Configuration setters
  void set_ble_client(ble_client::BLEClient *client) { this->client_ = client; }
  void set_discovery_mode(bool discovery) { this->discovery_mode_ = discovery; }
  void set_discovery_duration(uint32_t duration) { this->discovery_duration_ = duration; }
  void set_connection_timeout(uint32_t timeout) { this->connection_timeout_ = timeout; }
  void set_auto_reconnect(bool reconnect) { this->auto_reconnect_ = reconnect; }

  // BLEClientNode callbacks
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                          esp_ble_gattc_cb_param_t *param) override;

  // Command methods - public API for platform components
  bool send_command(Z407Command cmd);
  bool volume_up() { return this->send_command(Z407Command::VOLUME_UP); }
  bool volume_down() { return this->send_command(Z407Command::VOLUME_DOWN); }
  bool bass_up() { return this->send_command(Z407Command::BASS_UP); }
  bool bass_down() { return this->send_command(Z407Command::BASS_DOWN); }
  bool play_pause() { return this->send_command(Z407Command::PLAY_PAUSE); }
  bool next_track() { return this->send_command(Z407Command::NEXT_TRACK); }
  bool previous_track() { return this->send_command(Z407Command::PREVIOUS_TRACK); }
  bool bluetooth_pair() { return this->send_command(Z407Command::BLUETOOTH_PAIR); }
  bool factory_reset() { return this->send_command(Z407Command::FACTORY_RESET); }
  
  bool set_input(Z407Input input);
  bool set_input_bluetooth() { return this->set_input(Z407Input::BLUETOOTH); }
  bool set_input_aux() { return this->set_input(Z407Input::AUX); }
  bool set_input_usb() { return this->set_input(Z407Input::USB); }

  // State getters
  Z407State get_state() const { return this->state_; }
  Z407Input get_input() const { return this->current_input_; }
  bool is_connected() const { return this->state_ == Z407State::CONNECTED; }
  bool is_ready() const { return this->is_connected(); }

  // Platform component registration
  void register_button(Z407Button *button) { this->buttons_.push_back(button); }
  void register_select(Z407Select *select) { this->selects_.push_back(select); }
  void register_binary_sensor(Z407BinarySensor *sensor) { 
    this->binary_sensors_.push_back(sensor); 
  }
  void register_sensor(Z407Sensor *sensor) { this->sensors_.push_back(sensor); }
  void register_text_sensor(Z407TextSensor *sensor) { 
    this->text_sensors_.push_back(sensor); 
  }

  // State change callbacks
  void add_on_state_callback(std::function<void(Z407State)> &&callback) {
    this->state_callbacks_.add(std::move(callback));
  }
  void add_on_input_callback(std::function<void(Z407Input)> &&callback) {
    this->input_callbacks_.add(std::move(callback));
  }

 protected:
  // BLE connection management
  void start_handshake_();
  void handle_handshake_response_(const std::vector<uint8_t> &data);
  void handle_command_confirmation_(const std::vector<uint8_t> &data);
  void handle_status_update_(const std::vector<uint8_t> &data);
  
  // State management
  void set_state_(Z407State state);
  void set_input_(Z407Input input);
  
  // Discovery mode
  void start_discovery_();
  void handle_discovery_result_(const std::string &address, const std::string &name);
  
  // Command execution
  bool send_raw_command_(uint8_t hi, uint8_t lo);
  bool can_send_command_() const;
  
  // Reconnection logic
  void schedule_reconnect_();
  void attempt_reconnect_();

  // Member variables
  ble_client::BLEClient *client_{nullptr};
  Z407State state_{Z407State::DISCONNECTED};
  Z407Input current_input_{Z407Input::UNKNOWN};
  
  // Configuration
  bool discovery_mode_{false};
  uint32_t discovery_duration_{30000};  // 30 seconds
  uint32_t connection_timeout_{30000};  // 30 seconds
  bool auto_reconnect_{true};
  
  // Timing
  uint32_t last_command_time_{0};
  uint32_t connection_start_time_{0};
  uint32_t reconnect_attempt_time_{0};
  uint32_t reconnect_delay_{5000};  // 5 seconds between reconnect attempts
  uint8_t reconnect_attempts_{0};
  static constexpr uint8_t MAX_RECONNECT_ATTEMPTS = 10;
  static constexpr uint32_t COMMAND_DELAY_MS = 100;  // Min delay between commands
  
  // Handshake state
  bool handshake_step1_complete_{false};
  bool handshake_step2_complete_{false};
  
  // Platform components
  std::vector<Z407Button *> buttons_;
  std::vector<Z407Select *> selects_;
  std::vector<Z407BinarySensor *> binary_sensors_;
  std::vector<Z407Sensor *> sensors_;
  std::vector<Z407TextSensor *> text_sensors_;
  
  // Callbacks
  CallbackManager<void(Z407State)> state_callbacks_;
  CallbackManager<void(Z407Input)> input_callbacks_;
  
  // BLE characteristics
  esp_gatt_char_prop_t command_char_props_{0};
  esp_gatt_char_prop_t response_char_props_{0};
  uint16_t command_handle_{0};
  uint16_t response_handle_{0};
};

// Helper function to convert input enum to string
const char *z407_input_to_string(Z407Input input);

// Helper function to convert state enum to string
const char *z407_state_to_string(Z407State state);

}  // namespace z407_controller
}  // namespace esphome


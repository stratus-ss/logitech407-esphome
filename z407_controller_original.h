#pragma once
#include "esphome.h"

using namespace esphome;

static const char *const TAG_Z407 = "z407_controller";

// Z407 GATT UUIDs (control service / command / response) [web:14]
static const char *SERVICE_UUID   = "0000fdc2-0000-1000-8000-00805f9b34fb";
static const char *COMMAND_UUID   = "c2e758b9-0e78-41e0-b0cb-98a593193fc5";
static const char *RESPONSE_UUID  = "b84ac9c6-29c5-46d4-bba1-9d534784330f";

class Z407Controller : public Component, public ble_client::BLEClientNode {
 public:
  // IDs of the BLE client and output will be injected from YAML
  ble_client::BLEClient *client_{nullptr};
  output::BinaryOutput *cmd_output_{nullptr};

  bool handshake_done_{false};

  void setup() override {
    ESP_LOGI(TAG_Z407, "Z407Controller setup");
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG_Z407, "Z407Controller:");
    LOG_BOOL("  Handshake done", "handshake_done", this->handshake_done_);
  }

  // Called by YAML to associate the BLE client and output
  void set_client(ble_client::BLEClient *client) { this->client_ = client; }
  void set_cmd_output(output::BinaryOutput *out) { this->cmd_output_ = out; }

  // BLEClientNode hook: called when connected
  void on_ble_client_connected() override {
    ESP_LOGI(TAG_Z407, "BLE client connected, starting handshake");
    this->handshake_done_ = false;

    // Subscribe to response notifications
    auto *chr = this->client_->get_characteristic(SERVICE_UUID, RESPONSE_UUID);
    if (chr == nullptr) {
      ESP_LOGE(TAG_Z407, "Response characteristic not found");
      return;
    }
    this->client_->subscribe_for_notifications(chr);
    // Send 0x8405 (Pairing Initiate) [web:14]
    this->send_command_raw_(0x84, 0x05);
  }

  void on_ble_client_disconnected() override {
    ESP_LOGW(TAG_Z407, "BLE client disconnected");
    this->handshake_done_ = false;
  }

  // Called on every notification from the server
  void on_characteristic_notification(esp32_ble_tracker::BLECharacteristic *chr,
                                      const std::vector<uint8_t> &data) override {
    if (chr->get_uuid().to_string() != RESPONSE_UUID)
      return;

    if (data.size() < 3) {
      ESP_LOGW(TAG_Z407, "Short notification len=%d", (int) data.size());
      return;
    }

    ESP_LOGD(TAG_Z407, "Notification: %02X %02X %02X",
             data[0], data[1], data[2]);

    // Handshake responses [web:14]
    if (data[0] == 0xD4 && data[1] == 0x05 && data[2] == 0x01) {
      // Response to 0x8405 -> send 0x8400
      ESP_LOGI(TAG_Z407, "Got 0xD4 05 01 (initiate response), sending 0x8400");
      this->send_command_raw_(0x84, 0x00);
    } else if (data[0] == 0xD4 && data[1] == 0x00 && data[2] == 0x01) {
      ESP_LOGI(TAG_Z407, "Got 0xD4 00 01 (ack), handshake complete");
      this->handshake_done_ = true;
    }
    // You could also parse command confirmations / status here if desired. [web:14]
  }

  // Public API methods to be called from YAML lambdas
  void volume_up()          { this->send_if_ready_(0x80, 0x02); }  // 0x8002 [web:14]
  void volume_down()        { this->send_if_ready_(0x80, 0x03); }  // 0x8003
  void bass_up()            { this->send_if_ready_(0x80, 0x00); }  // 0x8000
  void bass_down()          { this->send_if_ready_(0x80, 0x01); }  // 0x8001
  void play_pause()         { this->send_if_ready_(0x80, 0x04); }  // 0x8004
  void next_track()         { this->send_if_ready_(0x80, 0x05); }  // 0x8005
  void previous_track()     { this->send_if_ready_(0x80, 0x06); }  // 0x8006
  void input_bluetooth()    { this->send_if_ready_(0x81, 0x01); }  // 0x8101
  void input_aux()          { this->send_if_ready_(0x81, 0x02); }  // 0x8102
  void input_usb()          { this->send_if_ready_(0x81, 0x03); }  // 0x8103
  void bluetooth_pair()     { this->send_if_ready_(0x82, 0x00); }  // 0x8200
  void factory_reset()      { this->send_if_ready_(0x83, 0x00); }  // 0x8300
  void sound_1()            { this->send_if_ready_(0x85, 0x01); }  // 0x8501
  void sound_2()            { this->send_if_ready_(0x85, 0x02); }  // 0x8502
  void sound_3()            { this->send_if_ready_(0x85, 0x03); }  // 0x8503

  // Generic raw command (for experimentation)
  void send_raw(uint8_t hi, uint8_t lo) { this->send_if_ready_(hi, lo); }

 protected:
  void send_if_ready_(uint8_t hi, uint8_t lo) {
    if (!this->client_ || !this->client_->is_connected()) {
      ESP_LOGW(TAG_Z407, "Not connected, cannot send command");
      return;
    }
    if (!this->handshake_done_) {
      ESP_LOGW(TAG_Z407, "Handshake not complete, ignoring command %02X%02X", hi, lo);
      return;
    }
    this->send_command_raw_(hi, lo);
  }

  void send_command_raw_(uint8_t hi, uint8_t lo) {
    if (!this->client_) {
      ESP_LOGW(TAG_Z407, "No BLE client set");
      return;
    }
    auto *chr = this->client_->get_characteristic(SERVICE_UUID, COMMAND_UUID);
    if (chr == nullptr) {
      ESP_LOGE(TAG_Z407, "Command characteristic not found");
      return;
    }

    std::vector<uint8_t> data{hi, lo};
    ESP_LOGD(TAG_Z407, "Write command %02X %02X", hi, lo);
    this->client_->write_value(chr, data);
  }
};

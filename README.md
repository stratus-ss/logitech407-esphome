# Z407 ESPHome Controller

Control your Logitech Z407 Bluetooth speakers from Home Assistant using an ESP32 and ESPHome.

## Overview

This custom ESPHome component allows you to control Logitech Z407 speakers via Bluetooth Low Energy (BLE). It provides full control over volume, bass, input selection, and playback functions, integrating seamlessly with Home Assistant.

### Features

- ✅ **Volume Control** - Increase/decrease master volume
- ✅ **Bass Control** - Adjust bass levels independently
- ✅ **Input Switching** - Switch between Bluetooth, AUX, and USB inputs
- ✅ **Playback Control** - Play/pause, next/previous track
- ✅ **Connection Status** - Monitor BLE connection state
- ✅ **Signal Strength** - View RSSI (signal strength)
- ✅ **Auto-Reconnect** - Automatically reconnects after disconnection
- ✅ **Discovery Mode** - Find your Z407's MAC address without external tools
- ✅ **State Tracking** - Tracks current input source
- ✅ **Home Assistant Integration** - Native entities (buttons, select, sensors)

## Hardware Requirements

- **ESP32 Development Board** (ESP32-DevKitC or similar)
- **Logitech Z407 Speakers**
- **USB Power Supply** for ESP32

### Supported ESP32 Boards

- 
- Any ESP32 board with BLE support

**Note:** ESP8266 is **not supported** (no BLE hardware).

## Quick Start

### Step 1: Discovery Mode

First, you need to find your Z407's MAC address:

1. **Prepare the Z407:**
   
   - Remove batteries from the physical Z407 remote (or ensure it's disconnected)
   - Ensure no other device is connected to the Z407
   - The Z407 should be powered on and advertising

2. **Flash discovery configuration:**
   
   ```bash
   esphome run examples/z407_discovery.yaml
   ```

3. **Find the MAC address:**
   
   - Check the logs or Home Assistant for "Discovered Z407 Address"
   - Note down the MAC address (format: `AA:BB:CC:DD:EE:FF`)

### Step 2: Normal Operation

1. **Update configuration:**
   
   - Copy `examples/z407_basic.yaml` to your ESPHome directory
   - Replace `z407_mac` with your discovered MAC address
   - Update WiFi credentials in `secrets.yaml`

2. **Flash the configuration:**
   
   ```bash
   esphome run z407_basic.yaml
   ```

3. **Add to Home Assistant:**
   
   - The device should auto-discover via ESPHome API
   - All entities will appear in Home Assistant

## Installation

### Method 1: External Component (Recommended)

Add to your ESPHome YAML:

```yaml
external_components:
  - source: github://stratus-ss/logitech407-esphome
    components: [z407_controller]
```

### Method 2: Local Component

1. Copy the `components/z407_controller` directory to your ESPHome `custom_components` folder
2. Reference it in your YAML:

```yaml
external_components:
  - source: custom_components
    components: [z407_controller]
```

## Configuration

### Basic Configuration

```yaml
esp32:
  board: esp32dev
  framework:
    type: esp-idf  # Required for stable BLE

esp32_ble_tracker:
  scan_parameters:
    active: true

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"
    id: z407_ble_client

z407_controller:
  id: z407
  ble_client_id: z407_ble_client
```

### Component Options

| Option               | Type    | Default   | Description                         |
| -------------------- | ------- | --------- | ----------------------------------- |
| `id`                 | ID      | Required  | Component ID                        |
| `ble_client_id`      | ID      | Required* | BLE client to use                   |
| `discovery_mode`     | boolean | `false`   | Enable discovery mode               |
| `discovery_duration` | time    | `30s`     | How long to scan in discovery mode  |
| `connection_timeout` | time    | `30s`     | Connection timeout during handshake |
| `auto_reconnect`     | boolean | `true`    | Auto-reconnect on disconnection     |

*Either `ble_client_id` or `discovery_mode` must be set, but not both.

### Platform Components

#### Buttons

All Z407 commands are available as buttons:

```yaml
button:
  - platform: z407_controller
    z407_controller_id: z407
    volume_up:
      name: "Volume Up"
    volume_down:
      name: "Volume Down"
    bass_up:
      name: "Bass Up"
    bass_down:
      name: "Bass Down"
    play_pause:
      name: "Play/Pause"
    next_track:
      name: "Next Track"
    previous_track:
      name: "Previous Track"
    input_bluetooth:
      name: "Switch to Bluetooth"
    input_aux:
      name: "Switch to AUX"
    input_usb:
      name: "Switch to USB"
    bluetooth_pair:
      name: "Bluetooth Pairing Mode"
    factory_reset:
      name: "Factory Reset"
    sound_1:
      name: "Sound 1"
    sound_2:
      name: "Sound 2"
    sound_3:
      name: "Sound 3"
```

#### Select

Input source selector:

```yaml
select:
  - platform: z407_controller
    z407_controller_id: z407
    input_source:
      name: "Input Source"
```

Options: `Bluetooth`, `AUX`, `USB`

#### Binary Sensor

Connection status:

```yaml
binary_sensor:
  - platform: z407_controller
    z407_controller_id: z407
    connection:
      name: "Connected"
      device_class: connectivity
```

#### Sensor

Signal strength (RSSI):

```yaml
sensor:
  - platform: z407_controller
    z407_controller_id: z407
    rssi:
      name: "Signal Strength"
      update_interval: 60s
```

#### Text Sensor

Discovery results (discovery mode only):

```yaml
text_sensor:
  - platform: z407_controller
    z407_controller_id: z407
    discovered_address:
      name: "Discovered Z407 Address"
```

## Protocol Details

### BLE Service

- **Service UUID:** `0000fdc2-0000-1000-8000-00805f9b34fb`
- **Command Characteristic:** `c2e758b9-0e78-41e0-b0cb-98a593193fc5` (write, no response)
- **Response Characteristic:** `b84ac9c6-29c5-46d4-bba1-9d534784330f` (notify)

### Handshake Sequence

1. Connect to BLE device
2. Subscribe to response characteristic
3. Send `0x8405` (Pairing Initiate)
4. Wait for `0xD40501` response
5. Send `0x8400` (Pairing Acknowledge)
6. Wait for `0xD40001` response
7. Wait for `0xD40003` (Connected)
8. Ready for commands

### Command Reference

| Command         | Hex Code | Description                   |
| --------------- | -------- | ----------------------------- |
| Bass Up         | `0x8000` | Increase bass level           |
| Bass Down       | `0x8001` | Decrease bass level           |
| Volume Up       | `0x8002` | Increase master volume        |
| Volume Down     | `0x8003` | Decrease master volume        |
| Play/Pause      | `0x8004` | Toggle playback               |
| Next Track      | `0x8005` | Skip to next track            |
| Previous Track  | `0x8006` | Go to previous track          |
| Input Bluetooth | `0x8101` | Switch to Bluetooth input     |
| Input AUX       | `0x8102` | Switch to 3.5mm AUX input     |
| Input USB       | `0x8103` | Switch to USB input           |
| Bluetooth Pair  | `0x8200` | Enter BT pairing mode         |
| Factory Reset   | `0x8300` | Reset to factory defaults     |
| Sound 1         | `0x8501` | Play failure/disconnect chime |
| Sound 2         | `0x8502` | Play mode switch chime        |
| Sound 3         | `0x8503` | Play connection chime         |

### Response Codes

| Response             | Hex Code   | Meaning               |
| -------------------- | ---------- | --------------------- |
| Initiate Response    | `0xD40501` | Handshake step 1 OK   |
| Acknowledge Response | `0xD40001` | Handshake step 2 OK   |
| Connected            | `0xD40003` | Handshake complete    |
| Command Confirmation | `0xC0XX`   | Command received      |
| Status Update        | `0xCF04`   | Switched to Bluetooth |
| Status Update        | `0xCF05`   | Switched to AUX       |
| Status Update        | `0xCF06`   | Switched to USB       |

## Advanced Usage

### Home Assistant Automations

Example automation to auto-switch input:

```yaml
automation:
  - alias: "Switch Z407 to Bluetooth when phone connects"
    trigger:
      - platform: state
        entity_id: device_tracker.phone
        to: "home"
    action:
      - service: button.press
        target:
          entity_id: button.z407_input_bluetooth
```

## Performance Notes

### BLE and WiFi Coexistence

ESP32 shares the 2.4GHz antenna between BLE and WiFi. For best performance:

1. Use `fast_connect: true` in WiFi config
2. Avoid heavy WiFi traffic during BLE operations
3. Consider disabling WiFi power saving
4. Use `esp-idf` framework (better BLE stack)


### Command Rate Limiting

Commands are rate-limited to 100ms minimum interval to prevent overwhelming the Z407. If you need to send multiple commands, add delays:

```yaml
script:
  - id: multi_command
    then:
      - button.press: volume_up
      - delay: 150ms
      - button.press: bass_up
```

## Credits

This project is based on the reverse engineering work by [freundTech](https://github.com/freundTech/logi-z407-reverse-engineering). Without their protocol documentation, this component would not exist.

## License

This project is licensed under the AGPLv3 License - see the LICENSE file for details.

## Disclaimer

This project is not affiliated, associated, authorized, endorsed by, or in any way officially connected with Logitech International S.A., or any of its subsidiaries or its affiliates.



## Changelog

### Version 1.0.0 (Initial Release)

- Full Z407 protocol implementation
- All commands supported
- Discovery mode
- Auto-reconnect
- State tracking
- Home Assistant integration
- Comprehensive documentation

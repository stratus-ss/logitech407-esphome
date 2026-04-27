# Z407 Controller Code Flow Documentation

This document provides comprehensive code flow diagrams to help you understand how the Z407 ESPHome component works. It covers the entire lifecycle from configuration to runtime operation.

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Configuration to Code Generation Flow](#2-configuration-to-code-generation-flow)
3. [BLE Connection Lifecycle](#3-ble-connection-lifecycle)
4. [Handshake Sequence](#4-handshake-sequence)
5. [Command Execution Flow](#5-command-execution-flow)
6. [State Management](#6-state-management)
7. [Platform Components](#7-platform-components)
8. [Discovery Mode](#8-discovery-mode)
9. [Input Source Switching](#9-input-source-switching)
10. [Error Handling & Recovery](#10-error-handling--recovery)

---

## 1. Architecture Overview

The Z407 Controller is an ESPHome custom component that bridges an ESP32 with Home Assistant to control Logitech Z407 Bluetooth speakers via BLE.

```mermaid
flowchart TB
    HA[Home Assistant]
    ESP[ESP32 Device]
    BLE[BLE Client]
    CTRL[Z407 Controller]
    Z407[Logitech Z407 Speaker]
    
    subgraph ESP["ESP32 Runtime"]
        BLE
        CTRL
        BTN[Button Platform]
        SEL[Select Platform]
        BIN[Binary Sensor Platform]
        SENS[Sensor Platform]
        TXT[Text Sensor Platform]
    end
    
    HA <-->|ESPHome API| ESP
    CTRL <-->|registers| BTN
    CTRL <-->|registers| SEL
    CTRL <-->|registers| BIN
    CTRL <-->|registers| SENS
    CTRL <-->|registers| TXT
    CTRL <-->|BLE GATT| BLE
    BLE <-->|BLE Connection| Z407
    
```

### Key Components

| Component | Purpose |
|-----------|---------|
| `Z407Controller` | Main component - manages BLE connection, handshake, and state |
| `BLEClient` | ESPHome's BLE client component that handles low-level BLE |
| `Button` | Individual button entities for each command |
| `Select` | Input source selector (Bluetooth/AUX/USB) |
| `BinarySensor` | Connection status indicator |
| `Sensor` | RSSI signal strength |
| `TextSensor` | Discovery result display |

---

## 2. Configuration to Code Generation Flow

ESPHome uses Python code generators to create C++ code from YAML configuration. This diagram shows how your `z407_basic.yaml` becomes running firmware.

```mermaid
flowchart LR
    subgraph YAML["YAML Configuration"]
        direction TB
        Y1[esp32_ble_tracker]
        Y2[ble_client]
        Y3[z407_controller]
        Y4[button]
        Y5[select]
        Y6[binary_sensor]
    end
    
    subgraph Python["Python Codegen"]
        direction TB
        P1["__init__.py<br/>z407_controller"]
        P2[button/__init__.py]
        P3[select/__init__.py]
        P4[binary_sensor/__init__.py]
        P5[sensor/__init__.py]
        P6[text_sensor/__init__.py]
    end
    
    subgraph CPP["Generated C++ Code"]
        direction TB
        C1[z407_controller.cpp]
        C2[Z407Controller class]
        C3["Button::press() calls"]
        C4[State callbacks]
    end
    
    YAML -->|esphome compile| Python
    P1 -->|CONFIG_SCHEMA| C1
    P2 -->|button_schema| C2
    P3 -->|select_schema| C3
    P4 -->|binary_sensor_schema| C4
    
```

### Configuration Validation Flow

When you compile, ESPHome validates your YAML against the `CONFIG_SCHEMA` defined in Python:

```mermaid
sequenceDiagram
    participant User as User YAML
    participant CV as Config Validation
    participant Py as Python __init__.py
    
    User->>Py: Submit configuration
    Py->>CV: validate_config()
    Note over CV: Check ble_client_id<br/>OR discovery_mode (not both)
    CV->>Py: Valid config
    Py->>User: Config OK
    
    alt Invalid: Both modes set
        CV-->>User: "Cannot use both 'ble_client_id' and 'discovery_mode'"
    end
    
    alt Invalid: Neither mode set
        CV-->>User: "Must specify either 'ble_client_id' or 'discovery_mode: true'"
    end
```

### Key Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `ble_client_id` | ID | Required* | Reference to ble_client component |
| `discovery_mode` | boolean | false | Enable MAC address discovery |
| `discovery_duration` | time | 30s | How long to scan for device |
| `connection_timeout` | time | 30s | Handshake timeout |
| `auto_reconnect` | boolean | true | Auto-reconnect on disconnect |

*One of `ble_client_id` or `discovery_mode` is required.

---

## 3. BLE Connection Lifecycle

This diagram shows the complete lifecycle of a BLE connection from initial setup to ready state.

```mermaid
stateDiagram-v2
    [*] --> DISCONNECTED
    
    DISCONNECTED --> CONNECTING: BLE OPEN event
    CONNECTING --> HANDSHAKING: Service discovery complete
    HANDSHAKING --> CONNECTED: Handshake complete
    CONNECTED --> DISCONNECTED: BLE DISCONNECT event
    
    DISCONNECTED --> ERROR: Open failed
    CONNECTING --> ERROR: Timeout or failure
    HANDSHAKING --> ERROR: Timeout or invalid response
    
    ERROR --> DISCONNECTED: Auto-reconnect
    ERROR --> [*]
    
    HANDSHAKING --> HANDSHAKING: Timeout check in loop()
```

### BLE Event Handler Flow

The `gattc_event_handler` processes all BLE events from ESP32's BLE stack:

```mermaid
flowchart TD
    EV[BLE GATT Event]
    
    EV -->|ESP_GATTC_OPEN_EVT| OPEN[Handle Connection Open]
    OPEN -->|status == OK| SET_CONNECTING[Set state = CONNECTING]
    OPEN -->|status != OK| SET_ERROR[Set state = ERROR]
    
    EV -->|ESP_GATTC_DISCONNECT_EVT| DISC[Handle Disconnect]
    DISC --> RESET_HANDSHAKE[Reset handshake flags]
    DISC --> CLEAR_HANDLES[Clear characteristic handles]
    DISC --> SET_DISCONNECTED[Set state = DISCONNECTED]
    DISC --> SET_UNKNOWN[Set input = UNKNOWN]
    
    EV -->|ESP_GATTC_SEARCH_CMPL_EVT| SEARCH[Service Discovery Complete]
    SEARCH --> START_HS[Start handshake_]
    
    EV -->|ESP_GATTC_REG_FOR_NOTIFY_EVT| REG[Registered for Notify]
    REG -->|state == HANDSHAKING| SEND_8405[Send 0x8405]
    
    EV -->|ESP_GATTC_NOTIFY_EVT| NOTIFY[Handle Notification]
    NOTIFY --> VALIDATE[Validate data length]
    VALIDATE -->|size < 3| IGNORE[Ignore short notifications]
    VALIDATE -->|size >= 3| CHECK_STATE{Check state}
    
    CHECK_STATE -->|HANDSHAKING| HS_RESP[handle_handshake_response_]
    CHECK_STATE -->|CONNECTED| CMD_RESP{Check response type}
    
    CMD_RESP -->|0xC0-0xC5| CMD_CONF[handle_command_confirmation_]
    CMD_RESP -->|0xCF| STATUS[handle_status_update_]
    
```

### Characteristic Discovery Flow

After BLE connection, the component discovers the Z407's GATT service and characteristics:

```mermaid
flowchart TD
    START[Start handshake_]
    GET_SVC[Get Z407 Service]
    GET_SVC -->|Service UUID| CHECK_SVC{SVC found?}
    
    CHECK_SVC -->|No| LOG_ERR1["Log: Service not found"]
    CHECK_SVC -->|No| SET_ERROR1[Set state = ERROR]
    
    CHECK_SVC -->|Yes| GET_RESP[Get Response Characteristic]
    GET_RESP -->|UUID b84ac9c6...| CHECK_RESP{Found?}
    
    CHECK_RESP -->|No| LOG_ERR2["Log: Response characteristic not found"]
    CHECK_RESP -->|No| SET_ERROR2[Set state = ERROR]
    
    CHECK_RESP -->|Yes| GET_CMD[Get Command Characteristic]
    GET_CMD -->|UUID c2e758b9...| CHECK_CMD{Found?}
    
    CHECK_CMD -->|No| LOG_ERR3["Log: Command characteristic not found"]
    CHECK_CMD -->|No| SET_ERROR3[Set state = ERROR]
    
    CHECK_CMD -->|Yes| STORE[Store handles]
    STORE --> REG[Register for notifications]
    REG --> LOG["Log: Registered for notifications"]
    
```

**Service UUID:** `0000fdc2-0000-1000-8000-00805f9b34fb`
**Command Characteristic:** `c2e758b9-0e78-41e0-b0cb-98a593193fc5` (write)
**Response Characteristic:** `b84ac9c6-29c5-46d4-bba1-9d534784330f` (notify)

---

## 4. Handshake Sequence

The Z407 requires a specific handshake to establish a valid communication session. This is a critical flow.

```mermaid
sequenceDiagram
    participant ESP as ESP32
    participant Z407 as Z407 Speaker
    participant LOG as ESP_LOG
    
    Note over ESP,Z407: HANDSHAKING State
    ESP->>Z407: Write 0x8405 (Pairing Initiate)
    Note over ESP: send_raw_command_(0x84, 0x05)
    LOG->>LOG: "Sending handshake initiate (0x8405)"
    
    Z407-->>ESP: Notify 0xD40501 (Initiate Response)
    Note over ESP: handle_handshake_response_
    LOG->>LOG: "Handshake step 1 complete"
    ESP->>Z407: Write 0x8400 (Pairing Acknowledge)
    Note over ESP: send_raw_command_(0x84, 0x00)
    LOG->>LOG: "Sending acknowledge (0x8400)"
    
    Z407-->>ESP: Notify 0xD40001 (Acknowledge Response)
    Note over ESP: handshake_step2_complete_ = true
    LOG->>LOG: "Handshake step 2 complete"
    
    Note over Z407: Optional step may be skipped
    Z407-->>ESP: Notify 0xD40003 (Connected)
    Note over ESP: set_state_(CONNECTED)
    LOG->>LOG: "Handshake complete!"
    
    Note over ESP,Z407: CONNECTED State - Ready for commands
```

### Handshake Response Handler

```mermaid
flowchart TD
    DATA[Received Notification Data]
    
    DATA --> PARSE["Parse 3 bytes: data[0], data[1], data[2]"]
    PARSE --> CHECK1{data == 0xD40501?}
    
    CHECK1 -->|Yes| LOG1["Log: Step 1 complete"]
    CHECK1 -->|Yes| SET_HS1[handshake_step1_complete_ = true]
    SET_HS1 --> SEND2["send_raw_command_(0x84, 0x00)"]
    
    CHECK1 -->|No| CHECK2{data == 0xD40001?}
    
    CHECK2 -->|Yes| LOG2["Log: Step 2 complete"]
    CHECK2 -->|Yes| SET_HS2[handshake_step2_complete_ = true]
    
    CHECK2 -->|No| CHECK3{data == 0xD40003?}
    
    CHECK3 -->|Yes| LOG3["Log: Handshake complete"]
    CHECK3 -->|Yes| SET_HS2B[handshake_step2_complete_ = true]
    SET_HS2B --> SET_CONN["set_state_(CONNECTED)"]
    
    CHECK3 -->|No| UNKNOWN["Log: Unexpected response"]
    
```

### Command Readiness Check

Before sending any command, the component verifies it's ready:

```mermaid
flowchart TD
    CMD[send_command called]
    CMD --> CHECK1{state == CONNECTED?}
    
    CHECK1 -->|No| REJECT[Return false - not ready]
    CHECK1 -->|Yes| CHECK2{command_handle != 0?}
    
    CHECK2 -->|No| REJECT2[Return false - no handle]
    CHECK2 -->|Yes| CHECK3{handshake_step1_complete?}
    
    CHECK3 -->|No| REJECT3[Return false - step 1 incomplete]
    CHECK3 -->|Yes| CHECK4{handshake_step2_complete?}
    
    CHECK4 -->|No| REJECT4[Return false - step 2 incomplete]
    CHECK4 -->|Yes| ALLOW[Send command]
    
    REJECT --> LOG_ERR["Log: Cannot send command - not ready"]
    REJECT2 --> LOG_ERR2["Log: Command characteristic not initialized"]
    REJECT3 --> LOG_ERR3["Log: Handshake step 1 incomplete"]
    REJECT4 --> LOG_ERR4["Log: Handshake step 2 incomplete"]
    
```

---

## 5. Command Execution Flow

### Button Press to Command

When you press a button in Home Assistant, this flow executes:

```mermaid
sequenceDiagram
    participant HA as Home Assistant
    participant API as ESPHome API
    participant ESP as ESP32 Firmware
    participant BTN as Z407Button
    participant CTRL as Z407Controller
    participant Z407 as Z407 Speaker
    
    HA->>API: button.press entity_id: z407_volume_up
    API->>ESP: Button Press Action
    ESP->>BTN: press()
    BTN->>CTRL: send_command(VOLUME_UP)
    CTRL->>CTRL: can_send_command_()?
    Note over CTRL: Checks: CONNECTED state,<br/>valid handle,<br/>handshake complete
    CTRL->>CTRL: send_raw_command_(0x80, 0x02)
    Note over CTRL: Rate limiting check<br/>100ms minimum between commands
    CTRL->>Z407: Write 0x8002 to Command Characteristic
    
    Z407-->>ESP: Notify 0xC002 (Volume Up Confirmation)
    ESP->>CTRL: handle_command_confirmation_(0xC002)
    Note over CTRL: Logs confirmation
```

### Command Rate Limiting

Commands are rate-limited to prevent overwhelming the Z407:

```mermaid
flowchart TD
    SEND[send_raw_command_ called]
    SEND --> NOW[Get current time]
    NOW --> DIFF{now - last_command_time}
    
    DIFF -->|diff < 100ms| WAIT[delay remaining time]
    WAIT --> UPDATE[Update last_command_time]
    
    DIFF -->|diff >= 100ms| UPDATE2[Update last_command_time]
    
    UPDATE --> GATT[esp_ble_gattc_write_char]
    UPDATE2 --> GATT
    
```

### Command to Hex Mapping

```mermaid
flowchart LR
    subgraph Commands["Z407Command Enum"]
        direction TB
        C1["VOLUME_UP = 0x8002"]
        C2["VOLUME_DOWN = 0x8003"]
        C3["BASS_UP = 0x8000"]
        C4["BASS_DOWN = 0x8001"]
        C5["PLAY_PAUSE = 0x8004"]
        C6["NEXT_TRACK = 0x8005"]
        C7["PREVIOUS_TRACK = 0x8006"]
        C8["INPUT_BLUETOOTH = 0x8101"]
        C9["INPUT_AUX = 0x8102"]
        C10["INPUT_USB = 0x8103"]
        C11["BLUETOOTH_PAIR = 0x8200"]
        C12["FACTORY_RESET = 0x8300"]
        C13["SOUND_1 = 0x8501"]
        C14["SOUND_2 = 0x8502"]
        C15["SOUND_3 = 0x8503"]
    end
    
    subgraph Split["Byte Splitting"]
        direction LR
        HI[hi byte]
        LO[lo byte]
    end
    
    C1 -->|0x8002| Split
    C2 -->|0x8003| Split
    C3 -->|0x8000| Split
    
    Split -->|HI| WRITE1[Write to GATT]
    Split -->|LO| WRITE1
    
    WRITE1 --> BLE["esp_ble_gattc_write_char ESP_GATT_WRITE_TYPE_NO_RSP"]
```

---

## 6. State Management

### State Machine

The component maintains two primary state machines:

```mermaid
stateDiagram-v2
    direction LR
    
    classDef controllerState fill:#bbf,stroke:#333,color:#000
    classDef inputState fill:#9f9,stroke:#333,color:#000
    
    note "Z407State (Connection)" as N1
    note "Z407Input (Source)" as N2
    
    [*] --> DISCONNECTED
    DISCONNECTED --> CONNECTING: Open successful
    CONNECTING --> HANDSHAKING: Service found
    HANDSHAKING --> CONNECTED: Handshake done
    CONNECTED --> DISCONNECTED: Disconnected
    
    DISCONNECTED --> ERROR: Open failed
    HANDSHAKING --> ERROR: Timeout/invalid
    CONNECTING --> ERROR: Timeout
    
    ERROR --> DISCONNECTED: Auto-reconnect
    
    note right of CONNECTED: Ready to send<br/>commands
    
    DISCONNECTED:::controllerState
    CONNECTING:::controllerState
    HANDSHAKING:::controllerState
    CONNECTED:::controllerState
    ERROR:::controllerState
    
    BLUETOOTH:::inputState
    AUX:::inputState
    USB:::inputState
    UNKNOWN:::inputState
```

### State Change Callback Flow

When state changes, registered callbacks are notified:

```mermaid
flowchart TD
    TRIGGER[State Change Triggered]
    TRIGGER --> CURRENT[Get current state]
    TRIGGER --> NEW[Set new state]
    NEW --> LOG["Log: State changed old -> new"]
    LOG --> CALLBACKS[Call state_callbacks_]
    CALLBACKS --> NOTIFY[Notify all registered callbacks]
    NOTIFY --> PLATFORM[Platform components update]
    
    PLATFORM --> BINARY["Binary Sensor: connection"]
    PLATFORM --> HOMEASSISTANT["Home Assistant: entity state"]
    
```

### Registered Platform Components

Platform components register themselves with the controller:

```mermaid
flowchart TB
    subgraph Registration[Registration]
        R1[register_button]
        R2[register_select]
        R3[register_binary_sensor]
        R4[register_sensor]
        R5[register_text_sensor]
    end
    
    subgraph Storage["Member Variables"]
        S1[buttons_ vector]
        S2[selects_ vector]
        S3[binary_sensors_ vector]
        S4[sensors_ vector]
        S5[text_sensors_ vector]
    end
    
    R1 -->|push_back| S1
    R2 -->|push_back| S2
    R3 -->|push_back| S3
    R4 -->|push_back| S4
    R5 -->|push_back| S5
    
```

---

## 7. Platform Components

### Button Platform

Buttons provide individual controls for each Z407 command:

```mermaid
flowchart TD
    subgraph Config["YAML Configuration"]
        BTN_YAML["volume_up:\n  name: 'Z407 Volume Up'"]
    end
    
    subgraph CodeGen["to_code()"]
        CG[Create Z407Button]
        CG --> PARENT[Get parent Z407Controller]
        PARENT --> REGISTER["btn.set_parent(parent)"]
        REGISTER --> CMD["btn.set_command(VOLUME_UP)"]
    end
    
    subgraph Runtime["Button Press"]
        PRESS[button.press]
        PRESS --> CALL[controller.send_command]
        CALL --> VALID{can_send_command_?}
        VALID -->|Yes| GATT[Write to BLE]
        VALID -->|No| LOG_FAIL["Log: not ready"]
    end
    
    Config --> CodeGen
```

### Select Platform (Input Source)

The select platform allows choosing between Bluetooth, AUX, and USB:

```mermaid
flowchart TD
    subgraph Config["YAML Configuration"]
        SEL_YAML["input_source:\n  name: 'Z407 Input'"]
    end
    
    subgraph CodeGen["to_code()"]
        CG[Create Z407Select]
        CG --> PARENT[Get parent Z407Controller]
        PARENT --> REGISTER["sel.set_parent(parent)"]
    end
    
    subgraph Runtime["Select Change"]
        CHANGE[Select new option]
        CHANGE --> MAP{Which input?}
        
        MAP -->|Bluetooth| SET_BT["set_input(BLUETOOTH)"]
        MAP -->|AUX| SET_AUX["set_input(AUX)"]
        MAP -->|USB| SET_USB["set_input(USB)"]
        
        SET_BT --> CMD1[Send INPUT_BLUETOOTH]
        SET_AUX --> CMD2[Send INPUT_AUX]
        SET_USB --> CMD3[Send INPUT_USB]
    end
    
    Config --> CodeGen
```

### Input Selection Mapping

```mermaid
flowchart LR
    subgraph UI["Home Assistant UI"]
        SEL["Select: Input Source<br/>Options: Bluetooth, AUX, USB"]
    end
    
    subgraph Logic["C++ set_input()"]
        CHECK{input == ?}
    end
    
    subgraph Command["Z407Command"]
        CMD_BT["INPUT_BLUETOOTH = 0x8101"]
        CMD_AUX["INPUT_AUX = 0x8102"]
        CMD_USB["INPUT_USB = 0x8103"]
    end
    
    SEL -->|Bluetooth| CHECK
    SEL -->|AUX| CHECK
    SEL -->|USB| CHECK
    
    CHECK -->|BLUETOOTH| CMD_BT
    CHECK -->|AUX| CMD_AUX
    CHECK -->|USB| CMD_USB
```

### Binary Sensor (Connection Status)

```mermaid
flowchart TD
    subgraph Setup["Initialization"]
        INIT[Binary Sensor Setup]
        INIT --> REGISTER[register_binary_sensor]
        REGISTER --> CALLBACK[add_on_state_callback]
    end
    
    subgraph Update["On State Change"]
        CHANGE[State changes to CONNECTED]
        CHANGE --> CALL[Invoke callbacks]
        CALL --> SENSOR[Update binary_sensor state]
        SENSOR --> HA["Home Assistant entity: Available"]
        
        CHANGE2[State changes away from CONNECTED]
        CHANGE2 --> CALL2[Invoke callbacks]
        CALL2 --> SENSOR2[Update binary_sensor state]
        SENSOR2 --> HA2["Home Assistant entity: Unavailable"]
    end
```

---

## 8. Discovery Mode

Discovery mode helps find the Z407's MAC address without external tools:

```mermaid
flowchart TD
    subgraph YAML["Discovery Configuration"]
        D_YAML["discovery_mode: true"]
        D_TIME["discovery_duration: 30s"]
    end
    
    subgraph Setup["setup() execution"]
        SETUP[Component setup]
        SETUP --> CHECK{discovery_mode_?}
        
        CHECK -->|true| START_DISC[Start discovery_]
        CHECK -->|false| NORMAL[Normal BLE client mode]
    end
    
    subgraph Discovery["start_discovery_()"]
        LOG_D["Log: Starting discovery"]
        LOG_D --> LOG_W["Log: Discovery not fully implemented"]
        LOG_W --> HELP["Log: Use esp32_ble_tracker to find service UUID"]
    end
    
    subgraph Alternative["Alternative: esp32_ble_tracker"]
        TRACKER["esp32_ble_tracker<br/>on_ble_advertise"]
        TRACKER --> FILTER[Filter for service UUID 0xFDC2]
        FILTER --> FOUND[Log discovered MAC address]
    end
    
    D_YAML --> Setup
    D_TIME --> Setup
    START_DISC --> Discovery
```

### Discovery Using esp32_ble_tracker

The `z407_discovery.yaml` example uses `esp32_ble_tracker` with a lambda filter:

```mermaid
flowchart TD
    subgraph Scan["BLE Scanning"]
        SCAN[esp32_ble_tracker active scan]
        SCAN --> ADVERTISE[on_ble_advertise event]
    end
    
    subgraph Filter["Lambda Filter"]
        CHECK_SVC[Get service_uuids]
        CHECK_SVC --> LOOP[For each service UUID]
        LOOP --> CMP{UUID == 0xFDC2?}
        
        CMP -->|Match| FOUND_Z407[Z407 Found!]
        CMP -->|No Match| NEXT[Continue scanning]
        
        FOUND_Z407 --> LOG_BOX[Log formatted MAC box]
        LOG_BOX --> STORE[Store z407_mac]
    end
    
```

---

## 9. Input Source Switching

### Input Source Response Handling

When the Z407 changes input source, it sends a notification:

```mermaid
sequenceDiagram
    participant Z407 as Z407 Speaker
    participant CTRL as Z407Controller
    participant LOG as Logs
    
    Note over Z407: User presses input button<br/>on physical remote
    Z407-->>CTRL: Notify 0xCF04/05/06
    
    CTRL->>CTRL: handle_status_update_
    Note over CTRL: data[0] = 0xCF<br/>data[1] = status code
    
    alt data[1] == 0x04
        CTRL->>LOG: "Input switched to Bluetooth"
        CTRL->>CTRL: set_input_(BLUETOOTH)
    else data[1] == 0x05
        CTRL->>LOG: "Input switched to AUX"
        CTRL->>CTRL: set_input_(AUX)
    else data[1] == 0x06
        CTRL->>LOG: "Input switched to USB"
        CTRL->>CTRL: set_input_(USB)
    end
    
    Note over CTRL: input_callbacks_.call(input)
    Note over CTRL: Home Assistant updates<br/>select entity display
```

### Command Confirmation vs Status Update

The Z407 sends two types of responses:

```mermaid
flowchart TD
    RECV[Received Notification]
    RECV --> CHECK1{"data[0] >= 0xC0 AND data[0] <= 0xC5?"}
    
    CHECK1 -->|Yes, Command Confirm| CONFIRM[handle_command_confirmation_]
    CONFIRM --> LOG_C["Log: Command confirmed"]
    CONFIRM --> UPDATE{Update input state?}
    UPDATE -->|0xC101| SET_BT["set_input_(BLUETOOTH)"]
    UPDATE -->|0xC102| SET_AUX["set_input_(AUX)"]
    UPDATE -->|0xC103| SET_USB["set_input_(USB)"]
    UPDATE -->|Other| IGNORE[No state change]
    
    CHECK1 -->|No| CHECK2{"data[0] == 0xCF?"}
    
    CHECK2 -->|Yes, Status Update| STATUS[handle_status_update_]
    STATUS --> LOG_S["Log: Status update"]
    STATUS --> PARSE{Parse status code}
    PARSE -->|0xCF04| SB["set_input_(BLUETOOTH)"]
    PARSE -->|0xCF05| SA["set_input_(AUX)"]
    PARSE -->|0xCF06| SU["set_input_(USB)"]
    
    CHECK1 -->|No| IGNORE2[Unknown, ignore]
    
```

---

## 10. Error Handling & Recovery

### Timeout Handling During Handshake

The `loop()` function monitors handshake timeout:

```mermaid
flowchart TD
    LOOP["loop() called<br/>every iteration"]
    LOOP --> CHECK{hs_state == HANDSHAKING?}
    
    CHECK -->|No| EXIT[Return]
    CHECK -->|Yes| CHECK_TIME{"now - connection_start_time > connection_timeout?"}
    
    CHECK_TIME -->|No| EXIT
    CHECK_TIME -->|Yes| TIMEOUT[Handle timeout]
    TIMEOUT --> LOG_W["Log: Handshake timeout"]
    TIMEOUT --> SET_ERR["set_state_(ERROR)"]
    
```

### Disconnect Handling

```mermaid
flowchart TD
    DISC[ESP_GATTC_DISCONNECT_EVT]
    DISC --> RESET[Reset handshake state]
    RESET --> RESET1[handshake_step1_complete_ = false]
    RESET --> RESET2[handshake_step2_complete_ = false]
    RESET --> RESET3[command_handle_ = 0]
    RESET --> RESET4[response_handle_ = 0]
    
    RESET --> STATE["set_state_(DISCONNECTED)"]
    RESET --> INPUT["set_input_(UNKNOWN)"]
    
    STATE --> RECONNECT{auto_reconnect_?}
    
    RECONNECT -->|Yes| LOG_R["Reconnect handled by BLE client"]
    RECONNECT -->|No| EXIT_D
    
    LOG_R --> EXIT_D[Exit handler]
    
```

### Auto-Reconnect Flow

```mermaid
flowchart TB
    subgraph BLEClient["ESPHome ble_client Component"]
        direction TB
        AUTO["auto_connect: true"]
        AUTO --> LISTEN[Listen for disconnect]
        LISTEN --> WAIT[Wait for Z407 advertisement]
        WAIT --> RECONNECT[Auto-connect]
    end
    
    subgraph Controller["Z407Controller"]
        direction TB
        SCHEDULE[schedule_reconnect_]
        SCHEDULE --> LOG_RC["Log: Reconnect will be handled by BLE client"]
    end
    
    RECONNECT -->|Connection opens| OPEN[ESP_GATTC_OPEN_EVT]
    OPEN -->|success| HANDSHAKE[Start handshake_]
    
```

---

## Quick Reference: Common Debugging Scenarios

### "Cannot send command - not ready" Error

This error means `can_send_command_()` returned false. Check:

1. **State**: Is `state_ == CONNECTED`? Check logs for "State changed: X -> Connected"
2. **Handles**: Is `command_handle_ != 0`? (Should be set after service discovery)
3. **Handshake**: Are both `handshake_step1_complete_` and `handshake_step2_complete_` true?

### Handshake Never Completes

Check for:
1. Z407 already connected to another device (remove remote batteries)
2. Wrong MAC address in configuration
3. Z407 not powered on or not in pairing mode
4. Connection timeout too short (try 60s)

### Unexpected Handshake Responses

The Z407 may send responses in different orders. The handler accepts:
- `0xD40501` → Step 1 response to `0x8405`
- `0xD40001` → Step 2 response to `0x8400` (may be skipped)
- `0xD40003` → Connection established

### Commands Rate Limited

If commands seem delayed, check:
- 100ms minimum between commands (`COMMAND_DELAY_MS`)
- Multiple rapid button presses will queue with delays

---

## File Structure Reference

```
components/z407_controller/
├── __init__.py              # Python config schema, to_code(), actions
├── z407_controller.h        # C++ class declarations, enums
├── z407_controller.cpp     # C++ implementation, BLE event handling
├── button/__init__.py       # Button platform codegen
├── select/__init__.py       # Select platform codegen
├── binary_sensor/__init__.py # Binary sensor platform codegen
├── sensor/__init__.py       # Sensor platform codegen
└── text_sensor/__init__.py # Text sensor platform codegen
```

### Key Files and Their Responsibilities

| File | Responsibility |
|------|---------------|
| `__init__.py` | Configuration validation, code generation entry points |
| `z407_controller.h` | Class definition, enums, constants |
| `z407_controller.cpp` | Runtime logic: BLE events, handshake, commands |
| `button/__init__.py` | Generate button entities from YAML |
| `select/__init__.py` | Generate select entity for input switching |

---

## Understanding the BLE Protocol

For detailed protocol information, see the reverse-engineering documentation:

- [Original Protocol Documentation](../logi-z407-reverse-engineering/doc/Protocol.md)
- [Reverse Engineering Notes](../logi-z407-reverse-engineering/doc/ReverseEngineering.md)

### Service and Characteristic UUIDs

| Type | UUID |
|------|------|
| Service | `0000fdc2-0000-1000-8000-00805f9b34fb` |
| Command Characteristic | `c2e758b9-0e78-41e0-b0cb-98a593193fc5` |
| Response Characteristic | `b84ac9c6-29c5-46d4-bba1-9d534784330f` |

---

*This documentation was generated to help maintain the Z407 ESPHome component. For questions or clarifications, please refer to the codebase or open an issue.*
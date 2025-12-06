# Z407 ESPHome Component - Quick Start Guide

## 🚀 Get Started in 5 Minutes

### What You Need

- ESP32 board (any model with BLE)
- Logitech Z407 speakers
- ESPHome installed (`pip install esphome`)
- WiFi credentials

### Step 1: Find Your Z407's MAC Address (2 minutes)

1. **Prepare the Z407:**
   
   ```
   ⚠️ IMPORTANT: Remove batteries from the physical Z407 remote!
   ✅ Ensure Z407 speakers are powered on
   ✅ Make sure no phone/computer is connected to Z407
   ```

2. **Create `z407_discovery.yaml`:**
   
   ```yaml
   esphome:
     name: z407-discovery
   
   esp32:
     board: esp32dev
     framework:
       type: esp-idf
   
   wifi:
     ssid: "YourWiFiName"
     password: "YourWiFiPassword"
   
   logger:
     level: DEBUG
   
   api:
   
   esp32_ble_tracker:
   
   z407_controller:
     id: z407
     discovery_mode: true
   
   text_sensor:
     - platform: z407_controller
       z407_controller_id: z407
       discovered_address:
         name: "Z407 MAC Address"
   ```

3. **Flash and discover:**
   
   ```bash
   esphome run z407_discovery.yaml
   ```

4. **Get the MAC address:**
   
   - Check the logs for "Found Z407 at XX:XX:XX:XX:XX:XX"
   - Or check Home Assistant for "Z407 MAC Address" sensor
   - **Write it down!** You'll need it for the next step

### Step 2: Normal Operation (3 minutes)

1. **Create `z407_controller.yaml`:**
   
   ```yaml
   esphome:
     name: z407-controller
   
   esp32:
     board: esp32dev
     framework:
       type: esp-idf
   
   wifi:
     ssid: "YourWiFiName"
     password: "YourWiFiPassword"
   
   logger:
   
   api:
   
   ota:
   
   # Add external component
   external_components:
     - source: github://stratus-ss/logitech407-esphome
       components: [z407_controller]
   
   # BLE setup
   esp32_ble_tracker:
   
   ble_client:
     - mac_address: "AA:BB:CC:DD:EE:FF"  # ← YOUR MAC HERE!
       id: z407_ble
   
   # Z407 Controller
   z407_controller:
     id: z407
     ble_client_id: z407_ble
   
   # Buttons
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
         name: "Next"
       previous_track:
         name: "Previous"
   
   # Input selector
   select:
     - platform: z407_controller
       z407_controller_id: z407
       input_source:
         name: "Input"
   
   # Connection status
   binary_sensor:
     - platform: z407_controller
       z407_controller_id: z407
       connection:
         name: "Connected"
   ```

2. **Flash your ESP32:**
   
   ```bash
   esphome run z407_controller.yaml
   ```

3. **Add to Home Assistant:**
   
   - Device should auto-discover
   - All buttons and sensors will appear
   - Start controlling your Z407! 🎉

## 📱 Using in Home Assistant

### Dashboard Card Example

```yaml
type: entities
title: Z407 Speakers
entities:
  - entity: binary_sensor.z407_connected
  - entity: select.z407_input
  - type: buttons
    entities:
      - entity: button.z407_volume_up
      - entity: button.z407_volume_down
      - entity: button.z407_bass_up
      - entity: button.z407_bass_down
  - type: buttons
    entities:
      - entity: button.z407_play_pause
      - entity: button.z407_previous
      - entity: button.z407_next
```

### Automation Example

```yaml
automation:
  - alias: "Morning Music"
    trigger:
      - platform: time
        at: "07:00:00"
    action:
      - service: button.press
        target:
          entity_id: button.z407_input_bluetooth
      - delay:
          seconds: 2
      - service: button.press
        target:
          entity_id: button.z407_play_pause
```

## 🔧 Troubleshooting

### Problem: Can't find MAC address

**Solution:**

1. Remove physical remote batteries
2. Power cycle Z407 speakers
3. Increase discovery duration: `discovery_duration: 60s`
4. Check logs: `esphome logs z407_discovery.yaml`

### Problem: Won't connect

**Solution:**

1. Verify MAC address is correct (case matters!)
2. Ensure Z407 isn't connected to another device
3. Move ESP32 closer to Z407
4. Check framework is `esp-idf` not `arduino`

### Problem: Connected but buttons don't work

**Solution:**

1. Check binary sensor shows "Connected"
2. Wait a few seconds after connection
3. Check logs for handshake completion
4. Try power cycling both devices

### Problem: Keeps disconnecting

**Solution:**

1. Check WiFi signal strength
2. Move ESP32 closer to Z407
3. Reduce WiFi traffic (disable OTA during use)
4. Check power supply is adequate (2A recommended)

## 📚 Next Steps

- **More features?** Check `examples/z407_advanced.yaml`
- **Technical details?** Read `IMPLEMENTATION_GUIDE.md`
- **Full documentation?** See `README.md`
- **Design decisions?** Review `EVALUATION.md`

## 💡 Pro Tips

1. **Keep physical remote batteries out** while ESP32 is connected
2. **Use esp-idf framework** for better BLE stability
3. **Add delays between commands** in scripts (200ms minimum)
4. **Monitor connection status** with binary sensor
5. **Create scripts** for common multi-step actions

## ⚡ Common Use Cases

### Volume Control Script

```yaml
script:
  - id: volume_boost
    then:
      - repeat:
          count: 3
          then:
            - button.press: volume_up
            - delay: 200ms
```

### Auto-Switch Input

```yaml
automation:
  - alias: "Switch to Bluetooth when home"
    trigger:
      - platform: state
        entity_id: person.you
        to: "home"
    action:
      - service: select.select_option
        target:
          entity_id: select.z407_input
        data:
          option: "Bluetooth"
```

### Connection Monitor

```yaml
automation:
  - alias: "Z407 Connection Alert"
    trigger:
      - platform: state
        entity_id: binary_sensor.z407_connected
        to: "off"
        for:
          minutes: 5
    action:
      - service: notify.mobile_app
        data:
          message: "Z407 speakers disconnected"
```

## 🎯 Quick Reference

### All Available Buttons

- `volume_up` / `volume_down` - Master volume
- `bass_up` / `bass_down` - Bass level
- `play_pause` - Toggle playback
- `next_track` / `previous_track` - Track control
- `input_bluetooth` / `input_aux` / `input_usb` - Input switching
- `bluetooth_pair` - Enter pairing mode
- `factory_reset` - Reset speakers
- `sound_1` / `sound_2` / `sound_3` - Test sounds

### Configuration Options

```yaml
z407_controller:
  id: z407
  ble_client_id: z407_ble
  auto_reconnect: true        # Auto-reconnect (default: true)
  connection_timeout: 30s     # Handshake timeout (default: 30s)
```

### Discovery Options

```yaml
z407_controller:
  id: z407
  discovery_mode: true
  discovery_duration: 30s     # Scan duration (default: 30s)
```

## 📞 Getting Help

- **Check logs first:** `esphome logs your_config.yaml`
- **Enable debug logging:** `logger: level: DEBUG`
- **Read full docs:** `README.md`
- **Report issues:** GitHub Issues (include logs!)

## ✅ Checklist

Before asking for help, verify:

- [ ] Using ESP32 (not ESP8266)
- [ ] Framework is `esp-idf`
- [ ] MAC address is correct
- [ ] Physical remote batteries removed
- [ ] Z407 is powered on
- [ ] ESP32 is within BLE range
- [ ] Checked logs for errors
- [ ] Tried power cycling both devices

---

**Ready to go?** Start with Step 1 above! 🚀

**Need more details?** Check the full `README.md`

**Having issues?** Enable debug logging and check the logs!

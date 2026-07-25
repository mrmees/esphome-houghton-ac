# ESPHome CARRIER_AC128 Climate Component

Custom [ESPHome](https://esphome.io/) component for Houghton/RecPro RV AC units that use the **CARRIER_AC128** IR protocol. Creates a native Home Assistant climate entity directly from an ESP8266/ESP8285 IR blaster -- no MQTT, Node-RED, or Tasmota needed.

This repository includes the **only public byte-level documentation** of the CARRIER_AC128 protocol, reverse-engineered from 398 raw IR captures.

## Features

- Native Home Assistant climate entity via ESPHome API (auto-discovered, no configuration needed in HA)
- All AC modes: Cool, Heat, Fan Only, Dehumidify, Maintain (Heat+Cool)
- Fan speeds: Auto, Low, Medium, High
- Temperature: 16-30 C / 60-86 F
- Eco and Sleep presets
- Optional switch entity for the AC's front panel LED display (turn the screen off at night)
- Optional room temperature and humidity display, sourced from any Home Assistant sensor or averaging helper
- Clock sync (sends current time with every command, just like the physical remote)
- IR receive support -- tracks manual remote usage and updates the HA entity state

## Hardware

Tested on an [**M5Stack NanoC6**](https://docs.m5stack.com/en/core/M5NanoC6) (ESP32-C6) paired with the [**M5Stack IR Unit**](https://shop.m5stack.com/products/ir-unit) over its Grove port:

- IR TX on GPIO2 (Grove SDA / G2)
- IR RX on GPIO1 (Grove SCL / G1)

If you don't need to track the physical remote, you can skip the IR Unit entirely and use the **NanoC6's onboard IR LED on GPIO3** for transmit-only operation.

Any ESP8266/ESP32 with an IR LED on any GPIO will work. The receiver is optional -- without it, the component still sends commands but won't track manual remote usage.

## Installation

Add the external component to your ESPHome YAML:

```yaml
external_components:
  - source: github://mrmees/esphome-houghton-ac
    components: [carrier_ac128]
```

## Configuration

Minimal config — NanoC6 onboard IR LED, transmit only:

```yaml
remote_transmitter:
  pin: GPIO3
  carrier_duty_percent: 50%

climate:
  - platform: carrier_ac128
    name: "My AC"
```

Full config — M5 IR Unit on the Grove port, with receive and clock sync:

```yaml
remote_transmitter:
  pin: GPIO2
  carrier_duty_percent: 50%

remote_receiver:
  id: ir_receiver
  pin:
    number: GPIO1
    inverted: true
  idle: 30ms        # CARRIER_AC128 has a 20.6ms gap between sections
  buffer_size: 350  # Full signal is ~267 transitions

time:
  - platform: homeassistant
    id: ha_time

climate:
  - platform: carrier_ac128
    id: my_ac
    name: "My AC"
    time_id: ha_time           # optional: syncs AC clock
    receiver_id: ir_receiver   # optional: tracks remote usage

switch:
  - platform: carrier_ac128    # optional: LED display on/off
    carrier_ac128_id: my_ac
    name: "Display"
```

See [`example.yaml`](example.yaml) for a complete working config for the M5 NanoC6 + IR Unit combo.

### Configuration Options

**`climate` platform:**

| Option | Required | Default | Description |
|--------|----------|---------|-------------|
| `name` | Yes | -- | Name of the climate entity in Home Assistant |
| `time_id` | No | -- | ID of a `time` component for clock sync |
| `receiver_id` | No | -- | ID of a `remote_receiver` for tracking the physical remote |
| `sensor` | No | -- | ID of a sensor to show as the current room temperature |
| `humidity_sensor` | No | -- | ID of a sensor to show as the current room humidity |

**`switch` platform (LED display):**

| Option | Required | Default | Description |
|--------|----------|---------|-------------|
| `name` | Yes | -- | Name of the switch entity in Home Assistant |
| `carrier_ac128_id` | No | Only climate instance | ID of the `carrier_ac128` climate component to control |
| `restore_mode` | No | `RESTORE_DEFAULT_ON` | Standard ESPHome switch restore mode |

All standard ESPHome [climate](https://esphome.io/components/climate/), [climate_ir](https://esphome.io/components/climate/climate_ir/), and [switch](https://esphome.io/components/switch/) options are supported.

### LED Display Switch

The switch controls bit 2 of byte 14 -- the same flag the remote's display button toggles. Turning it on or off re-sends the complete state frame, so the AC's mode, temperature, and fan speed are unchanged.

The switch is created as a config entity. It defaults to ON at boot and does **not** transmit at boot -- the stored value simply rides along with the next command. If the physical remote toggles the display and an IR receiver is configured, the switch state follows it.

To surface it as a light in Home Assistant instead of a switch, wrap it in a [template light](https://www.home-assistant.io/integrations/light.template/) or use a switch-as-x helper.

### Room Temperature and Humidity

The AC's own sensor isn't exposed over IR -- the protocol only carries commands, never readings -- so the climate entity shows no current temperature by default. You can feed it any sensor Home Assistant already knows about, including an average of several.

**1. Create a helper in Home Assistant** (Settings -> Devices & Services -> Helpers -> Create helper -> **Combine the state of several sensors**), set the statistic to **Mean**, and pick your temperature sensors. Repeat for humidity. This is where you choose which sensors count -- add or remove them later without touching YAML.

**2. Point the component at the helpers:**

```yaml
sensor:
  - platform: homeassistant
    id: avg_temp
    entity_id: sensor.average_temperature
    internal: true
    filters:
      - lambda: return (x - 32.0) * 5.0 / 9.0;  # only if HA reports °F

  - platform: homeassistant
    id: avg_humidity
    entity_id: sensor.average_humidity
    internal: true

climate:
  - platform: carrier_ac128
    name: "My AC"
    sensor: avg_temp
    humidity_sensor: avg_humidity
```

Notes:

- **Units.** ESPHome climate works in Celsius internally, so a Fahrenheit source needs the conversion filter above or 74°F arrives as 74°C. Humidity needs no conversion.
- **`internal: true`** stops ESPHome from publishing a duplicate sensor entity back to Home Assistant.
- **Display only.** The AC still cycles on its own internal sensor -- the protocol has no field for an external temperature, so this changes what the card reads, not how the unit behaves. To regulate on the average instead, drive the setpoint from an HA automation or something like [Better Thermostat](https://github.com/KartoffelToby/better_thermostat).
- **No humidity setpoint.** Dehumidify is a plain on/off mode in this protocol, so Home Assistant shows current humidity but offers no target.

## How It Works

Every command sends the full AC state as a 16-byte (128-bit) IR bitmap:

```
HA climate entity --> ESPHome builds 16-byte bitmap --> IR LED blasts it --> AC unit
```

The bitmap encodes mode, fan speed, temperature (both C and F), eco/sleep flags, power state, timers, and the current clock time. Two independent checksums protect each 8-byte half.

When the physical remote is used (and an IR receiver is connected), the component decodes the signal, verifies checksums, and updates the climate entity state in HA.

## Supported AC Units

This component works with AC units that use the **CARRIER_AC128** IR protocol, including:

- RecPro RV air conditioners
- Houghton window AC units
- Other units whose remotes are identified as `CARRIER_AC128` by [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266)

Note: Despite the protocol name, these are **not** standard Carrier HVAC units. The CARRIER_AC128 protocol is used by RecPro/Houghton and possibly other OEM-branded units.

## Protocol Documentation

See [`docs/protocol-spec.md`](docs/protocol-spec.md) for the complete byte-level protocol reference, including:

- 16-byte bitmap layout with all fields
- Fan speed, mode, temperature, timer, and flag encoding
- Dual checksum algorithm with worked examples
- BCD encoding reference
- Errata from the original reverse-engineering

## Troubleshooting

**AC doesn't respond to commands:**
- Verify the IR LED is connected to the correct GPIO and pointed at the AC's IR receiver
- Check ESPHome logs for the `carrier_ac128` tag -- it logs the full 16-byte hex command on every send
- Try increasing `carrier_duty_percent` (some IR LEDs need more power)

**Climate entity not appearing in HA:**
- Ensure the ESPHome device is connected and online
- Check the ESPHome integration in HA for the device
- Verify the `api:` section is configured in your ESPHome YAML

**IR receiver not tracking remote:**
- Ensure `idle: 30ms` and `buffer_size: 350` are set on the `remote_receiver` -- the CARRIER_AC128 signal has a 20.6ms gap between its two halves that exceeds ESPHome's default idle timeout
- Verify `inverted: true` on the receiver pin (most IR demodulators output active-low)
- Check that `receiver_id` is set in the climate config
- Look for `carrier_ac128: Received:` messages in the ESPHome logs when pressing remote buttons

## License

MIT

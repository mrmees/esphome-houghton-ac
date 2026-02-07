# ESPHome CARRIER_AC128 Climate Component

Custom [ESPHome](https://esphome.io/) component for Houghton/RecPro RV AC units that use the **CARRIER_AC128** IR protocol. Creates a native Home Assistant climate entity directly from an ESP8266/ESP8285 IR blaster -- no MQTT, Node-RED, or Tasmota needed.

This repository includes the **only public byte-level documentation** of the CARRIER_AC128 protocol, reverse-engineered from 398 raw IR captures.

## Features

- Native Home Assistant climate entity via ESPHome API (auto-discovered, no configuration needed in HA)
- All AC modes: Cool, Heat, Fan Only, Dehumidify, Maintain (Heat+Cool)
- Fan speeds: Auto, Low, Medium, High
- Temperature: 16-30 C / 60-86 F
- Eco and Sleep presets
- Clock sync (sends current time with every command, just like the physical remote)
- IR receive support -- tracks manual remote usage and updates the HA entity state

## Hardware

You need an ESP8266 or ESP8285 with an IR LED (transmitter) and optionally an IR receiver/demodulator. Tested on an **ESP8285 ESP-01M IR transceiver module** with:

- IR TX on GPIO4
- IR RX on GPIO14 (optional but recommended)
- 5V power (onboard regulator)

Any ESP8266/ESP32 with an IR LED on any GPIO will work. The receiver is optional -- without it, the component still sends commands but won't track manual remote usage.

## Installation

Add the external component to your ESPHome YAML:

```yaml
external_components:
  - source: github://mrmees/esphome-houghton-ac
    components: [carrier_ac128]
```

## Configuration

Minimal config (TX only):

```yaml
remote_transmitter:
  pin: GPIO4
  carrier_duty_percent: 50%

climate:
  - platform: carrier_ac128
    name: "My AC"
```

Full config with receiver and clock sync:

```yaml
remote_transmitter:
  pin: GPIO4
  carrier_duty_percent: 50%

remote_receiver:
  id: ir_receiver
  pin:
    number: GPIO14
    inverted: true

time:
  - platform: homeassistant
    id: ha_time

climate:
  - platform: carrier_ac128
    name: "My AC"
    time_id: ha_time           # optional: syncs AC clock
    receiver_id: ir_receiver   # optional: tracks remote usage
```

See [`example.yaml`](example.yaml) for a complete working config for the ESP8285 ESP-01M module.

### Configuration Options

| Option | Required | Default | Description |
|--------|----------|---------|-------------|
| `name` | Yes | -- | Name of the climate entity in Home Assistant |
| `time_id` | No | -- | ID of a `time` component for clock sync |
| `receiver_id` | No | -- | ID of a `remote_receiver` for tracking the physical remote |

All standard ESPHome [climate](https://esphome.io/components/climate/) and [climate_ir](https://esphome.io/components/climate/climate_ir/) options are supported.

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
- Verify `inverted: true` on the receiver pin (most IR demodulators output active-low)
- Check that `receiver_id` is set in the climate config
- Look for `carrier_ac128: Received:` messages in the ESPHome logs when pressing remote buttons

## License

MIT

#include "carrier_ac128.h"
#include "esphome/core/log.h"

namespace esphome {
namespace carrier_ac128 {

static const char *const TAG = "carrier_ac128";

// --- Transmit ---
// Framing matches IRremoteESP8266 sendCarrierAC128():
//   Section 1: hdr(4600/2600) + 64 bits + footer(340) + gap(20600)
//   Inter:     mark(4600) + space(6700)
//   Section 2: hdr(9300/5000) + 64 bits + footer(340) + gap(20600)
//   Final:     mark(4600)

void CarrierAC128Climate::transmit_state() {
  uint8_t bytes[16] = {0};
  build_state_(bytes);

  ESP_LOGD(TAG, "Sending: %02X %02X %02X %02X %02X %02X %02X %02X  %02X %02X %02X %02X %02X %02X %02X %02X",
           bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
           bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(kCarrierFreq);

  // Section 1: bytes 0-7 (LSB first, matches IRremoteESP8266 MSBfirst=false)
  data->mark(kHdrMark);
  data->space(kHdrSpace);
  for (uint8_t i = 0; i < 8; i++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      data->mark(kBitMark);
      data->space((bytes[i] & (1 << bit)) ? kOneSpace : kZeroSpace);
    }
  }
  data->mark(kBitMark);       // section 1 footer mark
  data->space(kSectionGap);   // section gap (20600us)

  // Inter-section markers
  data->mark(kHdrMark);       // inter-section mark (4600us)
  data->space(kInterSpace);   // inter-section space (6700us)

  // Section 2: bytes 8-15 (LSB first)
  data->mark(kHdr2Mark);
  data->space(kHdr2Space);
  for (uint8_t i = 8; i < 16; i++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      data->mark(kBitMark);
      data->space((bytes[i] & (1 << bit)) ? kOneSpace : kZeroSpace);
    }
  }
  data->mark(kBitMark);       // section 2 footer mark
  data->space(kSectionGap);   // section gap (20600us)
  data->mark(kHdrMark);       // final mark (4600us)

  transmit.perform();
}

void CarrierAC128Climate::build_state_(uint8_t *bytes) {
  // Byte 1: Header (always 0x16)
  bytes[0] = 0x16;

  // Byte 2: Fan speed (high nibble) + Mode (low nibble)
  uint8_t fan_nibble;
  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_LOW:    fan_nibble = 0x8; break;
    case climate::CLIMATE_FAN_MEDIUM: fan_nibble = 0x4; break;
    case climate::CLIMATE_FAN_HIGH:   fan_nibble = 0x2; break;
    default:                          fan_nibble = 0x1; break;
  }

  uint8_t mode_nibble;
  bool power_on = this->mode != climate::CLIMATE_MODE_OFF;
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:      mode_nibble = 0x2; break;
    case climate::CLIMATE_MODE_HEAT:      mode_nibble = 0x8; break;
    case climate::CLIMATE_MODE_FAN_ONLY:  mode_nibble = 0x4; break;
    case climate::CLIMATE_MODE_DRY:       mode_nibble = 0x1; break;
    case climate::CLIMATE_MODE_HEAT_COOL: mode_nibble = 0xA; break;
    default:                              mode_nibble = 0x2; break;  // OFF defaults to cool
  }
  bytes[1] = (fan_nibble << 4) | mode_nibble;

  // Bytes 3-4: Clock time (BCD, syncs the AC's internal clock)
  uint8_t hours = 0, minutes = 0, seconds = 0;
#ifdef USE_TIME
  if (this->time_source_ != nullptr) {
    auto time = this->time_source_->now();
    if (time.is_valid()) {
      hours = time.hour;
      minutes = time.minute;
      seconds = time.second;
    }
  }
#endif
  bytes[2] = bcd_(minutes);
  bytes[3] = bcd_(hours);

  // Bytes 5-6: Timers (disabled -- timer scheduling is handled at the HA level)
  bytes[4] = 0x00;
  bytes[5] = 0x00;

  // Byte 7: Temperature Celsius (BCD)
  uint8_t temp_c = (uint8_t) std::max(16, std::min(30, (int) roundf(this->target_temperature)));
  bytes[6] = bcd_(temp_c);

  // Byte 8: flags (low nibble) -- checksum computed below
  //   Bit 5 (0x8): Celsius display flag
  //   Bit 7 (0x2): Sleep mode
  uint8_t byte8_flags = 0;
  bool sleep_on = this->preset.has_value() &&
                  this->preset.value() == climate::CLIMATE_PRESET_SLEEP;
  if (sleep_on) byte8_flags |= 0x2;
  bytes[7] = byte8_flags;

  // Bytes 9-10: Timer minutes (disabled)
  bytes[8] = 0x00;
  bytes[9] = 0x00;

  // Byte 11: Tx counter (0) + power flags
  bytes[10] = power_on ? 0x08 : 0x0C;

  // Byte 12: Temperature Fahrenheit (BCD)
  // Derived from raw float target (not rounded C) to preserve F precision
  float temp_f_raw = this->target_temperature * 9.0f / 5.0f + 32.0f;
  uint8_t temp_f = (uint8_t) std::max(60, std::min(86, (int) roundf(temp_f_raw)));
  bytes[11] = bcd_(temp_f);

  // Byte 13: Eco mode (bit 6 = 0x04)
  bool eco_on = this->preset.has_value() &&
                this->preset.value() == climate::CLIMATE_PRESET_ECO;
  bytes[12] = eco_on ? 0x04 : 0x00;

  // Byte 14: Lock (off) + LED display (on)
  bytes[13] = 0x00;

  // Byte 15: Clock seconds (BCD)
  bytes[14] = bcd_(seconds);

  // Byte 16: checksum placeholder
  bytes[15] = 0x00;

  // Checksum 1: sum all hex digits of bytes 1-7 + low nibble of byte 8, mod 16
  uint8_t cs1 = 0;
  for (int i = 0; i < 7; i++) {
    cs1 += (bytes[i] >> 4) & 0xF;
    cs1 += bytes[i] & 0xF;
  }
  cs1 += bytes[7] & 0xF;
  bytes[7] = ((cs1 % 16) << 4) | byte8_flags;

  // Checksum 2: sum all hex digits of bytes 9-15 + low nibble of byte 16, mod 16
  uint8_t cs2 = 0;
  for (int i = 8; i < 15; i++) {
    cs2 += (bytes[i] >> 4) & 0xF;
    cs2 += bytes[i] & 0xF;
  }
  cs2 += bytes[15] & 0xF;  // always 0
  bytes[15] = (cs2 % 16) << 4;
}

// --- Receive ---
// Matches IRremoteESP8266 decodeCarrierAC128():
//   Section 1: hdr(4600/2600) + 64 bits + footer(340) + gap(20600)
//   Inter:     mark(4600) + space(6700)
//   Section 2: hdr(9300/5000) + 64 bits

bool CarrierAC128Climate::on_receive(remote_base::RemoteReceiveData data) {
  // Section 1 header
  if (!data.expect_item(kHdrMark, kHdrSpace))
    return false;

  uint8_t bytes[16] = {0};

  // Read 64 bits (bytes 0-7), LSB first (matches IRremoteESP8266 MSBfirst=false)
  for (uint8_t i = 0; i < 8; i++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (!data.expect_mark(kBitMark))
        return false;
      if (data.expect_space(kOneSpace)) {
        bytes[i] |= (1 << bit);
      } else if (data.expect_space(kZeroSpace)) {
        // bit stays 0
      } else {
        return false;
      }
    }
  }

  // Section 1 footer + section gap
  if (!data.expect_mark(kBitMark))
    return false;
  if (!data.expect_space(kSectionGap))
    return false;

  // Inter-section markers (4600us mark + 6700us space)
  if (!data.expect_mark(kHdrMark))
    return false;
  if (!data.expect_space(kInterSpace))
    return false;

  // Section 2 header
  if (!data.expect_item(kHdr2Mark, kHdr2Space))
    return false;

  // Read 64 bits (bytes 8-15), LSB first
  for (uint8_t i = 8; i < 16; i++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (!data.expect_mark(kBitMark))
        return false;
      if (data.expect_space(kOneSpace)) {
        bytes[i] |= (1 << bit);
      } else if (data.expect_space(kZeroSpace)) {
        // bit stays 0
      } else {
        return false;
      }
    }
  }

  // Verify protocol header
  if (bytes[0] != 0x16)
    return false;

  // Verify checksum 1
  uint8_t cs1 = 0;
  for (int i = 0; i < 7; i++) {
    cs1 += (bytes[i] >> 4) & 0xF;
    cs1 += bytes[i] & 0xF;
  }
  cs1 += bytes[7] & 0xF;
  if (((cs1 % 16) << 4) != (bytes[7] & 0xF0))
    return false;

  // Verify checksum 2
  uint8_t cs2 = 0;
  for (int i = 8; i < 15; i++) {
    cs2 += (bytes[i] >> 4) & 0xF;
    cs2 += bytes[i] & 0xF;
  }
  cs2 += bytes[15] & 0xF;
  if (((cs2 % 16) << 4) != (bytes[15] & 0xF0))
    return false;

  ESP_LOGD(TAG, "Received: %02X %02X %02X %02X %02X %02X %02X %02X  %02X %02X %02X %02X %02X %02X %02X %02X",
           bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
           bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);

  // Decode power (byte 11 low nibble: 0x0C = off)
  bool power_off = (bytes[10] & 0x0F) == 0x0C;

  // Decode mode (byte 2 low nibble)
  if (power_off) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    switch (bytes[1] & 0x0F) {
      case 0x1: this->mode = climate::CLIMATE_MODE_DRY; break;
      case 0x2: this->mode = climate::CLIMATE_MODE_COOL; break;
      case 0x4: this->mode = climate::CLIMATE_MODE_FAN_ONLY; break;
      case 0x8: this->mode = climate::CLIMATE_MODE_HEAT; break;
      case 0xA: this->mode = climate::CLIMATE_MODE_HEAT_COOL; break;
      default:  this->mode = climate::CLIMATE_MODE_COOL; break;
    }
  }

  // Decode fan speed (byte 2 high nibble)
  switch ((bytes[1] >> 4) & 0x0F) {
    case 0x8: this->fan_mode = climate::CLIMATE_FAN_LOW; break;
    case 0x4: this->fan_mode = climate::CLIMATE_FAN_MEDIUM; break;
    case 0x2: this->fan_mode = climate::CLIMATE_FAN_HIGH; break;
    default:  this->fan_mode = climate::CLIMATE_FAN_AUTO; break;
  }

  // Decode temperature (byte 7, Celsius BCD)
  this->target_temperature = (float) from_bcd_(bytes[6]);

  // Decode presets (eco and sleep are mutually exclusive in ESPHome's model)
  bool eco = (bytes[12] & 0x04) != 0;
  bool sleep = (bytes[7] & 0x02) != 0;
  if (eco)
    this->preset = climate::CLIMATE_PRESET_ECO;
  else if (sleep)
    this->preset = climate::CLIMATE_PRESET_SLEEP;
  else
    this->preset = climate::CLIMATE_PRESET_NONE;

  this->publish_state();
  return true;
}

}  // namespace carrier_ac128
}  // namespace esphome

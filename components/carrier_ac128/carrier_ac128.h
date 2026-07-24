#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome {
namespace carrier_ac128 {

// IR timing constants (microseconds) — from IRremoteESP8266 ir_Carrier.cpp
static const uint16_t kHdrMark = 4600;      // section 1 header mark (also inter-section mark)
static const uint16_t kHdrSpace = 2600;     // section 1 header space
static const uint16_t kHdr2Mark = 9300;     // section 2 header mark
static const uint16_t kHdr2Space = 5000;    // section 2 header space
static const uint16_t kBitMark = 340;       // data bit mark
static const uint16_t kOneSpace = 1000;     // data bit 1 space
static const uint16_t kZeroSpace = 400;     // data bit 0 space
static const uint16_t kSectionGap = 20600;  // space after each section's 64 data bits
static const uint16_t kInterSpace = 6700;   // space between inter-section mark and section 2
static const uint32_t kCarrierFreq = 38000;

class CarrierAC128Climate : public climate_ir::ClimateIR {
 public:
  CarrierAC128Climate()
      : climate_ir::ClimateIR(
            16.0f, 30.0f, 0.5f,  // min, max, step (0.5C gives ~1F precision)
            true,   // supports DRY (dehumidify)
            true,   // supports FAN_ONLY
            {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW,
             climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH},
            {},  // no swing modes
            {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_ECO,
             climate::CLIMATE_PRESET_SLEEP}) {}

#ifdef USE_TIME
  void set_time_source(time::RealTimeClock *time_source) { this->time_source_ = time_source; }
#endif

  /// Turn the AC's front panel LED display on/off (re-sends the full state frame).
  void set_display(bool display_on);
  /// Set the stored display state without transmitting -- used to restore it at boot.
  void set_display_state(bool display_on) { this->display_on_ = display_on; }
  bool get_display() const { return this->display_on_; }

#ifdef USE_SWITCH
  void set_display_switch(switch_::Switch *display_switch) { this->display_switch_ = display_switch; }
#endif

 protected:
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

 private:
  uint8_t bcd_(uint8_t value) { return ((value / 10) << 4) | (value % 10); }
  uint8_t from_bcd_(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
  void build_state_(uint8_t *bytes);
  void publish_display_state_();

  bool display_on_{true};

#ifdef USE_SWITCH
  switch_::Switch *display_switch_{nullptr};
#endif

#ifdef USE_TIME
  time::RealTimeClock *time_source_{nullptr};
#endif
};

}  // namespace carrier_ac128
}  // namespace esphome

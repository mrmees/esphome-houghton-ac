#include "carrier_ac128_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace carrier_ac128 {

static const char *const TAG = "carrier_ac128.switch";

void CarrierAC128DisplaySwitch::setup() {
  // Adopt the restored (or default) state without blasting IR at boot -- the AC
  // keeps its own display setting, and the next command carries this value.
  bool state = this->get_initial_state_with_restore_mode().value_or(true);
  this->parent_->set_display_state(state);
  this->publish_state(state);
}

void CarrierAC128DisplaySwitch::dump_config() { log_switch(TAG, "  ", "Carrier AC128 Display Switch", this); }

void CarrierAC128DisplaySwitch::write_state(bool state) {
  this->parent_->set_display(state);
  this->publish_state(state);
}

}  // namespace carrier_ac128
}  // namespace esphome

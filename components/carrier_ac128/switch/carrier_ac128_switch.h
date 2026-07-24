#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/carrier_ac128/carrier_ac128.h"

namespace esphome {
namespace carrier_ac128 {

class CarrierAC128DisplaySwitch : public switch_::Switch,
                                  public Component,
                                  public Parented<CarrierAC128Climate> {
 public:
  void setup() override;
  void dump_config() override;
  void write_state(bool state) override;
};

}  // namespace carrier_ac128
}  // namespace esphome

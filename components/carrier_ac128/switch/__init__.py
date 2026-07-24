import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import carrier_ac128_ns
from ..climate import CarrierAC128Climate

CONF_CARRIER_AC128_ID = "carrier_ac128_id"

CarrierAC128DisplaySwitch = carrier_ac128_ns.class_(
    "CarrierAC128DisplaySwitch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = (
    switch.switch_schema(
        CarrierAC128DisplaySwitch,
        icon="mdi:led-outline",
        default_restore_mode="RESTORE_DEFAULT_ON",
        entity_category=ENTITY_CATEGORY_CONFIG,
    )
    .extend(
        {
            cv.GenerateID(CONF_CARRIER_AC128_ID): cv.use_id(CarrierAC128Climate),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_CARRIER_AC128_ID])
    await cg.register_parented(var, parent)
    cg.add(parent.set_display_switch(var))

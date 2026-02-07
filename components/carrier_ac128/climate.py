import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir, time as time_component
from esphome.const import CONF_ID

from . import carrier_ac128_ns

AUTO_LOAD = ["climate_ir"]

CONF_TIME_ID = "time_id"

CarrierAC128Climate = carrier_ac128_ns.class_(
    "CarrierAC128Climate", climate_ir.ClimateIR
)

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(CarrierAC128Climate),
        cv.Optional(CONF_TIME_ID): cv.use_id(time_component.RealTimeClock),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate_ir.register_climate_ir(var, config)

    if CONF_TIME_ID in config:
        time_source = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time_source(time_source))

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir, time as time_component
from esphome.const import CONF_ID

from . import carrier_ac128_ns

AUTO_LOAD = ["climate_ir"]

CONF_TIME_ID = "time_id"
CONF_TEMPERATURE_UNIT = "temperature_unit"

# (visual step, min, max) in Celsius, plus the AC's own display flag.
#
# Home Assistant applies the step as its display precision *after* converting
# to the user's unit, and only 0.5 and 0.1 get fractional rounding -- anything
# else lands on whole numbers. 5/9 C is exactly 1 F, so a Fahrenheit UI steps
# and displays in whole degrees; 1.0 does the same for a Celsius UI.
TEMPERATURE_UNITS = {
    # 15.5556 C == 60 F, the lowest set point the remote offers
    "fahrenheit": (5.0 / 9.0, 15.5556, 30.0, False),
    "celsius": (1.0, 16.0, 30.0, True),
}

CarrierAC128Climate = carrier_ac128_ns.class_(
    "CarrierAC128Climate", climate_ir.ClimateIR
)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(CarrierAC128Climate).extend(
    {
        cv.Optional(CONF_TIME_ID): cv.use_id(time_component.RealTimeClock),
        cv.Optional(CONF_TEMPERATURE_UNIT, default="fahrenheit"): cv.one_of(
            *TEMPERATURE_UNITS, lower=True
        ),
    }
)


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)

    step, min_temp, max_temp, celsius = TEMPERATURE_UNITS[config[CONF_TEMPERATURE_UNIT]]
    cg.add(var.set_temperature_step(step))
    cg.add(var.set_temperature_range(min_temp, max_temp))
    cg.add(var.set_celsius_display(celsius))

    if CONF_TIME_ID in config:
        time_source = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time_source(time_source))

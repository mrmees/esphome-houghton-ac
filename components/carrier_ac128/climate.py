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
# The step has to be >= 1.0 in both cases. Home Assistant buckets it into a
# display precision (>= 1 whole, >= 0.5 halves, else tenths) and applies that
# after converting to the user's unit, so a sub-1.0 step lands a Fahrenheit
# dashboard on half degrees. HA does not unit-convert the step itself, so 1.0
# reads as one degree in whichever unit is on screen. Only min/max differ.
TEMPERATURE_UNITS = {
    # 15.5556 C == 60 F, the lowest set point the remote offers
    "fahrenheit": (1.0, 15.5556, 30.0, False),
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

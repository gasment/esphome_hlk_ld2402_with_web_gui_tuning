import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, UNIT_CENTIMETER, ICON_RULER, STATE_CLASS_MEASUREMENT
from . import LD2402Component, CONF_LD2402_ID

DEPENDENCIES = ["ld2402"]

CONF_DISTANCE = "distance"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2402_ID): cv.use_id(LD2402Component),
        cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CENTIMETER,
            icon=ICON_RULER,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LD2402_ID])
    if CONF_DISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_DISTANCE])
        cg.add(hub.set_distance_sensor(sens))

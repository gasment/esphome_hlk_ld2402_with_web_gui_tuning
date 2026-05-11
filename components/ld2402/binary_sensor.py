import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID
from . import LD2402Component, CONF_LD2402_ID, ld2402_ns

DEPENDENCIES = ["ld2402"]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_LD2402_ID): cv.use_id(LD2402Component),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LD2402_ID])
    var = await binary_sensor.new_binary_sensor(config)
    cg.add(hub.set_presence_sensor(var))

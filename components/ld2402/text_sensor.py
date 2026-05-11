import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID
from . import LD2402Component, CONF_LD2402_ID

DEPENDENCIES = ["ld2402"]

CONF_FIRMWARE_VERSION = "firmware_version"
CONF_WORK_MODE = "work_mode"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2402_ID): cv.use_id(LD2402Component),
        cv.Optional(CONF_FIRMWARE_VERSION): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_WORK_MODE): text_sensor.text_sensor_schema(),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LD2402_ID])
    if CONF_FIRMWARE_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_FIRMWARE_VERSION])
        cg.add(hub.set_firmware_sensor(sens))
    if CONF_WORK_MODE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_WORK_MODE])
        cg.add(hub.set_work_mode_sensor(sens))

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart", "esp32", "network"] 
AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]
MULTI_CONF = False

ld2402_ns = cg.esphome_ns.namespace("ld2402")
LD2402Component = ld2402_ns.class_(
    "LD2402Component", cg.Component, uart.UARTDevice
)

CONF_LD2402_ID = "ld2402_id"
CONF_WEB_PORT = "web_port"
CONF_WEB_USERNAME = "web_username"
CONF_WEB_PASSWORD = "web_password"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD2402Component),
            cv.Optional(CONF_WEB_PORT, default=8080): cv.port,
            cv.Optional(CONF_WEB_USERNAME, default="admin"): cv.string,
            cv.Optional(CONF_WEB_PASSWORD, default="admin"): cv.string,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_web_port(config[CONF_WEB_PORT]))
    cg.add(var.set_web_credentials(
        config[CONF_WEB_USERNAME],
        config[CONF_WEB_PASSWORD]
    ))

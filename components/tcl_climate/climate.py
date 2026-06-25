import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uart, select
from esphome.const import CONF_ID
from esphome.components.tcl_climate import tcl_climate_ns

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["select"]

TCLClimate = tcl_climate_ns.class_(
    "TCLClimate", climate.Climate, uart.UARTDevice, cg.PollingComponent
)

CONF_VSWING_SELECT = "vswing_select"
CONF_HSWING_SELECT = "hswing_select"

CONFIG_SCHEMA = climate.climate_schema(TCLClimate).extend({
    cv.Optional(CONF_VSWING_SELECT): cv.use_id(select.Select),
    cv.Optional(CONF_HSWING_SELECT): cv.use_id(select.Select),
}).extend(uart.UART_DEVICE_SCHEMA).extend(cv.polling_component_schema("450ms"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    await uart.register_uart_device(var, config)

    if CONF_VSWING_SELECT in config:
        sel = await cg.get_variable(config[CONF_VSWING_SELECT])
        cg.add(var.set_vswing_select(sel))

    if CONF_HSWING_SELECT in config:
        sel = await cg.get_variable(config[CONF_HSWING_SELECT])
        cg.add(var.set_hswing_select(sel))

"""
tcl_climate/climate.py  –  ESPHome 2026.6.x

Platform-Datei für climate.tcl_climate.
Alle custom Config-Keys (vswing_select, hswing_select) müssen hier stehen,
damit ESPHome sie im climate-Platform-Schema-Kontext erkennt.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uart, select
from esphome.components.tcl_climate import tcl_climate_ns

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["select"]

TclClimate = tcl_climate_ns.class_(
    "TclClimate", climate.Climate, uart.UARTDevice, cg.Component
)

CONF_VSWING_SELECT = "vswing_select"
CONF_HSWING_SELECT = "hswing_select"

# climate_schema(Class) ist die korrekte API ab ESPHome 2025.11.0
CONFIG_SCHEMA = climate.climate_schema(TclClimate).extend(
    {
        cv.Optional(CONF_VSWING_SELECT): cv.use_id(select.Select),
        cv.Optional(CONF_HSWING_SELECT): cv.use_id(select.Select),
    }
).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[cg.CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)

    if CONF_VSWING_SELECT in config:
        sel = await cg.get_variable(config[CONF_VSWING_SELECT])
        cg.add(var.set_vswing_select(sel))

    if CONF_HSWING_SELECT in config:
        sel = await cg.get_variable(config[CONF_HSWING_SELECT])
        cg.add(var.set_hswing_select(sel))

"""
tcl_climate/__init__.py

ESPHome Custom Component für TCL/Kesser/Pioneer/DAIZUKI Klimaanlagen.
Unterstützt ab ESPHome 2026.6.x vertikale UND horizontale Lamellensteuerung.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uart, select
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["select"]

tcl_climate_ns = cg.esphome_ns.namespace("tcl_climate")
TclClimate = tcl_climate_ns.class_(
    "TclClimate", climate.Climate, uart.UARTDevice, cg.Component
)

# Config-Schlüssel
CONF_VSWING_SELECT = "vswing_select"
CONF_HSWING_SELECT = "hswing_select"

CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TclClimate),
        # Vertikales Lamellen-Select (optional, rückwärtskompatibel)
        cv.Optional(CONF_VSWING_SELECT): cv.use_id(select.Select),
        # Horizontales Lamellen-Select (neu)
        cv.Optional(CONF_HSWING_SELECT): cv.use_id(select.Select),
    }
).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)

    if CONF_VSWING_SELECT in config:
        sel = await cg.get_variable(config[CONF_VSWING_SELECT])
        cg.add(var.set_vswing_select(sel))

    if CONF_HSWING_SELECT in config:
        sel = await cg.get_variable(config[CONF_HSWING_SELECT])
        cg.add(var.set_hswing_select(sel))

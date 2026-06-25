"""
tcl_climate/__init__.py  –  ESPHome 2026.6.x kompatibel

Der Namespace und die Klasse werden hier nur registriert.
Alle Platform-spezifischen Config-Keys (vswing_select, hswing_select)
gehören in climate.py, damit ESPHome sie im richtigen Schema-Kontext findet.
"""

import esphome.codegen as cg

tcl_climate_ns = cg.esphome_ns.namespace("tcl_climate")

TclClimate = tcl_climate_ns.class_("TclClimate")

"""
Select platform for Z407 Controller

Provides a select entity for choosing the input source.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID, CONF_ICON
from .. import z407_controller_ns, Z407Controller

DEPENDENCIES = ["z407_controller"]

# Define select class
Z407Select = z407_controller_ns.class_("Z407Select", select.Select, cg.Component)

# Constants
CONF_Z407_CONTROLLER_ID = "z407_controller_id"
CONF_INPUT_SOURCE = "input_source"

# Input source options
INPUT_OPTIONS = ["Bluetooth", "AUX", "USB"]

# Configuration schema
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_Z407_CONTROLLER_ID): cv.use_id(Z407Controller),
        cv.Optional(CONF_INPUT_SOURCE): select.select_schema(
            Z407Select,
            icon="mdi:audio-input-stereo-minijack",
        ),
    }
)


async def to_code(config):
    """Generate C++ code for select platform."""
    parent = await cg.get_variable(config[CONF_Z407_CONTROLLER_ID])
    
    if CONF_INPUT_SOURCE in config:
        conf = config[CONF_INPUT_SOURCE]
        sel = await select.new_select(conf, options=INPUT_OPTIONS)
        await cg.register_component(sel, conf)
        
        cg.add(sel.set_parent(parent))


"""
Text Sensor platform for Z407 Controller

Provides text sensors for discovery results and other string data.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_ID,
    CONF_ICON,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from .. import z407_controller_ns, Z407Controller

DEPENDENCIES = ["z407_controller"]

# Define text sensor class
Z407TextSensor = z407_controller_ns.class_(
    "Z407TextSensor", text_sensor.TextSensor, cg.Component
)

# Constants
CONF_Z407_CONTROLLER_ID = "z407_controller_id"
CONF_DISCOVERED_ADDRESS = "discovered_address"

# Configuration schema
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_Z407_CONTROLLER_ID): cv.use_id(Z407Controller),
        cv.Optional(CONF_DISCOVERED_ADDRESS): text_sensor.text_sensor_schema(
            Z407TextSensor,
            icon="mdi:bluetooth-audio",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    """Generate C++ code for text sensor platform."""
    parent = await cg.get_variable(config[CONF_Z407_CONTROLLER_ID])
    
    if CONF_DISCOVERED_ADDRESS in config:
        conf = config[CONF_DISCOVERED_ADDRESS]
        sens = await text_sensor.new_text_sensor(conf)
        await cg.register_component(sens, conf)
        
        cg.add(sens.set_parent(parent))


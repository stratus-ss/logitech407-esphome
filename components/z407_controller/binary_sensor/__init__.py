"""
Binary Sensor platform for Z407 Controller

Provides a binary sensor for connection status.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_ICON,
    DEVICE_CLASS_CONNECTIVITY,
)
from .. import z407_controller_ns, Z407Controller

DEPENDENCIES = ["z407_controller"]

# Define binary sensor class
Z407BinarySensor = z407_controller_ns.class_(
    "Z407BinarySensor", binary_sensor.BinarySensor, cg.Component
)

# Constants
CONF_Z407_CONTROLLER_ID = "z407_controller_id"
CONF_CONNECTION = "connection"

# Configuration schema
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_Z407_CONTROLLER_ID): cv.use_id(Z407Controller),
        cv.Optional(CONF_CONNECTION): binary_sensor.binary_sensor_schema(
            Z407BinarySensor,
            device_class=DEVICE_CLASS_CONNECTIVITY,
            icon="mdi:bluetooth-connect",
        ),
    }
)


async def to_code(config):
    """Generate C++ code for binary sensor platform."""
    parent = await cg.get_variable(config[CONF_Z407_CONTROLLER_ID])
    
    if CONF_CONNECTION in config:
        conf = config[CONF_CONNECTION]
        sens = await binary_sensor.new_binary_sensor(conf)
        await cg.register_component(sens, conf)
        
        cg.add(sens.set_parent(parent))


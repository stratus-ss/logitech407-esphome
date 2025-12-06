"""
Sensor platform for Z407 Controller

Provides sensors for RSSI and other metrics.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_ICON,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from .. import z407_controller_ns, Z407Controller

DEPENDENCIES = ["z407_controller"]

# Define sensor class
Z407Sensor = z407_controller_ns.class_("Z407Sensor", sensor.Sensor, cg.Component)

# Constants
CONF_Z407_CONTROLLER_ID = "z407_controller_id"
CONF_RSSI = "rssi"
CONF_UPDATE_INTERVAL = "update_interval"

# Configuration schema
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_Z407_CONTROLLER_ID): cv.use_id(Z407Controller),
        cv.Optional(CONF_RSSI): sensor.sensor_schema(
            Z407Sensor,
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            icon="mdi:bluetooth-audio",
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(
            {
                cv.Optional(
                    CONF_UPDATE_INTERVAL, default="60s"
                ): cv.positive_time_period_milliseconds,
            }
        ),
    }
)


async def to_code(config):
    """Generate C++ code for sensor platform."""
    parent = await cg.get_variable(config[CONF_Z407_CONTROLLER_ID])
    
    if CONF_RSSI in config:
        conf = config[CONF_RSSI]
        sens = await sensor.new_sensor(conf)
        await cg.register_component(sens, conf)
        
        cg.add(sens.set_parent(parent))
        if CONF_UPDATE_INTERVAL in conf:
            cg.add(sens.set_update_interval(conf[CONF_UPDATE_INTERVAL]))


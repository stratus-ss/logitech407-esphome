"""
Button platform for Z407 Controller

Provides button entities for all Z407 commands.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import (
    CONF_ID,
    CONF_ICON,
    DEVICE_CLASS_EMPTY,
)
from .. import z407_controller_ns, Z407Controller

DEPENDENCIES = ["z407_controller"]

# Add constant for z407_controller_id
CONF_Z407_CONTROLLER_ID = "z407_controller_id"

# Define button class
Z407Button = z407_controller_ns.class_("Z407Button", button.Button, cg.Component)

# Define command enum
Z407Command = z407_controller_ns.enum("Z407Command")

# Command definitions with default icons
COMMANDS = {
    "volume_up": {
        "value": 0x8002,
        "icon": "mdi:volume-plus",
    },
    "volume_down": {
        "value": 0x8003,
        "icon": "mdi:volume-minus",
    },
    "bass_up": {
        "value": 0x8000,
        "icon": "mdi:music-clef-bass",
    },
    "bass_down": {
        "value": 0x8001,
        "icon": "mdi:music-clef-bass",
    },
    "play_pause": {
        "value": 0x8004,
        "icon": "mdi:play-pause",
    },
    "next_track": {
        "value": 0x8005,
        "icon": "mdi:skip-next",
    },
    "previous_track": {
        "value": 0x8006,
        "icon": "mdi:skip-previous",
    },
    "input_bluetooth": {
        "value": 0x8101,
        "icon": "mdi:bluetooth",
    },
    "input_aux": {
        "value": 0x8102,
        "icon": "mdi:audio-input-stereo-minijack",
    },
    "input_usb": {
        "value": 0x8103,
        "icon": "mdi:usb",
    },
    "bluetooth_pair": {
        "value": 0x8200,
        "icon": "mdi:bluetooth-connect",
    },
    "factory_reset": {
        "value": 0x8300,
        "icon": "mdi:restore-alert",
    },
    "sound_1": {
        "value": 0x8501,
        "icon": "mdi:music-note",
    },
    "sound_2": {
        "value": 0x8502,
        "icon": "mdi:music-note",
    },
    "sound_3": {
        "value": 0x8503,
        "icon": "mdi:music-note",
    },
}


def button_schema(command_name):
    """Create schema for a button with default icon."""
    cmd_info = COMMANDS[command_name]
    return button.button_schema(
        Z407Button,
        icon=cmd_info["icon"],
        device_class=DEVICE_CLASS_EMPTY,
    )


# Build configuration schema with all commands as optional
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_Z407_CONTROLLER_ID): cv.use_id(Z407Controller),
    }
)

# Add each command as an optional button
for cmd_name in COMMANDS:
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
        {
            cv.Optional(cmd_name): button_schema(cmd_name),
        }
    )


async def to_code(config):
    """Generate C++ code for button platform."""
    parent = await cg.get_variable(config[CONF_Z407_CONTROLLER_ID])
    
    # Create button for each configured command
    for cmd_name, cmd_info in COMMANDS.items():
        if cmd_name in config:
            conf = config[cmd_name]
            btn = await button.new_button(conf)
            await cg.register_component(btn, conf)
            
            cg.add(btn.set_parent(parent))
            # Cast integer to Z407Command enum
            cg.add(btn.set_command(cg.RawExpression(f"static_cast<esphome::z407_controller::Z407Command>({cmd_info['value']})")))



"""
Z407 Controller Component for ESPHome

This component provides control of Logitech Z407 Bluetooth speakers via BLE.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
)
from esphome import automation

# Define constants
CONF_BLE_CLIENT_ID = "ble_client_id"
CONF_DISCOVERY_MODE = "discovery_mode"
CONF_DISCOVERY_DURATION = "discovery_duration"
CONF_CONNECTION_TIMEOUT = "connection_timeout"
CONF_AUTO_RECONNECT = "auto_reconnect"
CONF_ON_STATE = "on_state"
CONF_ON_INPUT_CHANGE = "on_input_change"

# Dependencies
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = []
MULTI_CONF = False

# Create namespace
z407_controller_ns = cg.esphome_ns.namespace("z407_controller")

# Define main component class
Z407Controller = z407_controller_ns.class_(
    "Z407Controller", cg.Component, ble_client.BLEClientNode
)

# Define enums
Z407State = z407_controller_ns.enum("Z407State")
Z407_STATES = {
    "DISCONNECTED": Z407State.DISCONNECTED,
    "CONNECTING": Z407State.CONNECTING,
    "HANDSHAKING": Z407State.HANDSHAKING,
    "CONNECTED": Z407State.CONNECTED,
    "ERROR": Z407State.ERROR,
}

Z407Input = z407_controller_ns.enum("Z407Input")
Z407_INPUTS = {
    "BLUETOOTH": Z407Input.BLUETOOTH,
    "AUX": Z407Input.AUX,
    "USB": Z407Input.USB,
    "UNKNOWN": Z407Input.UNKNOWN,
}

# Define triggers
Z407StateChangeTrigger = z407_controller_ns.class_(
    "Z407StateChangeTrigger",
    automation.Trigger.template(Z407State),
)

Z407InputChangeTrigger = z407_controller_ns.class_(
    "Z407InputChangeTrigger",
    automation.Trigger.template(Z407Input),
)


def validate_config(config):
    """Validate that either ble_client_id or discovery_mode is set, but not both."""
    has_ble_client = CONF_BLE_CLIENT_ID in config
    has_discovery = config.get(CONF_DISCOVERY_MODE, False)
    
    if has_ble_client and has_discovery:
        raise cv.Invalid(
            "Cannot use both 'ble_client_id' and 'discovery_mode'. "
            "Use 'discovery_mode: true' to find the device MAC address, "
            "then use 'ble_client_id' with the discovered address for normal operation."
        )
    
    if not has_ble_client and not has_discovery:
        raise cv.Invalid(
            "Must specify either 'ble_client_id' or 'discovery_mode: true'. "
            "Use 'discovery_mode: true' first to discover the device MAC address."
        )
    
    return config


# Configuration schema
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Z407Controller),
            cv.Optional(CONF_BLE_CLIENT_ID): cv.use_id(ble_client.BLEClient),
            cv.Optional(CONF_DISCOVERY_MODE, default=False): cv.boolean,
            cv.Optional(
                CONF_DISCOVERY_DURATION, default="30s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_CONNECTION_TIMEOUT, default="30s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_AUTO_RECONNECT, default=True): cv.boolean,
            cv.Optional(CONF_ON_STATE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        Z407StateChangeTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_INPUT_CHANGE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        Z407InputChangeTrigger
                    ),
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    validate_config,
)


async def to_code(config):
    """Generate C++ code for the component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # Set discovery mode or BLE client
    if config.get(CONF_DISCOVERY_MODE, False):
        cg.add(var.set_discovery_mode(True))
        cg.add(
            var.set_discovery_duration(config[CONF_DISCOVERY_DURATION])
        )
    else:
        # Register with BLE client
        parent = await cg.get_variable(config[CONF_BLE_CLIENT_ID])
        cg.add(var.set_ble_client(parent))
        cg.add(parent.register_ble_node(var))
    
    # Set optional configuration
    if CONF_CONNECTION_TIMEOUT in config:
        cg.add(var.set_connection_timeout(config[CONF_CONNECTION_TIMEOUT]))
    
    if CONF_AUTO_RECONNECT in config:
        cg.add(var.set_auto_reconnect(config[CONF_AUTO_RECONNECT]))
    
    # Set up automations
    for conf in config.get(CONF_ON_STATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(Z407State, "state")], conf)
    
    for conf in config.get(CONF_ON_INPUT_CHANGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(Z407Input, "input")], conf)


# Actions
@automation.register_action(
    "z407_controller.send_command",
    automation.Action,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(Z407Controller),
            cv.Required("command"): cv.templatable(cv.hex_uint16_t),
        }
    ),
)
async def z407_send_command_to_code(config, action_id, template_arg, args):
    """Generate code for send_command action."""
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    
    template_ = await cg.templatable(config["command"], args, cg.uint16)
    cg.add(var.set_command(template_))
    return var


@automation.register_action(
    "z407_controller.set_input",
    automation.Action,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(Z407Controller),
            cv.Required("input"): cv.templatable(
                cv.enum(Z407_INPUTS, upper=True)
            ),
        }
    ),
)
async def z407_set_input_to_code(config, action_id, template_arg, args):
    """Generate code for set_input action."""
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    
    template_ = await cg.templatable(config["input"], args, Z407Input)
    cg.add(var.set_input(template_))
    return var


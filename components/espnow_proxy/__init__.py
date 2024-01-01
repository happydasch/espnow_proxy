from esphome.cpp_generator import Expression, SafeExpType, safe_exp
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_TRIGGER_ID,
)

CONF_ESPNowProxy_ID = "ESPNowProxy_ID"

CONF_PEERS = "peers"

CONF_NAME_PREFIX = "name_prefix"

CONF_ON_PACKET_DATA = "on_packet_data"
CONF_ON_COMMAND_DATA = "on_command_data"

CONF_ON_SEND_STARTED = "on_send_started"
CONF_ON_SEND_FINISHED = "on_send_finished"
CONF_ON_SEND_FAILED = "on_send_failed"

CONF_COMPLETE_ONLY = "complete_only"  # for send action

DEPENDENCIES = ["logger", "wifi"]
AUTO_LOAD = []

base_ns = cg.global_ns.namespace("espnow_proxy_base")
proxy_ns = cg.esphome_ns.namespace("espnow_proxy")

ESPNowProxyPeer = proxy_ns.class_("ESPNowProxyPeer", cg.Component)
PacketDataTrigger = proxy_ns.class_("PacketDataTrigger", automation.Trigger.template())
CommandDataTrigger = proxy_ns.class_("CommandDataTrigger", automation.Trigger.template())

SendStartedTrigger = proxy_ns.class_("SendStartedTrigger", automation.Trigger.template())
SendFinishedTrigger = proxy_ns.class_("SendFinishedTrigger", automation.Trigger.template())
SendFailedTrigger = proxy_ns.class_("SendFailedTrigger", automation.Trigger.template())

PacketData = proxy_ns.struct("packet_data_t")


class ExplicitClassPtrCast(Expression):
    __slots__ = ("classop", "xhs")

    def __init__(self, classop: SafeExpType, xhs: SafeExpType):
        self.classop = safe_exp(classop).operator("ptr")
        self.xhs = safe_exp(xhs)

    def __str__(self):
        # Surround with parentheses to ensure generated code has same
        # order as python one
        return f"({self.classop})({self.xhs})"


class PeerStorage:
    peer_ = {}
    mac_address_ = {}
    name_prefix_ = {}

    def __init__(self, peer_, mac_address_, name_prefix_):
        self.peer_ = peer_
        self.mac_address_ = mac_address_
        self.name_prefix_ = name_prefix_

    def get_peer(self):
        return self.peer_

    def get_mac_address(self):
        return self.mac_address_

    def get_name_prefix(self):
        return self.name_prefix_


class Generator:
    proxy_ = {}
    proxy_id_ = {}
    peers_by_addr_ = {}

    event_schema = cv.Schema({
        cv.Optional(CONF_ON_PACKET_DATA): automation.validate_automation({
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(PacketDataTrigger),
        }),
        cv.Optional(CONF_ON_COMMAND_DATA): automation.validate_automation({
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CommandDataTrigger),
        }),
        cv.Optional(CONF_ON_SEND_STARTED): automation.validate_automation({
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SendStartedTrigger),
        }),
        cv.Optional(CONF_ON_SEND_FINISHED): automation.validate_automation({
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SendFinishedTrigger),
        }),
        cv.Optional(CONF_ON_SEND_FAILED): automation.validate_automation({
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SendFailedTrigger),
        }),
    })

    def __init__(self, proxy_id):
        self.proxy_id_ = proxy_id

    def proxy_factory(self):
        return proxy_ns.class_("ESPNowProxy", cg.Component)

    def peer_class_factory(self):
        return ESPNowProxyPeer

    def get_receiver(self):
        if not self.proxy_:
            self.proxy_ = self.proxy_factory()
        return self.proxy_

    def generate_proxy_config(self):
        schema = self.generate_proxy_schema()
        return schema, self.to_code

    def generate_peer_schema(self):
        return cv.Schema({
            cv.GenerateID(): cv.declare_id(self.peer_class_factory()),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_NAME_PREFIX): cv.string
        }).extend(self.event_schema)

    def generate_proxy_schema(self):
        schema = cv.Schema({
            cv.GenerateID(): cv.declare_id(self.get_receiver()),
            cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_PEERS): cv.ensure_list(
                self.generate_peer_schema()
            )
        }).extend(self.event_schema).extend(cv.COMPONENT_SCHEMA)
        return schema

    async def to_code_automations(self, config, var):
        for conf in config.get(CONF_ON_PACKET_DATA, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)

            if CONF_MAC_ADDRESS in config:
                cg.add(trigger.set_peer_address(config[CONF_MAC_ADDRESS].as_hex))

            await automation.build_automation(
                trigger,
                [
                    (cg.uint64.operator("const"), "address"),
                    (PacketData.operator("const"), "x"),
                ],
                conf,
            )

        for conf in config.get(CONF_ON_COMMAND_DATA, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)

            if CONF_MAC_ADDRESS in config:
                cg.add(trigger.set_peer_address(config[CONF_MAC_ADDRESS].as_hex))

            await automation.build_automation(
                trigger,
                [
                    (cg.uint64.operator("const"), "address"),
                    (cg.std_string.operator("const"), "x"),
                ],
                conf,
            )

        for conf in config.get(CONF_ON_SEND_STARTED, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await automation.build_automation(trigger, [], conf)

        for conf in config.get(CONF_ON_SEND_FINISHED, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await automation.build_automation(trigger, [], conf)

        for conf in config.get(CONF_ON_SEND_FAILED, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await automation.build_automation(trigger, [], conf)

    async def to_code_peer(self, component, config, ID_PROP):
        # add to peer registry
        mac_address_str = str(config[CONF_MAC_ADDRESS])
        if mac_address_str not in self.peers_by_addr_:
            var = cg.Pvariable(
                config[ID_PROP],
                ExplicitClassPtrCast(
                    self.peer_class_factory(),
                    component.set_peer(config[CONF_MAC_ADDRESS].as_hex),
                ),
            )
            await cg.register_component(var, config)
            name_prefix_str = (
                str(config[CONF_NAME_PREFIX]
                    ) if CONF_NAME_PREFIX in config else None
            )
            peer = PeerStorage(var, mac_address_str, name_prefix_str)
            self.peers_by_addr_[mac_address_str] = peer
        else:
            peer = self.peers_by_addr_[mac_address_str]
            var = peer.get_peer()

        if CONF_NAME_PREFIX in config:
            cg.add(var.set_name_prefix(config[CONF_NAME_PREFIX]))

        await self.to_code_automations(config, var)

        return peer

    async def to_code(self, config):
        var = cg.new_Pvariable(config[CONF_ID])

        if CONF_MAC_ADDRESS in config:
            cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
        await cg.register_component(var, config)

        if CONF_PEERS in config:
            for _, config_item in enumerate(config[CONF_PEERS]):
                await self.to_code_peer(var, config_item, CONF_ID)

        await self.to_code_automations(config, var)

        return var


gen = Generator(CONF_ESPNowProxy_ID)
CONFIG_SCHEMA, to_code = gen.generate_proxy_config()

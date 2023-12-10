#pragma once

#include <vector>
#include <map>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"

#include "espnow_proxy.h"

namespace esphome {
namespace espnow_proxy {

    using namespace espnow_proxy_base;

    class PacketDataTrigger : public Trigger<const mac_address_t, const packet_data_t> {
        public:
            explicit PacketDataTrigger(EventTarget *parent) {
                parent->add_on_packet_data_callback([this](const mac_address_t address, const packet_data_t x) {
                    // filter packet on address if peer
                    if ((!peer_address_ || peer_address_ == address)) {
                        trigger(address, x);
                    }
                });
            }
            void set_peer_address(mac_address_t value) { peer_address_ = value; };

        private:
            mac_address_t peer_address_{0};
    };

    class CommandDataTrigger : public Trigger<const mac_address_t, const std::string> {
        public:
            explicit CommandDataTrigger(EventTarget *parent) {
                parent->add_on_command_data_callback([this](const mac_address_t address, const std::string x) {
                    // filter command on address if peer
                    if ((!peer_address_ || peer_address_ == address)) {
                        trigger(address, x);
                    }
                });
            }
            void set_peer_address(mac_address_t value) { peer_address_ = value; };

        private:
            mac_address_t peer_address_{0};
    };

    class SendStartedTrigger : public Trigger<> {
        public:
            explicit SendStartedTrigger(ESPNowProxy *parent) {
                parent->add_on_send_started_callback(
                    [this](){ trigger(); }
                );
            }
    };

    class SendFinishedTrigger : public Trigger<> {
        public:
            explicit SendFinishedTrigger(ESPNowProxy *parent) {
                parent->add_on_send_finished_callback(
                    [this](){ trigger(); }
                );
            }
    };

    class SendFailedTrigger : public Trigger<> {
        public:
            explicit SendFailedTrigger(ESPNowProxy *parent) {
                parent->add_on_send_failed_callback(
                    [this](){ trigger(); }
                );
            }
    };

}  // namespace espnow_proxy
}  // namespace esphome
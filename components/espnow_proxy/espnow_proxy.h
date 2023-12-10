#pragma once

#include <vector>
#include <map>
#include <deque>

#include "esphome/core/log.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"

#include "base.h"

namespace esphome {
namespace espnow_proxy {

    using namespace espnow_proxy_base;

    class EventTarget {

        public:
            CallbackManager<void(const mac_address_t, const packet_data_t)> on_packet_data_callback;
            CallbackManager<void(const mac_address_t, const std::string)> on_command_data_callback;
            CallbackManager<void()> on_send_started_callback;
            CallbackManager<void()> on_send_finished_callback;
            CallbackManager<void()> on_send_failed_callback;

            void add_on_packet_data_callback(std::function<void(const mac_address_t, const packet_data_t)> callback) {
                on_packet_data_callback.add(std::move(callback));
            }

            void add_on_command_data_callback(std::function<void(const mac_address_t, const std::string)> callback) {
                on_command_data_callback.add(std::move(callback));
            }

            void add_on_send_started_callback(std::function<void()> callback) {
                on_send_started_callback.add(std::move(callback));
            }

            void add_on_send_finished_callback(std::function<void()> callback) {
                on_send_finished_callback.add(std::move(callback));
            }

            void add_on_send_failed_callback(std::function<void()> callback) {
                on_send_failed_callback.add(std::move(callback));
            }
    };

    class ESPNowProxyBase: public Component, public EventTarget {

        public:
            mac_address_t get_address() { return address_; };
            void set_address(mac_address_t address) { address_ = address; };

            uint8_t get_channel() { return channel_; }
            void set_channel(uint8_t channel) { channel_ = channel; }

        protected:
            mac_address_t address_{0};
            uint8_t channel_{0};
    };

    class ESPNowProxyPeer: public ESPNowProxyBase {

        private:
            std::string name_prefix_{};

        public:
            ESPNowProxyPeer(mac_address_t address) { set_address(address); }

            std::string get_name_prefix() { return name_prefix_; };
            void set_name_prefix(std::string value) { name_prefix_ = value; };

    };

    class ESPNowProxy : public ESPNowProxyBase {

        #define MAX_SEND_QUEUE_LEN 5
        #define MAX_SEND_RETRIES 10

        private:
            // peers
            std::map<mac_address_t, ESPNowProxyPeer *> peers_;

            //  queues
            std::deque<recv_data_t *> *recv_queue_ = new std::deque<recv_data_t *>();
            std::deque<send_data_t *> *send_queue_ = new std::deque<send_data_t *>();
            std::deque<send_data_t *> *send_ack_queue_ = new std::deque<send_data_t *>();

            // packet
            send_data_t *send_data_;
            uint8_t last_packet_id_{0};

            // basic functions
            void setup_wifi_();

            // peers functions
            ESPNowProxyPeer *create_peer_(const mac_address_t address);
            ESPNowProxyPeer *get_peer_by_mac_address_(const mac_address_t address);

            // send / recv functions
            void on_send_(const uint8_t *addr, esp_now_send_status_t status);
            void on_recv_(const uint8_t *addr, const uint8_t *data, int size);

        public:
            bool send(const char *data);
            void setup() override;
            void loop() override;
            void dump_config() override;
            float get_setup_priority() const override { return setup_priority::WIFI; }
            ESPNowProxyPeer *set_peer(mac_address_t address);

        protected:
            void pre_process_queues_();
            bool process_recv_queue_();
            bool process_send_queue_();

    };

}  // namespace espnow_proxy
}  // namespace esphome
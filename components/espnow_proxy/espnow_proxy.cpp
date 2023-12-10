#include "espnow_proxy.h"

//FIXME #include "esphome/components/wifi/wifi_component.h"

namespace esphome {
namespace espnow_proxy {

    static const char *const TAG = "espnow_proxy";
    #define GLOBAL_PACKET_ID_PREFS_ID 127671120UL

    // internal

    void ESPNowProxy::setup_wifi_() {

        espnow_proxy_base::begin(channel_);

        if (!espnow_proxy_base::is_ready()) {
            ESP_LOGW(TAG, "ESPNow not ready, will try later to begin");
        }

        // disables wifi component
        //FIXME should the default component be disabled? ap needs to be setup then. wifi::global_wifi_component->mark_failed();

        // ensure receiver address is in peers
        uint8_t *receiver;
        int netif = ESP_IF_WIFI_STA;
        int peer_netif = ESP_IF_WIFI_AP;
        if (address_) {
            receiver = addr64_to_addr(address_);
        } else {
            receiver = (uint8_t *)espnow_proxy_base::BROADCAST;
            netif = ESP_IF_WIFI_AP;
            peer_netif = ESP_IF_WIFI_STA;
        }
        if (!espnow_proxy_base::has_peer(receiver)) {
            espnow_proxy_base::add_peer(receiver, 0, netif);
        }

        // add peers
        for (auto it = peers_.begin(); it != peers_.end(); ++it) {
            uint8_t *address = addr64_to_addr(it->first);
            if (!espnow_proxy_base::has_peer(address)) {
                espnow_proxy_base::add_peer(address, 0, peer_netif);
            }
        }

    }

    ESPNowProxyPeer *ESPNowProxy::create_peer_(const mac_address_t address) {

        return new ESPNowProxyPeer(address);

    }

    ESPNowProxyPeer *ESPNowProxy::get_peer_by_mac_address_(const mac_address_t address) {
        ESP_LOGD(TAG, "looking for address %s", addr64_to_str(address).c_str());
        // address match
        if (peers_.find(address) != peers_.end()) {
            return peers_[address];
        }
        // softap address match
        uint8_t *base_address = addr64_to_addr(address);
        base_address[MAC_ADDRESS_LEN-1]--;
        mac_address_t temp_address = addr_to_addr64(base_address);
        if (peers_.find(temp_address) != peers_.end()) {
            if (!espnow_proxy_base::has_peer(addr64_to_addr(address))) {
                ESP_LOGW(TAG, "Adding softAP peer %s", addr64_to_str(address).c_str());
                espnow_proxy_base::add_peer(addr64_to_addr(address), 0);
            }
            return peers_[temp_address];
        }
        // no match
        return nullptr;
    }

    // callback handler

    void ESPNowProxy::on_send_(const uint8_t *addr, esp_now_send_status_t status) {

        if (!send_data_) {
            ESP_LOGD(TAG, "Send callback called without send data");
            return;
        }

        ESP_LOGD(TAG, "Message send status %d", status);

        if (status == ESP_NOW_SEND_SUCCESS) {

            last_packet_id_++;
            send_data_->sent = true;
            this->on_send_finished_callback.call();

        } else {

            ESP_LOGW(TAG, "Message send failed, moving back to send queue");
            send_data_->sent = false;
            this->on_send_failed_callback.call();

        }
        send_queue_->push_back(send_data_);
        send_data_ = nullptr;
    }

    void ESPNowProxy::on_recv_(const uint8_t *addr, const uint8_t *data, int size) {

        // create received data, this needs to be freed later
        recv_data_t *received = (recv_data_t *)malloc(sizeof(recv_data_t));
        memcpy((uint8_t *)received->data.raw, (uint8_t *)data, size);
        memcpy((uint8_t *)received->addr, (uint8_t *)addr, MAC_ADDRESS_LEN);
        received->size = size;

        // add received data to queue
        recv_queue_->push_back(received);

    }

    // public functions

    bool ESPNowProxy::send(const char *data) {

        ESP_LOGD(TAG, "Add send command to queue, queue size: %d", send_queue_->size());
        if (send_queue_->size() >= MAX_SEND_QUEUE_LEN) {
            ESP_LOGW(TAG, "Send command queue full, dropping command");
            return false;
        }

        // create send data for later processing, this needs later to be freed
        send_data_t *send = (send_data_t *)malloc(sizeof(send_data_t));
        memcpy(send->data, data, MAX_PAYLOAD_LENGTH);
        send->size = sizeof(data);
        if (address_) {
            send->address = address_;
        } else {
            send->address = addr_to_addr64(espnow_proxy_base::BROADCAST);
        }
        send->time = 0;
        send->retries = 0;
        send->packet_id = 0;
        send->sent = false;

        // add send data to queue
        send_queue_->push_back(send);

        return true;

    }

    void ESPNowProxy::setup() {

        // setup callbacks (send/recv)
        espnow_proxy_base::add_send_callback(
            [&](const uint8_t *addr, esp_now_send_status_t status) { on_send_(addr, status); }
        );
        espnow_proxy_base::add_recv_callback(
            [&](const uint8_t *addr, const uint8_t *data, int size) { on_recv_(addr, data, size); }
        );

        // prepare connection
        setup_wifi_();

    }

    void ESPNowProxy::loop() {

        if (!espnow_proxy_base::is_ready()) {
            ESP_LOGD(TAG, "ESPNow not ready, trying to start in loop");
            setup_wifi_();
        } else if (espnow_proxy_base::is_sending()) {
            return;
        }

        pre_process_queues_();
        if (process_recv_queue_()) {
            return;
        }
        if (process_send_queue_()) {
            return;
        }

    }

    void ESPNowProxy::dump_config() {

        ESP_LOGCONFIG(TAG, "ESPNowProxy...");
        ESP_LOGCONFIG(TAG, "  Connection State: %d", espnow_proxy_base::is_ready());
        ESP_LOGCONFIG(TAG, "  Network Channel: %d", channel_);
        if (address_) {
            ESP_LOGCONFIG(TAG, "  Receiver Address: %s", addr64_to_str(get_address()).c_str());
        } else {
            ESP_LOGCONFIG(TAG, "  Receiver Broadcast");
        }

        // peers configured
        ESP_LOGCONFIG(TAG, "  Peers:");
        for (auto it = peers_.begin(); it != peers_.end(); ++it) {
            mac_address_t address = it->first;
            ESPNowProxyPeer* peer = it->second;
            ESP_LOGCONFIG(
                TAG, "    Peer %s - address: %s",
                peer->get_name_prefix().c_str(),
                addr64_to_str(peer->get_address()).c_str());
        }

        // esp now peers
        esp_now_peer_num_t num_peers;
        if (esp_now_get_peer_num(&num_peers) != ESP_OK) {
            return;
        }
        esp_now_peer_info_t peers[num_peers.total_num];
        int total = espnow_proxy_base::list_peers((esp_now_peer_info_t *)peers, num_peers.total_num);
        ESP_LOGCONFIG(TAG, "  ESPNow Peers:");
        for (auto idx = 0; idx < total; idx++) {
            if (idx >= MAX_PEERS) {
                break;
            }
            esp_now_peer_info_t peer = peers[idx];
            mac_address_t address = espnow_proxy_base::addr_to_addr64(peer.peer_addr);
            ESP_LOGCONFIG(
                TAG, "    ESPNow Peer - address: %s channel: %d ifidx: %d%s",
                addr_to_str(peer.peer_addr).c_str(),
                peer.channel,
                peer.ifidx,
                address_ == address ? " <- Receiver" : "");
        }
        ESP_LOGCONFIG(TAG, "    ... total (%d)", total);

    }

    ESPNowProxyPeer *ESPNowProxy::set_peer(mac_address_t address) {

        if (sizeof(peers_) < MAX_PEERS) {
            ESP_LOGW(TAG, "Max peers already set");
        }
        auto peer = create_peer_(address);
        peers_.emplace(address, peer);

        return peer;

    }

    //
    // queues
    //

    void ESPNowProxy::pre_process_queues_() {

        // check exisiting queue items for invalidity
        uint32_t current = millis();
        for (auto it = send_queue_->begin(); it != send_queue_->end();) {
            send_data_t *item = *it;
            bool keep = true;

            // check for retries
            if (item->retries >= MAX_SEND_RETRIES) {

                // too many retries, remove item from queue
                ESP_LOGW(TAG, "Too many retries for sending to %s", addr64_to_str(item->address).c_str());
                keep = false;

            // check for timeout
            } else if (item->time && current > item->time + SEND_TIMEOUT_MS) {

                // timeout, remove item from queue
                ESP_LOGW(
                    TAG, "Timeout occurred waiting for send ack from %s queue size: %d (%d / %d)",
                    addr64_to_str(item->address).c_str(),
                    send_queue_->size(),
                    current,
                    item->time);
                keep = false;

            }

            // decide what to do with item
            if (!keep) {

                // do not keep item in queue, free
                it = send_queue_->erase(it);
                free(item);

            } else {

                // keep item in queue
                ++it;

            }
        }

    }

    bool ESPNowProxy::process_send_queue_() {
        if (send_queue_->empty() || send_data_) {
            return false;
        }

        /*send_data_t *message = send_queue_->front();
        if (message->sent) {  // while waiting for a response, don't send new messages
            message = nullptr;
        }*/
        // get message that was not sent yet from queue
        send_data_t *message = nullptr;
        for (auto it = send_queue_->begin(); it != send_queue_->end(); ) {
            send_data_t *item = *it;
            if (!item->sent) {
                it = send_queue_->erase(it);
                message = item;
                break;
            }
            ++it;
        }

        if (!message) {
            return false;
        }

        // a message was found, update send attempts
        message->retries++;

        // log message details
        ESP_LOGD(TAG, "Using message %s: %s (%d / %d)", addr64_to_str(message->address).c_str(), message->data, message->retries, MAX_SEND_RETRIES);

        // send message
        if (message->time == 0) {
            message->time = millis();
        }
        message->packet_id = last_packet_id_;
        send_data_ = message;
        if (send_command_data(addr64_to_addr(message->address), message->data, message->size, message->packet_id)) {

            // the message was sent and will be processed later
            // after success in on_send_
            this->on_send_started_callback.call();
            ESP_LOGD(TAG, "Message sent, further processing in on_send_");
            // store in send_data_ so in on_send_ it can be processed

        } else {

            // the message was not sent, it will not be processed
            // in on_send_
            ESP_LOGD(TAG, "Message not sent, moving back to queue");
            // put back into send queue if sending was unsuccessful
            send_queue_->push_back(message);
            send_data_ = nullptr;

        }

        return true;

    }

    bool ESPNowProxy::process_recv_queue_() {

        if (recv_queue_->size() == 0) {
            return false;
        }

        recv_data_t *message = recv_queue_->front();
        recv_queue_->pop_front();

        auto header = message->data.command_header;
        auto packet_id = header.packet_id;

        auto client_addr_a64 = addr_to_addr64(message->addr);
        auto peer = static_cast<ESPNowProxyPeer*>(get_peer_by_mac_address_(client_addr_a64));

        Command_e command = get_command(message->data.raw, message->size);

        // Log command details
        ESP_LOGD(
            TAG, "Recv command: 0x%02x, (size: %d), from: %s, peer: %s",
            command,
            message->size,
            addr_to_str(message->addr).c_str(),
            peer ? peer->get_name_prefix().c_str() : "<unknown>");


        // only if peer is found continue
        if (!peer) {
            ESP_LOGW(TAG, "Peer not found, ignoring command");
            free(message);
            return true;
        }

        // use peer address for responses
        auto peer_addr_a64 = peer->get_address();
        on_packet_data_callback.call(peer_addr_a64, message->data);
        peer->on_packet_data_callback.call(peer_addr_a64, message->data);

        // process command received
        switch (command) {
            case Command_None:
                break;

            case Command_Data:
                {
                    command_data_t command_data = message->data.command_data;
                    ESP_LOGD(TAG, "Received Data from %s: %s", addr_to_str(message->addr).c_str(), (char *)command_data.data);
                    const std::string data = (char *)command_data.data;
                    on_command_data_callback.call(peer_addr_a64, data);
                    peer->on_command_data_callback.call(peer_addr_a64, data);
                    ESP_LOGD(TAG, "Sending DataAck to %s (%d)", addr64_to_str(peer->get_address()).c_str(), packet_id);
                    send_command_data_ack(addr64_to_addr(peer->get_address()), packet_id, message->data.command_header.packet_id);
                }
                break;

            case Command_DataAck:
                {
                    command_data_ack_t command_data_ack = message->data.command_data_ack;
                    ESP_LOGD(TAG, "Received DataAck from %s packet_id: %d", addr_to_str(message->addr).c_str(), command_data_ack.packet_id_acked);
                    for (auto it = send_queue_->begin(); it != send_queue_->end(); ) {
                        send_data_t* item = *it;
                        if (item->sent == true && item->packet_id == command_data_ack.packet_id_acked) {
                            // packet confirmed, sent message sent and confirmed
                            ESP_LOGD(TAG, "Packet %d confirmed, message sent and confirmed", command_data_ack.packet_id_acked);
                            it = send_queue_->erase(it);
                            free(item);
                            break;

                        } else {

                            // not the packet id, next item
                            ESP_LOGD(TAG, "Packet %d (%d) not confirmed, next item", command_data_ack.packet_id_acked, item->packet_id);
                            ++it;
                        }
                    }
                }
                break;

        }

        // free recv message
        ESP_LOGD(TAG, "Freeing recv message");
        free(message);
        return true;

    }

}  // namespace espnow_proxy
}  // namespace esphome
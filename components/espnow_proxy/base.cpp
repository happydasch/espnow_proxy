#include "base.h"

namespace esphome {
namespace espnow_proxy_base {

    static const char *TAG = "espnow_proxy_base";

    send_callback_t send_callbacks_[MAX_CALLBACKS];
    recv_callback_t recv_callbacks_[MAX_CALLBACKS];
    uint8_t send_callback_idx_ = 0;
    uint8_t recv_callback_idx_ = 0;
    State state_;

    // internal

    void set_sending_(bool sending) {
        state_.is_sending = sending;
    }

    void set_success_(bool success) {
        state_.is_success = success;
    }

    void inc_sent_error_() {
        state_.sent_error++;
    }

    void inc_received_() {
        state_.received++;
    }

    void inc_sent_() {
        state_.sent++;
    }

    void set_sender_(uint8_t *sender) {
        state_.sender = sender;
    }

    void set_send_time_(uint32_t time) {
        state_.send_time = time;
    }

    uint16_t calc_duration_(uint32_t start_time) {
        return micros() - start_time;
    }

    // peers

    bool add_peer(const uint8_t *peer, int channel, int netif) {
        if (!is_ready()) {
            return false;
        }

        esp_now_peer_info_t peer_info{};
        std::copy_n(peer, MAC_ADDRESS_LEN, peer_info.peer_addr);

        peer_info.channel = static_cast<uint8_t>(channel);
        peer_info.ifidx = static_cast<wifi_interface_t>(netif);

        if (has_peer(peer)) {
            ESP_LOGD(TAG, "Peer %s already exists", addr_to_str(peer).c_str());
            return esp_now_mod_peer(&peer_info) == ESP_OK;
        }
        ESP_LOGD(TAG, "Adding peer %s", addr_to_str(peer).c_str());
        return esp_now_add_peer(&peer_info) == ESP_OK;
    }

    bool has_peer(const uint8_t *peer) {
        return is_ready() && esp_now_is_peer_exist(peer);
    }

    bool remove_peer(const uint8_t *peer) {
        return is_ready() && esp_now_del_peer(peer) == ESP_OK;
    }

    int list_peers(esp_now_peer_info_t* peers, int max_peers) {
        if (!is_ready()) {
            return 0;
        }
        int total = 0;
        esp_now_peer_info_t peer;
        for (
            esp_err_t e = esp_now_fetch_peer(true, &peer);
            e == ESP_OK;
            e = esp_now_fetch_peer(false, &peer)
        ) {
            uint8_t* mac = peer.peer_addr;
            uint8_t channel = peer.channel;
            if (total < max_peers) {
                memcpy(&peers[total], &peer, sizeof(esp_now_peer_info_t));
            }
            ++total;
        }
        return total;
    }

    // public functions

    bool add_send_callback(send_callback_t callback) {
        if (send_callback_idx_ == MAX_CALLBACKS - 1) {
            return false;
        }
        send_callbacks_[send_callback_idx_++] = callback;
        return true;
    }

    bool add_recv_callback(recv_callback_t callback) {
        if (recv_callback_idx_ == MAX_CALLBACKS - 1) {
            return false;
        }
        recv_callbacks_[recv_callback_idx_++] = callback;
        return true;
    }

    bool send(uint8_t *dest, uint8_t *data, size_t size) {
        ESP_LOGD(TAG, "Send handler begin: %s", addr_to_str(dest).c_str());
        set_sending_(true);
        set_send_time_(micros());
        set_success_(false);

        if (has_peer(dest)) {
            if (esp_now_send(dest, data, size) != ESP_OK) {
                set_success_(false);
            } else {
                set_success_(true);
            }
        } else {
            ESP_LOGW(TAG, "Unknown peer: %s", addr_to_str(dest).c_str());
            set_sending_(false);
            return false;
        }

        ESP_LOGD(TAG, "Send handler finished: %d", is_success());
        return is_success();
    }

    void begin(uint8_t channel) {
        if (is_ready()) {
            end();
        }

        /*ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
        ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
        ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_APSTA) );
        ESP_ERROR_CHECK( esp_wifi_start());
        ESP_ERROR_CHECK( esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));*/

        uint8_t current_channel = WiFi.channel();
        ESP_LOGD(TAG, "Begin: %d (current channel %d)", channel, current_channel);
        WiFi.mode(WIFI_AP_STA);
        ESP_LOGD(TAG, "Begin: change channel to %d", channel);
        WiFi.channel(channel);
        state_.is_ready = false;
        if (esp_now_init() == ESP_OK) {
            ESP_LOGD(TAG, "Begin: init done");
            ESP_ERROR_CHECK(esp_now_register_send_cb(send_handler));
            ESP_ERROR_CHECK(esp_now_register_recv_cb(recv_handler));
            state_.is_ready = true;
        } else {
            ESP_LOGW(TAG, "Begin: esp_now_init failed");
        }
        ESP_LOGI(TAG, "Begin: finished");
    }

    void end() {
        if (!is_ready()) {
            return;
        }
        WiFi.disconnect(true, false);
        ESP_LOGD(TAG, "End: deinit call");
        esp_now_deinit();
        ESP_LOGD(TAG, "End: unregister recv cb call");
        esp_now_unregister_recv_cb();
        ESP_LOGD(TAG, "base setup_wifi: unregister send cb call");
        esp_now_unregister_send_cb();
        state_.is_ready = false;
        ESP_LOGD(TAG, "End: finished");
    }

    // status

    bool is_success() {
        return state_.is_success;
    }

    bool is_ready() {
        return state_.is_ready;
    }

    bool is_sending() {
        return state_.is_sending;
    }

    uint8_t *sender() {
        return state_.sender;
    }

    State get_state() {
        return state_;
    }

    // callbacks

    void send_handler(const uint8_t *addr, esp_now_send_status_t status) {
        if (status == ESP_NOW_SEND_SUCCESS) {
            set_success_(true);
            inc_sent_();
        } else {
            set_success_(false);
            inc_sent_error_();
        }
        state_.duration = calc_duration_(state_.send_time);
        set_sending_(false);
        // run callbacks
        for (auto i = 0; i < send_callback_idx_; i++) {
            if (send_callbacks_[i]) {
                send_callbacks_[i](addr, status);
            }
        }
    }

    void recv_handler(const uint8_t *addr, const uint8_t *data, int size) {
        // addr may be different then base address, see the link below for details.
        // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/misc_system_api.html
        set_sender_((uint8_t *)addr);  // store the sender of the last message received
        inc_received_();
        // run callbacks
        for (auto i = 0; i < recv_callback_idx_; i++) {
            if (recv_callbacks_[i]) {
                recv_callbacks_[i](addr, data, size);
            }
        }
    }

}  // namespace espnow_proxy_base
}  // namespace esphome
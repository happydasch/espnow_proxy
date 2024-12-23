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
            ESP_LOGW(TAG, "Attempted to add peer when not ready.");            
            return false;
        }
        if (has_peer(peer)) {
            ESP_LOGD(TAG, "Peer %s already exists", addr_to_str(peer).c_str());
            return false;
        }
        ESP_LOGD(TAG, "Adding peer %s", addr_to_str(peer).c_str());
        
#ifdef ESP32
        esp_now_peer_info_t peer_info{};
        std::copy_n(peer, MAC_ADDRESS_LEN, peer_info.peer_addr);
        peer_info.channel = static_cast<uint8_t>(channel);
        peer_info.ifidx = static_cast<wifi_interface_t>(netif);
        return esp_now_add_peer(&peer_info) == ESP_OK;
#elif ESP8266
        uint8_t peer_addr[MAC_ADDRESS_LEN];
        std::copy_n(peer, MAC_ADDRESS_LEN, peer_addr);        
        return esp_now_add_peer(peer_addr, ESP_NOW_ROLE_COMBO, static_cast<uint8_t>(channel), NULL, 0) == ESP_OK;
#endif        
    }

    bool has_peer(const uint8_t *peer) {
#ifdef ESP32
        return is_ready() && esp_now_is_peer_exist(peer);
#elif ESP8266
        uint8_t peer_addr[MAC_ADDRESS_LEN];
        std::copy_n(peer, MAC_ADDRESS_LEN, peer_addr);        
        return is_ready() && esp_now_is_peer_exist(peer_addr);
#endif
    }

    bool remove_peer(const uint8_t *peer) {
#ifdef ESP32        
        return is_ready() && esp_now_del_peer(peer) == ESP_OK;
#elif ESP8266
        uint8_t peer_addr[MAC_ADDRESS_LEN];
        std::copy_n(peer, MAC_ADDRESS_LEN, peer_addr);        
        return is_ready() && esp_now_del_peer(peer_addr) == ESP_OK;
#endif        
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
            if (esp_now_send(dest, data, size) == ESP_OK) {
                set_success_(true);
            }
        } else {
            if (!add_peer(dest, 0, 0)) {
                ESP_LOGW(TAG, "Unknown peer: %s", addr_to_str(dest).c_str());
            }
        }

        ESP_LOGD(TAG, "Send handler finished: %d", is_success());
        set_sending_(false);
        return is_success();
    }

    void begin() {
        if (is_ready()) {
            end();
        }

        state_.is_ready = false;
        if (esp_now_init() != ESP_OK) {
            ESP_LOGW(TAG, "Begin: esp_now_init failed");
            return;
        }
        ESP_LOGD(TAG, "Begin: init done");
        
        if (esp_now_register_send_cb(send_handler) != ESP_OK) {
            ESP_LOGW(TAG, "Begin: esp_now_register_send_cb failed");
            return;
        }
        if (esp_now_register_recv_cb(send_handler) != ESP_OK) {
            ESP_LOGW(TAG, "Begin: esp_now_register_recv_cb failed");
            return;
        }
        
        state_.is_ready = true;
        ESP_LOGI(TAG, "Begin: finished");
    }

    void end() {
        if (!is_ready()) {
            return;
        }
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

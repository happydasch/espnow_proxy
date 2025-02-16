#pragma once

#include <functional>

#include "esphome/core/log.h"


#ifdef ESP32
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_log.h>
#elif ESP8266
#include <c_types.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
typedef uint8_t esp_now_send_status_t;
typedef esp_now_send_status_t uint8_t;
static const int ESP_IF_WIFI_STA = 1;
static const int ESP_OK = 0;
static const int ESP_NOW_SEND_SUCCESS = 0;
#endif

#include <Arduino.h>

#include "common.h"
#include "send.h"

namespace esphome {
namespace espnow_proxy_base {

    //using command_callback_t = std::function<void(const uint8_t, const uint8_t *, const int)>;
    using send_callback_t = std::function<void(const uint8_t *, esp_now_send_status_t)>;
    using recv_callback_t = std::function<void(const uint8_t *, const uint8_t *, int)>;

    struct State {
        bool is_ready = false;
        bool is_sending = false;
        bool is_success = false;
        uint32_t sent_error = 0;
        uint32_t sent = 0;
        uint32_t received = 0;
        uint32_t send_time = 0;
        uint16_t duration = 0;
        uint8_t *sender = nullptr;
    };

    static const uint8_t BROADCAST[MAC_ADDRESS_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Function prototypes
    bool send(uint8_t *dest, uint8_t *data, size_t size);
    void begin();
    void end();
    bool add_send_callback(send_callback_t callback);
    bool add_recv_callback(recv_callback_t callback);

    bool add_peer(const uint8_t *peer, int channel=0, int netif=ESP_IF_WIFI_STA);
    bool has_peer(const uint8_t *peer);
    bool remove_peer(const uint8_t *peer);

    uint8_t *sender();
    bool is_success();
    bool is_ready();
    bool is_sending();
    State get_state();

    // Callback function prototypes
#ifdef ESP32
    void send_handler(const uint8_t *addr, esp_now_send_status_t status);
    void recv_handler(const uint8_t *addr, const uint8_t *data, int size);
#elif ESP8266
    void send_handler(uint8_t *addr, esp_now_send_status_t status);
    void recv_handler(uint8_t *addr, uint8_t *data, uint8_t size);
#endif
}  // namespace espnow_proxy_base
}  // esphome

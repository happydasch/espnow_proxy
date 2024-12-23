#pragma once

#include <cstring>
#include <string>

namespace esphome {
namespace espnow_proxy_base {

    #define MAX_CALLBACKS 5
    #define MAX_PEERS 10

    #define MAC_ADDRESS_LEN 6
    #define MAGIC_HEADER_LEN 2
    #define MAX_DATA_LEN 250
    #define SEND_TIMEOUT_MS 2500L
    #define ESPNOW_HEADER_LEN (sizeof(command_header_t))
    #define MAX_PAYLOAD_LENGTH (MAX_DATA_LEN - ESPNOW_HEADER_LEN)

    typedef uint64_t mac_address_t;

    typedef enum {
        Command_None = 0x00,
        Command_Data = 0x01,
        Command_DataAck = 0x02,
    } Command_e;

    typedef struct __attribute__((packed)) {
        uint8_t magic[MAGIC_HEADER_LEN];
        uint8_t command;
        uint8_t packet_id;
    } command_header_t;

    typedef struct __attribute__((packed)) {
        command_header_t header;
        uint8_t data[MAX_PAYLOAD_LENGTH];
    } command_data_t;

    typedef struct __attribute__((packed)) {
        command_header_t header;
        uint8_t packet_id_acked;
    } command_data_ack_t;

    typedef union __attribute__((packed)) {
        uint8_t raw[MAX_DATA_LEN];
        command_header_t command_header;
        command_data_t command_data;
        command_data_ack_t command_data_ack;
    } packet_data_t;

    struct recv_data_t {
        uint32_t time;
        uint8_t addr[MAC_ADDRESS_LEN];
        packet_data_t data;
        size_t size;
    };

    struct send_data_t {
        uint8_t packet_id;
        uint32_t time;
        uint8_t retries;
        mac_address_t address;
        uint8_t data[MAX_PAYLOAD_LENGTH];
        size_t size;
        bool sent;
    };

    std::string addr64_to_str(mac_address_t address);
    uint8_t *addr64_to_addr(mac_address_t address);
    std::string addr_to_str(const uint8_t *address);
    mac_address_t addr_to_addr64(const uint8_t *address);

}  // namespace espnow_proxy_base
}  // esphome

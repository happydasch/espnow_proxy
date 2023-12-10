#include "common.h"

namespace esphome {
namespace espnow_proxy_base {

    mac_address_t addr_to_addr64(const uint8_t *address) {
        uint64_t temp = (uint64_t(address[0]) << 40) |
                        (uint64_t(address[1]) << 32) |
                        (uint64_t(address[2]) << 24) |
                        (uint64_t(address[3]) << 16) |
                        (uint64_t(address[4]) << 8) |
                        (uint64_t(address[5]) << 0);
        return temp;
    }

    uint8_t *addr64_to_addr(mac_address_t address) {
        static uint8_t buffer[MAC_ADDRESS_LEN];
        buffer[0] = (uint8_t)(address >> 40) & 0xff;
        buffer[1] = (uint8_t)(address >> 32) & 0xff;
        buffer[2] = (uint8_t)(address >> 24) & 0xff;
        buffer[3] = (uint8_t)(address >> 16) & 0xff;
        buffer[4] = (uint8_t)(address >> 8) & 0xff;
        buffer[5] = (uint8_t)(address >> 0) & 0xff;
        return buffer;
    }

    std::string addr64_to_str(mac_address_t address) {
        char buffer[18];
        snprintf(
            buffer,
            sizeof(buffer),
            "%02x:%02x:%02x:%02x:%02x:%02x",
            (uint8_t)(address >> 40) & 0xff,
            (uint8_t)(address >> 32) & 0xff,
            (uint8_t)(address >> 24) & 0xff,
            (uint8_t)(address >> 16) & 0xff,
            (uint8_t)(address >> 8) & 0xff,
            (uint8_t)(address >> 0) & 0xff);
        return buffer;
    }

    std::string addr_to_str(const uint8_t *address) {
        char buffer[18];
        snprintf(buffer, sizeof(buffer), "%02x:%02x:%02x:%02x:%02x:%02x",
                    (uint8_t)address[0],
                    (uint8_t)address[1],
                    (uint8_t)address[2],
                    (uint8_t)address[3],
                    (uint8_t)address[4],
                    (uint8_t)address[5]);
        return buffer;
    };

}  // namespace espnow_proxy_base
}  // esphome
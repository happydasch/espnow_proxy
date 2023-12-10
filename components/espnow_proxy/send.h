#pragma once

#include "base.h"

namespace esphome {
namespace espnow_proxy_base {

    static const uint8_t MAGIC_HEADER[MAGIC_HEADER_LEN] = {0xD3, 0xFC};

    Command_e get_command(const uint8_t *data, const size_t size);

    bool send_command_data(uint8_t *dest, uint8_t *data, uint8_t size, uint8_t packet_id=0);
    bool send_command_data_ack(uint8_t *dest, uint8_t packet_id_acked=0, uint8_t packet_id=0);

}  // namespace espnow_proxy_base
}  // esphome
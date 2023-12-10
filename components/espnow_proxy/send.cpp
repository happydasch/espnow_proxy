#include "send.h"

namespace esphome {
namespace espnow_proxy_base {

    static const char *TAG = "espnow_proxy_base.send";

    packet_data_t buffer;

    Command_e get_command(const uint8_t *data, const size_t size) {

        if (size <= MAGIC_HEADER_LEN || memcmp(data, MAGIC_HEADER, MAGIC_HEADER_LEN) != 0) {
            return Command_None;
        }

        uint8_t command = data[MAGIC_HEADER_LEN];

        return (Command_e)command;

    }

    void fill_command_header(uint8_t command, uint8_t packet_id = 0) {

        memcpy(buffer.command_header.magic, MAGIC_HEADER, MAGIC_HEADER_LEN);
        buffer.command_header.command = command;
        buffer.command_header.packet_id = packet_id;

    }

    bool send_command_data(uint8_t *dest, uint8_t *data, uint8_t size, uint8_t packet_id) {

        if (size > MAX_PAYLOAD_LENGTH) {
            return false;
        }

        fill_command_header(Command_Data, packet_id);
        memcpy(buffer.command_data.data, data, size);

        return send(dest, buffer.raw, size + HEADER_LEN);

    }

    bool send_command_data_ack(uint8_t *dest, uint8_t packet_id_acked, uint8_t packet_id) {

        fill_command_header(Command_DataAck, packet_id);
        buffer.command_data_ack.packet_id_acked = packet_id_acked;

        return send(dest, buffer.raw, sizeof(command_data_ack_t));

    }

}  // namespace espnow_proxy_base
}  // esphome
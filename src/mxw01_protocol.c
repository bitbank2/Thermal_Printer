/* Adapted from the bundled MXW01 project; see mxw01_protocol.LICENSE. */
#include "mxw01_protocol.h"

#include <limits.h>
#include <string.h>

#define MXW01_PREAMBLE_FIRST 0x22u
#define MXW01_PREAMBLE_SECOND 0x21u
#define MXW01_COMMAND_PROTOCOL_BYTE 0x00u
#define MXW01_PACKET_FOOTER 0xFFu
#define MXW01_PRINT_FIXED_BYTE 0x30u

uint8_t mxw01_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0u;
    size_t index;

    if (data == NULL && length != 0u) {
        return 0u;
    }

    for (index = 0u; index < length; ++index) {
        unsigned bit;

        crc ^= data[index];
        for (bit = 0u; bit < 8u; ++bit) {
            if ((crc & 0x80u) != 0u) {
                crc = (uint8_t)((uint8_t)(crc << 1u) ^ 0x07u);
            } else {
                crc = (uint8_t)(crc << 1u);
            }
        }
    }

    return crc;
}

uint8_t mxw01_reverse_bits(uint8_t value)
{
    value = (uint8_t)(((value & 0x55u) << 1u) |
                      ((value & 0xAAu) >> 1u));
    value = (uint8_t)(((value & 0x33u) << 2u) |
                      ((value & 0xCCu) >> 2u));
    return (uint8_t)((value << 4u) | (value >> 4u));
}

mxw01_protocol_result_t mxw01_build_command(
    uint8_t command_id,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length)
{
    size_t packet_length;

    if (output == NULL || output_length == NULL ||
        (payload == NULL && payload_length != 0u)) {
        return MXW01_PROTOCOL_INVALID_ARGUMENT;
    }
    if (payload_length > UINT16_MAX) {
        return MXW01_PROTOCOL_PAYLOAD_TOO_LARGE;
    }

    packet_length = MXW01_PACKET_OVERHEAD + payload_length;
    if (output_capacity < packet_length) {
        return MXW01_PROTOCOL_BUFFER_TOO_SMALL;
    }

    output[0] = MXW01_PREAMBLE_FIRST;
    output[1] = MXW01_PREAMBLE_SECOND;
    output[2] = command_id;
    output[3] = MXW01_COMMAND_PROTOCOL_BYTE;
    output[4] = (uint8_t)(payload_length & 0xFFu);
    output[5] = (uint8_t)((payload_length >> 8u) & 0xFFu);
    if (payload_length != 0u) {
        memcpy(&output[6], payload, payload_length);
    }
    output[6u + payload_length] = mxw01_crc8(payload, payload_length);
    output[7u + payload_length] = MXW01_PACKET_FOOTER;
    *output_length = packet_length;

    return MXW01_PROTOCOL_OK;
}

mxw01_protocol_result_t mxw01_build_get_status_command(
    uint8_t *output, size_t output_capacity, size_t *output_length)
{
    static const uint8_t payload[] = {0x00u};
    return mxw01_build_command(MXW01_COMMAND_GET_STATUS, payload,
                               sizeof(payload), output, output_capacity,
                               output_length);
}

mxw01_protocol_result_t mxw01_build_set_intensity_command(
    uint8_t intensity,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length)
{
    const uint8_t payload[] = {intensity};
    return mxw01_build_command(MXW01_COMMAND_SET_INTENSITY, payload,
                               sizeof(payload), output, output_capacity,
                               output_length);
}

mxw01_protocol_result_t mxw01_build_print_command(
    uint16_t line_count,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length)
{
    const uint8_t payload[] = {
        (uint8_t)(line_count & 0xFFu),
        (uint8_t)((line_count >> 8u) & 0xFFu),
        MXW01_PRINT_FIXED_BYTE,
        MXW01_PRINT_MODE_MONOCHROME
    };
    return mxw01_build_command(MXW01_COMMAND_PRINT, payload,
                               sizeof(payload), output, output_capacity,
                               output_length);
}

mxw01_protocol_result_t mxw01_build_flush_command(
    uint8_t *output, size_t output_capacity, size_t *output_length)
{
    static const uint8_t payload[] = {0x00u};
    return mxw01_build_command(MXW01_COMMAND_FLUSH, payload,
                               sizeof(payload), output, output_capacity,
                               output_length);
}

mxw01_protocol_result_t mxw01_parse_notification(
    const uint8_t *packet,
    size_t packet_length,
    mxw01_notification_t *notification)
{
    uint16_t payload_length;
    size_t payload_end;
    size_t trailer_length;

    if (packet == NULL || notification == NULL) {
        return MXW01_PROTOCOL_INVALID_ARGUMENT;
    }
    if (packet_length < 6u) {
        return MXW01_PROTOCOL_TRUNCATED_PACKET;
    }
    if (packet[0] != MXW01_PREAMBLE_FIRST ||
        packet[1] != MXW01_PREAMBLE_SECOND) {
        return MXW01_PROTOCOL_INVALID_PREAMBLE;
    }

    payload_length = (uint16_t)packet[4] |
                     (uint16_t)((uint16_t)packet[5] << 8u);
    payload_end = 6u + (size_t)payload_length;
    if (packet_length < payload_end) {
        return MXW01_PROTOCOL_TRUNCATED_PACKET;
    }

    notification->command_id = packet[2];
    notification->protocol_byte = packet[3];
    notification->payload = &packet[6];
    notification->payload_length = payload_length;
    notification->trailer = MXW01_NOTIFICATION_TRAILER_NONE;
    notification->crc_valid = false;
    notification->footer_valid = false;

    trailer_length = packet_length - payload_end;
    if (trailer_length == 1u) {
        if (packet[payload_end] == MXW01_PACKET_FOOTER) {
            notification->trailer = MXW01_NOTIFICATION_TRAILER_FOOTER_ONLY;
            notification->footer_valid = true;
        } else {
            notification->trailer = MXW01_NOTIFICATION_TRAILER_PARTIAL;
        }
    } else if (trailer_length >= 2u) {
        notification->trailer = MXW01_NOTIFICATION_TRAILER_CRC_AND_FOOTER;
        notification->crc_valid =
            packet[payload_end] ==
            mxw01_crc8(notification->payload, notification->payload_length);
        notification->footer_valid =
            packet[payload_end + 1u] == MXW01_PACKET_FOOTER;
    }

    return MXW01_PROTOCOL_OK;
}

const char *mxw01_protocol_result_string(mxw01_protocol_result_t result)
{
    switch (result) {
    case MXW01_PROTOCOL_OK:
        return "ok";
    case MXW01_PROTOCOL_INVALID_ARGUMENT:
        return "invalid argument";
    case MXW01_PROTOCOL_BUFFER_TOO_SMALL:
        return "buffer too small";
    case MXW01_PROTOCOL_PAYLOAD_TOO_LARGE:
        return "payload too large";
    case MXW01_PROTOCOL_INVALID_PREAMBLE:
        return "invalid preamble";
    case MXW01_PROTOCOL_TRUNCATED_PACKET:
        return "truncated packet";
    default:
        return "unknown protocol result";
    }
}

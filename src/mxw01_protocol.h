#ifndef MXW01_PROTOCOL_H
#define MXW01_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MXW01_PRINTER_WIDTH_PIXELS 384u
#define MXW01_BYTES_PER_ROW 48u

/* TODO(MXW01): Confirm on hardware whether short jobs really require 90 rows. */
#define MXW01_MIN_PRINT_LINES 90u

#define MXW01_SERVICE_UUID 0xAE30u
/* TODO(MXW01): Confirm whether embedded BLE stacks ever expose AF30. */
#define MXW01_SERVICE_UUID_ALT 0xAF30u
#define MXW01_CONTROL_CHARACTERISTIC_UUID 0xAE01u
#define MXW01_NOTIFY_CHARACTERISTIC_UUID 0xAE02u
#define MXW01_DATA_CHARACTERISTIC_UUID 0xAE03u

#define MXW01_COMMAND_GET_STATUS 0xA1u
#define MXW01_COMMAND_SET_INTENSITY 0xA2u
#define MXW01_COMMAND_PRINT 0xA9u
#define MXW01_COMMAND_PRINT_COMPLETE 0xAAu
#define MXW01_COMMAND_FLUSH 0xADu

/* The Python implementation and captured packet vectors use mode 0 for 1-bpp. */
#define MXW01_PRINT_MODE_MONOCHROME 0x00u

#define MXW01_PACKET_OVERHEAD 8u
#define MXW01_MAX_KNOWN_CONTROL_PAYLOAD 4u
#define MXW01_MAX_KNOWN_CONTROL_PACKET_SIZE \
    (MXW01_PACKET_OVERHEAD + MXW01_MAX_KNOWN_CONTROL_PAYLOAD)

typedef enum {
    MXW01_PROTOCOL_OK = 0,
    MXW01_PROTOCOL_INVALID_ARGUMENT,
    MXW01_PROTOCOL_BUFFER_TOO_SMALL,
    MXW01_PROTOCOL_PAYLOAD_TOO_LARGE,
    MXW01_PROTOCOL_INVALID_PREAMBLE,
    MXW01_PROTOCOL_TRUNCATED_PACKET
} mxw01_protocol_result_t;

typedef enum {
    MXW01_NOTIFICATION_TRAILER_NONE = 0,
    MXW01_NOTIFICATION_TRAILER_FOOTER_ONLY,
    MXW01_NOTIFICATION_TRAILER_CRC_AND_FOOTER,
    MXW01_NOTIFICATION_TRAILER_PARTIAL
} mxw01_notification_trailer_t;

/* payload points into the packet supplied to mxw01_parse_notification(). */
typedef struct {
    uint8_t command_id;
    uint8_t protocol_byte;
    const uint8_t *payload;
    uint16_t payload_length;
    mxw01_notification_trailer_t trailer;
    bool crc_valid;
    bool footer_valid;
} mxw01_notification_t;

uint8_t mxw01_crc8(const uint8_t *data, size_t length);
uint8_t mxw01_reverse_bits(uint8_t value);

mxw01_protocol_result_t mxw01_build_command(
    uint8_t command_id,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length);

mxw01_protocol_result_t mxw01_build_get_status_command(
    uint8_t *output, size_t output_capacity, size_t *output_length);

mxw01_protocol_result_t mxw01_build_set_intensity_command(
    uint8_t intensity,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length);

mxw01_protocol_result_t mxw01_build_print_command(
    uint16_t line_count,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length);

mxw01_protocol_result_t mxw01_build_flush_command(
    uint8_t *output, size_t output_capacity, size_t *output_length);

mxw01_protocol_result_t mxw01_parse_notification(
    const uint8_t *packet,
    size_t packet_length,
    mxw01_notification_t *notification);

const char *mxw01_protocol_result_string(mxw01_protocol_result_t result);

#ifdef __cplusplus
}
#endif

#endif

#include "mxw01_protocol.h"

#include <stdio.h>
#include <string.h>

static unsigned failures;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: check failed: %s\n",                    \
                    __FILE__, __LINE__, #condition);                            \
            ++failures;                                                        \
        }                                                                       \
    } while (0)

static void check_packet(const uint8_t *actual,
                         size_t actual_length,
                         const uint8_t *expected,
                         size_t expected_length)
{
    CHECK(actual_length == expected_length);
    if (actual_length == expected_length) {
        CHECK(memcmp(actual, expected, expected_length) == 0);
    }
}

static void test_crc_matches_python_vectors(void)
{
    static const uint8_t zero[] = {0x00u};
    static const uint8_t intensity[] = {0x5Du};
    static const uint8_t print_90[] = {0x5Au, 0x00u, 0x30u, 0x00u};
    static const uint8_t sequence[] = {
        0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u,
        0x08u, 0x09u, 0x0Au, 0x0Bu, 0x0Cu, 0x0Du, 0x0Eu, 0x0Fu
    };

    CHECK(mxw01_crc8(NULL, 0u) == 0x00u);
    CHECK(mxw01_crc8(zero, sizeof(zero)) == 0x00u);
    CHECK(mxw01_crc8(intensity, sizeof(intensity)) == 0x94u);
    CHECK(mxw01_crc8(print_90, sizeof(print_90)) == 0x99u);
    CHECK(mxw01_crc8(sequence, sizeof(sequence)) == 0x41u);
}

static void test_known_commands(void)
{
    uint8_t packet[MXW01_MAX_KNOWN_CONTROL_PACKET_SIZE];
    size_t packet_length = 0u;
    static const uint8_t expected_status[] = {
        0x22u, 0x21u, 0xA1u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0xFFu
    };
    static const uint8_t expected_intensity[] = {
        0x22u, 0x21u, 0xA2u, 0x00u, 0x01u, 0x00u, 0x5Du, 0x94u, 0xFFu
    };
    static const uint8_t expected_print[] = {
        0x22u, 0x21u, 0xA9u, 0x00u, 0x04u, 0x00u,
        0x5Au, 0x00u, 0x30u, 0x00u, 0x99u, 0xFFu
    };
    static const uint8_t expected_flush[] = {
        0x22u, 0x21u, 0xADu, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0xFFu
    };

    CHECK(mxw01_build_get_status_command(packet, sizeof(packet), &packet_length) == MXW01_PROTOCOL_OK);
    check_packet(packet, packet_length, expected_status, sizeof(expected_status));
    CHECK(mxw01_build_set_intensity_command(0x5Du, packet, sizeof(packet), &packet_length) == MXW01_PROTOCOL_OK);
    check_packet(packet, packet_length, expected_intensity, sizeof(expected_intensity));
    CHECK(mxw01_build_print_command(90u, packet, sizeof(packet), &packet_length) == MXW01_PROTOCOL_OK);
    check_packet(packet, packet_length, expected_print, sizeof(expected_print));
    CHECK(mxw01_build_flush_command(packet, sizeof(packet), &packet_length) == MXW01_PROTOCOL_OK);
    check_packet(packet, packet_length, expected_flush, sizeof(expected_flush));
}

static void test_printer_bit_order(void)
{
    CHECK(mxw01_reverse_bits(0x00u) == 0x00u);
    CHECK(mxw01_reverse_bits(0x80u) == 0x01u);
    CHECK(mxw01_reverse_bits(0x01u) == 0x80u);
    CHECK(mxw01_reverse_bits(0x96u) == 0x69u);
}

static void test_command_errors(void)
{
    uint8_t packet[8];
    size_t packet_length = 123u;
    static const uint8_t payload[] = {0x01u};

    CHECK(mxw01_build_command(0xA1u, payload, sizeof(payload), packet,
                              sizeof(packet), &packet_length) ==
          MXW01_PROTOCOL_BUFFER_TOO_SMALL);
    CHECK(packet_length == 123u);
    CHECK(mxw01_build_command(0xA1u, NULL, sizeof(payload), packet,
                              sizeof(packet), &packet_length) ==
          MXW01_PROTOCOL_INVALID_ARGUMENT);
}

static void test_notification_variants(void)
{
    mxw01_notification_t notification;
    static const uint8_t without_trailer[] = {
        0x22u, 0x21u, 0xA9u, 0x03u, 0x01u, 0x00u, 0x00u
    };
    static const uint8_t footer_only[] = {
        0x22u, 0x21u, 0xAAu, 0x00u, 0x00u, 0x00u, 0xFFu
    };
    static const uint8_t crc_and_footer[] = {
        0x22u, 0x21u, 0xA2u, 0x00u, 0x01u, 0x00u, 0x5Du, 0x94u, 0xFFu
    };
    static const uint8_t bad_crc[] = {
        0x22u, 0x21u, 0xA2u, 0x00u, 0x01u, 0x00u, 0x5Du, 0x00u, 0xFFu
    };

    CHECK(mxw01_parse_notification(without_trailer, sizeof(without_trailer),
                                    &notification) == MXW01_PROTOCOL_OK);
    CHECK(notification.command_id == MXW01_COMMAND_PRINT);
    CHECK(notification.payload_length == 1u && notification.payload[0] == 0u);
    CHECK(notification.trailer == MXW01_NOTIFICATION_TRAILER_NONE);
    CHECK(mxw01_parse_notification(footer_only, sizeof(footer_only),
                                    &notification) == MXW01_PROTOCOL_OK);
    CHECK(notification.command_id == MXW01_COMMAND_PRINT_COMPLETE);
    CHECK(notification.trailer == MXW01_NOTIFICATION_TRAILER_FOOTER_ONLY);
    CHECK(notification.footer_valid);
    CHECK(mxw01_parse_notification(crc_and_footer, sizeof(crc_and_footer),
                                    &notification) == MXW01_PROTOCOL_OK);
    CHECK(notification.crc_valid && notification.footer_valid);
    CHECK(mxw01_parse_notification(bad_crc, sizeof(bad_crc),
                                    &notification) == MXW01_PROTOCOL_OK);
    CHECK(!notification.crc_valid && notification.footer_valid);
}

static void test_notification_errors(void)
{
    mxw01_notification_t notification;
    static const uint8_t too_short[] = {0x22u, 0x21u, 0xA1u};
    static const uint8_t bad_preamble[] = {
        0x00u, 0x21u, 0xA1u, 0x00u, 0x00u, 0x00u
    };
    static const uint8_t truncated_payload[] = {
        0x22u, 0x21u, 0xA1u, 0x00u, 0x02u, 0x00u, 0x01u
    };

    CHECK(mxw01_parse_notification(too_short, sizeof(too_short),
                                    &notification) ==
          MXW01_PROTOCOL_TRUNCATED_PACKET);
    CHECK(mxw01_parse_notification(bad_preamble, sizeof(bad_preamble),
                                    &notification) ==
          MXW01_PROTOCOL_INVALID_PREAMBLE);
    CHECK(mxw01_parse_notification(truncated_payload,
                                    sizeof(truncated_payload),
                                    &notification) ==
          MXW01_PROTOCOL_TRUNCATED_PACKET);
}

int main(void)
{
    test_crc_matches_python_vectors();
    test_known_commands();
    test_printer_bit_order();
    test_command_errors();
    test_notification_variants();
    test_notification_errors();

    if (failures != 0u) {
        fprintf(stderr, "%u test(s) failed\n", failures);
        return 1;
    }
    puts("All MXW01 protocol tests passed");
    return 0;
}

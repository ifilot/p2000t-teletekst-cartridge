#ifndef P2WP_H
#define P2WP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define P2WP_VERSION 2u
#define P2WP_MAX_PAYLOAD 512u
#define P2WP_HEADER_SIZE 6u
#define P2WP_CRC_SIZE 2u
#define P2WP_MAX_BODY (P2WP_HEADER_SIZE + P2WP_MAX_PAYLOAD + P2WP_CRC_SIZE)
#define P2WP_MAX_ENCODED (2u + (2u * P2WP_MAX_BODY))

#define P2WP_DELIMITER 0x7eu
#define P2WP_ESCAPE 0x7du
#define P2WP_ESCAPE_XOR 0x20u

enum p2wp_flag {
    P2WP_FLAG_RESPONSE = 1u << 0,
    P2WP_FLAG_ERROR = 1u << 1,
    P2WP_FLAG_MORE = 1u << 2,
};

enum p2wp_type {
    P2WP_TYPE_HELLO = 0x01,
    P2WP_TYPE_ECHO = 0x02,
    P2WP_TYPE_LINK_STATS = 0x03,
    P2WP_TYPE_WIFI_SCAN_START = 0x10,
    P2WP_TYPE_WIFI_SCAN_STATUS = 0x11,
    P2WP_TYPE_WIFI_SCAN_RESULT = 0x12,
    P2WP_TYPE_WIFI_CONNECT = 0x13,
    P2WP_TYPE_WIFI_STATUS = 0x14,
    P2WP_TYPE_WIFI_PROFILE_STATUS = 0x20,
    P2WP_TYPE_WIFI_PROFILE_CONNECT = 0x21,
    P2WP_TYPE_WIFI_PROFILE_SAVE = 0x22,
    P2WP_TYPE_WIFI_PROFILE_DELETE = 0x23,
    P2WP_TYPE_TELETEKST_FETCH_START = 0x30,
    P2WP_TYPE_TELETEKST_FETCH_STATUS = 0x31,
    P2WP_TYPE_TELETEKST_FETCH_ROWS = 0x32,
};

enum p2wp_teletekst_source {
    P2WP_TELETEKST_SOURCE_NOS = 0,
    P2WP_TELETEKST_SOURCE_P2000T = 1,
};

enum p2wp_capability {
    P2WP_CAPABILITY_ECHO = 1u << 0,
    P2WP_CAPABILITY_WIFI = 1u << 1,
    P2WP_CAPABILITY_INTERNET = 1u << 2,
    P2WP_CAPABILITY_WIFI_PROFILE = 1u << 3,
};

enum p2wp_error_code {
    P2WP_ERROR_UNSUPPORTED_VERSION = 0x01,
    P2WP_ERROR_UNKNOWN_TYPE = 0x02,
    P2WP_ERROR_INVALID_PAYLOAD = 0x03,
    P2WP_ERROR_SEQUENCE_CONFLICT = 0x04,
    P2WP_ERROR_INTERNAL = 0x05,
    P2WP_ERROR_WIFI_UNAVAILABLE = 0x06,
    P2WP_ERROR_WIFI_BUSY = 0x07,
};

typedef struct {
    uint8_t version;
    uint8_t flags;
    uint8_t type;
    uint8_t sequence;
    uint16_t payload_length;
    uint8_t payload[P2WP_MAX_PAYLOAD];
} p2wp_frame_t;

typedef enum {
    P2WP_PARSE_NONE,
    P2WP_PARSE_FRAME,
    P2WP_PARSE_ERROR,
} p2wp_parse_result_t;

typedef struct {
    bool active;
    bool escaped;
    bool overflow;
    size_t length;
    uint8_t body[P2WP_MAX_BODY];
} p2wp_parser_t;

/**
 * @brief Calculate a CRC-16/CCITT-FALSE checksum.
 *
 * @param data Bytes to include in the checksum.
 * @param length Number of bytes in @p data.
 * @return The 16-bit checksum using polynomial 0x1021 and initial value 0xffff.
 */
uint16_t p2wp_crc16(const uint8_t *data, size_t length);

/**
 * @brief Calculate the identity used to recognize a retransmitted request.
 *
 * The identity covers the decoded header and payload, independently of the
 * escaping used by the wire representation.
 *
 * @param frame Decoded frame to identify.
 * @return CRC-based identity of the frame header and payload.
 */
uint16_t p2wp_frame_identity(const p2wp_frame_t *frame);

/**
 * @brief Reset a streaming P2WP/2 parser.
 *
 * @param[out] parser Parser state to initialize.
 */
void p2wp_parser_init(p2wp_parser_t *parser);

/**
 * @brief Feed one encoded byte into a streaming P2WP/2 parser.
 *
 * A frame or error is reported only when a closing delimiter is received.
 * Intermediate bytes return P2WP_PARSE_NONE.
 *
 * @param[in,out] parser Parser state retained between bytes.
 * @param byte Next encoded wire byte.
 * @param[out] frame Destination for a successfully decoded frame.
 * @return Current parsing result.
 */
p2wp_parse_result_t p2wp_parser_feed(
    p2wp_parser_t *parser,
    uint8_t byte,
    p2wp_frame_t *frame
);

/**
 * @brief Encode a decoded frame into its escaped P2WP/2 wire representation.
 *
 * @param frame Frame header and payload to encode.
 * @param[out] output Destination for delimiters, escaped body, and CRC.
 * @param capacity Number of bytes available in @p output.
 * @return Encoded byte count, or zero for invalid input or insufficient space.
 */
size_t p2wp_encode(
    const p2wp_frame_t *frame,
    uint8_t *output,
    size_t capacity
);

#endif

#include "p2wp.h"

#include <string.h>

/** @copydoc p2wp_crc16 */
uint16_t p2wp_crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xffffu;

    for (size_t index = 0; index < length; ++index) {
        crc ^= (uint16_t)data[index] << 8;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000u) != 0u
                ? (uint16_t)((crc << 1) ^ 0x1021u)
                : (uint16_t)(crc << 1);
        }
    }

    return crc;
}

/** @copydoc p2wp_select_version */
uint8_t p2wp_select_version(
    uint8_t host_minimum,
    uint8_t host_maximum,
    uint8_t peripheral_minimum,
    uint8_t peripheral_maximum
) {
    if (host_minimum > host_maximum ||
        peripheral_minimum > peripheral_maximum) {
        return 0u;
    }
    const uint8_t lowest = host_minimum > peripheral_minimum
        ? host_minimum
        : peripheral_minimum;
    const uint8_t highest = host_maximum < peripheral_maximum
        ? host_maximum
        : peripheral_maximum;
    return highest >= lowest ? highest : 0u;
}

/**
 * @brief Add one byte to an in-progress CRC-16/CCITT-FALSE checksum.
 *
 * @param crc Current checksum value.
 * @param byte Byte to incorporate.
 * @return Updated checksum value.
 */
static uint16_t crc_update(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)byte << 8;
    for (unsigned bit = 0; bit < 8; ++bit) {
        crc = (crc & 0x8000u) != 0u
            ? (uint16_t)((crc << 1) ^ 0x1021u)
            : (uint16_t)(crc << 1);
    }
    return crc;
}

/** @copydoc p2wp_frame_identity */
uint16_t p2wp_frame_identity(const p2wp_frame_t *frame) {
    uint16_t crc = 0xffffu;
    const uint8_t header[P2WP_HEADER_SIZE] = {
        frame->version,
        frame->flags,
        frame->type,
        frame->sequence,
        (uint8_t)frame->payload_length,
        (uint8_t)(frame->payload_length >> 8),
    };

    for (size_t index = 0; index < sizeof(header); ++index) {
        crc = crc_update(crc, header[index]);
    }
    for (size_t index = 0; index < frame->payload_length; ++index) {
        crc = crc_update(crc, frame->payload[index]);
    }
    return crc;
}

/** @copydoc p2wp_parser_init */
void p2wp_parser_init(p2wp_parser_t *parser) {
    memset(parser, 0, sizeof(*parser));
}

/**
 * @brief Validate and decode one unescaped frame body.
 *
 * @param body Header, payload, and little-endian CRC bytes.
 * @param body_length Number of bytes in @p body.
 * @param[out] frame Destination for the validated frame.
 * @return P2WP_PARSE_FRAME on success, otherwise P2WP_PARSE_ERROR.
 */
static p2wp_parse_result_t decode_body(
    const uint8_t *body,
    size_t body_length,
    p2wp_frame_t *frame
) {
    if (body_length < P2WP_HEADER_SIZE + P2WP_CRC_SIZE) {
        return P2WP_PARSE_ERROR;
    }

    const uint16_t payload_length =
        (uint16_t)body[4] | ((uint16_t)body[5] << 8);
    if (payload_length > P2WP_MAX_PAYLOAD ||
        body_length != P2WP_HEADER_SIZE + payload_length + P2WP_CRC_SIZE) {
        return P2WP_PARSE_ERROR;
    }

    const size_t crc_offset = P2WP_HEADER_SIZE + payload_length;
    const uint16_t received_crc =
        (uint16_t)body[crc_offset] | ((uint16_t)body[crc_offset + 1] << 8);
    if (p2wp_crc16(body, crc_offset) != received_crc) {
        return P2WP_PARSE_ERROR;
    }

    frame->version = body[0];
    frame->flags = body[1];
    frame->type = body[2];
    frame->sequence = body[3];
    frame->payload_length = payload_length;
    if (payload_length != 0u) {
        memcpy(frame->payload, body + P2WP_HEADER_SIZE, payload_length);
    }
    return P2WP_PARSE_FRAME;
}

/** @copydoc p2wp_parser_feed */
p2wp_parse_result_t p2wp_parser_feed(
    p2wp_parser_t *parser,
    uint8_t byte,
    p2wp_frame_t *frame
) {
    if (byte == P2WP_DELIMITER) {
        p2wp_parse_result_t result = P2WP_PARSE_NONE;

        if (parser->active && (parser->length != 0u || parser->overflow)) {
            result = parser->escaped || parser->overflow
                ? P2WP_PARSE_ERROR
                : decode_body(parser->body, parser->length, frame);
        }

        parser->active = true;
        parser->escaped = false;
        parser->overflow = false;
        parser->length = 0u;
        return result;
    }

    if (!parser->active || parser->overflow) {
        return P2WP_PARSE_NONE;
    }

    if (parser->escaped) {
        byte ^= P2WP_ESCAPE_XOR;
        parser->escaped = false;
    } else if (byte == P2WP_ESCAPE) {
        parser->escaped = true;
        return P2WP_PARSE_NONE;
    }

    if (parser->length == sizeof(parser->body)) {
        parser->overflow = true;
        return P2WP_PARSE_NONE;
    }

    parser->body[parser->length++] = byte;
    return P2WP_PARSE_NONE;
}

/**
 * @brief Append one byte using P2WP delimiter escaping when required.
 *
 * @param byte Decoded byte to append.
 * @param[out] output Encoded destination buffer.
 * @param capacity Total capacity of @p output.
 * @param[in,out] length Current and resulting encoded length.
 * @return true when the byte fits, otherwise false.
 */
static bool append_escaped(
    uint8_t byte,
    uint8_t *output,
    size_t capacity,
    size_t *length
) {
    if (byte == P2WP_DELIMITER || byte == P2WP_ESCAPE) {
        if (*length + 2u > capacity) {
            return false;
        }
        output[(*length)++] = P2WP_ESCAPE;
        output[(*length)++] = byte ^ P2WP_ESCAPE_XOR;
        return true;
    }

    if (*length == capacity) {
        return false;
    }
    output[(*length)++] = byte;
    return true;
}

/** @copydoc p2wp_encode */
size_t p2wp_encode(
    const p2wp_frame_t *frame,
    uint8_t *output,
    size_t capacity
) {
    if (frame->payload_length > P2WP_MAX_PAYLOAD || capacity < 2u) {
        return 0u;
    }

    uint8_t body[P2WP_HEADER_SIZE + P2WP_MAX_PAYLOAD];
    body[0] = frame->version;
    body[1] = frame->flags;
    body[2] = frame->type;
    body[3] = frame->sequence;
    body[4] = (uint8_t)frame->payload_length;
    body[5] = (uint8_t)(frame->payload_length >> 8);
    if (frame->payload_length != 0u) {
        memcpy(body + P2WP_HEADER_SIZE, frame->payload, frame->payload_length);
    }

    const size_t crc_offset = P2WP_HEADER_SIZE + frame->payload_length;
    const uint16_t crc = p2wp_crc16(body, crc_offset);
    size_t output_length = 0u;
    output[output_length++] = P2WP_DELIMITER;

    for (size_t index = 0; index < crc_offset; ++index) {
        if (!append_escaped(body[index], output, capacity, &output_length)) {
            return 0u;
        }
    }
    if (!append_escaped((uint8_t)crc, output, capacity, &output_length) ||
        !append_escaped((uint8_t)(crc >> 8), output, capacity, &output_length) ||
        output_length == capacity) {
        return 0u;
    }

    output[output_length++] = P2WP_DELIMITER;
    return output_length;
}

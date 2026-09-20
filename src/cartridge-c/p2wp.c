#include "p2wp.h"

#include "platform.h"

enum {
  P2WP_BOOTSTRAP_VERSION = 2,
  P2WP_MIN_VERSION = 2,
  P2WP_MAX_VERSION = 7,
  P2WP_TYPE_HELLO = 1,
  P2WP_FLAG_RESPONSE = 1,
  P2WP_FLAG_ERROR = 2,
  P2WP_ERROR_UNSUPPORTED_VERSION = 1,
  P2WP_DELIMITER = 0x7e,
  P2WP_ESCAPE = 0x7d,
  P2WP_ESCAPE_XOR = 0x20,
  P2WP_STATUS_RX_READY = 1,
  P2WP_STATUS_TX_READY = 2,
  P2WP_HOST_LIMIT = 240,
  P2WP_HEADER_SIZE = 6,
  P2WP_CRC_SIZE = 2,
  P2WP_MAX_BODY_SIZE = P2WP_HEADER_SIZE + P2WP_HOST_LIMIT + P2WP_CRC_SIZE,
  P2WP_LINK_TIMEOUT_TICKS = 100,
  P2WP_ATTEMPTS = 3,
};

static uint8_t request[P2WP_HEADER_SIZE + P2WP_HOST_LIMIT];
static uint8_t response[P2WP_MAX_BODY_SIZE];
static uint16_t response_length;

uint16_t p2wp_crc16(const uint8_t *data, uint16_t length) {
  uint16_t crc = 0xffffu;
  uint8_t bit;

  while (length-- != 0u) {
    crc ^= (uint16_t)*data++ << 8;
    for (bit = 0; bit != 8u; ++bit) {
      crc = (crc & 0x8000u) != 0u ? (uint16_t)((crc << 1) ^ 0x1021u)
                                  : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

static uint8_t deadline_reached(uint16_t deadline) {
  return ((int16_t)(platform_clock() - deadline)) >= 0;
}

static uint8_t send_byte(uint8_t value, uint16_t deadline) {
  while ((platform_link_status() & P2WP_STATUS_TX_READY) == 0u) {
    if (deadline_reached(deadline)) {
      return 0u;
    }
  }
  platform_link_send(value);
  return 1u;
}

static uint8_t receive_byte(uint8_t *value, uint16_t deadline) {
  while ((platform_link_status() & P2WP_STATUS_RX_READY) == 0u) {
    if (deadline_reached(deadline)) {
      return 0u;
    }
  }
  *value = platform_link_receive();
  return 1u;
}

static uint8_t send_escaped(uint8_t value, uint16_t deadline) {
  if (value == P2WP_DELIMITER || value == P2WP_ESCAPE) {
    if (!send_byte(P2WP_ESCAPE, deadline)) {
      return 0u;
    }
    value ^= P2WP_ESCAPE_XOR;
  }
  return send_byte(value, deadline);
}

static uint8_t send_frame(const uint8_t *body, uint16_t length,
                          uint16_t deadline) {
  uint16_t index;
  uint16_t crc = p2wp_crc16(body, length);

  if (!send_byte(P2WP_DELIMITER, deadline)) {
    return 0u;
  }
  for (index = 0; index != length; ++index) {
    if (!send_escaped(body[index], deadline)) {
      return 0u;
    }
  }
  return send_escaped((uint8_t)crc, deadline) &&
         send_escaped((uint8_t)(crc >> 8), deadline) &&
         send_byte(P2WP_DELIMITER, deadline);
}

static uint8_t receive_frame(uint16_t deadline) {
  uint8_t value;
  uint16_t length = 0u;
  uint8_t active = 0u;
  uint8_t escaped = 0u;

  while (receive_byte(&value, deadline)) {
    if (value == P2WP_DELIMITER) {
      if (!active) {
        active = 1u;
        continue;
      }
      if (length == 0u) {
        continue;
      }
      if (escaped || length < P2WP_HEADER_SIZE + P2WP_CRC_SIZE ||
          ((uint16_t)response[4] | ((uint16_t)response[5] << 8)) >
              P2WP_HOST_LIMIT ||
          ((uint16_t)response[4] | ((uint16_t)response[5] << 8)) +
                  P2WP_HEADER_SIZE + P2WP_CRC_SIZE !=
              length) {
        return 0u;
      }
      if (p2wp_crc16(response, length - 2u) !=
          ((uint16_t)response[length - 2u] |
           ((uint16_t)response[length - 1u] << 8))) {
        return 0u;
      }
      response_length = length;
      return 1u;
    }
    if (!active) {
      continue;
    }
    if (escaped) {
      value ^= P2WP_ESCAPE_XOR;
      escaped = 0u;
    } else if (value == P2WP_ESCAPE) {
      escaped = 1u;
      continue;
    }
    if (length == sizeof(response)) {
      return 0u;
    }
    response[length++] = value;
  }
  return 0u;
}

static void build_request(uint8_t version, uint8_t type, uint8_t sequence,
                          const uint8_t *payload, uint16_t payload_length) {
  uint16_t index;

  request[0] = version;
  request[1] = 0u;
  request[2] = type;
  request[3] = sequence;
  request[4] = (uint8_t)payload_length;
  request[5] = (uint8_t)(payload_length >> 8);
  for (index = 0u; index != payload_length; ++index) {
    request[P2WP_HEADER_SIZE + index] = payload[index];
  }
}

static enum p2wp_result validate_response(uint8_t version, uint8_t type,
                                          uint8_t sequence,
                                          p2wp_response_t *reply) {
  uint8_t response_flags;
  uint16_t payload_length =
      (uint16_t)response[4] | ((uint16_t)response[5] << 8);

  if (response[0] != version || response[2] != type ||
      response[3] != sequence) {
    return P2WP_INVALID;
  }
  response_flags = response[1] & (P2WP_FLAG_RESPONSE | P2WP_FLAG_ERROR);
  if (response_flags != P2WP_FLAG_RESPONSE &&
      response_flags != (P2WP_FLAG_RESPONSE | P2WP_FLAG_ERROR)) {
    return P2WP_INVALID;
  }
  reply->flags = response[1];
  reply->type = type;
  reply->sequence = sequence;
  reply->payload_length = payload_length;
  reply->payload = response + P2WP_HEADER_SIZE;
  reply->remote_error = 0u;
  if (response_flags == (P2WP_FLAG_RESPONSE | P2WP_FLAG_ERROR)) {
    if (payload_length == 0u) {
      return P2WP_INVALID;
    }
    reply->remote_error = reply->payload[0];
    return P2WP_REMOTE_ERROR;
  }
  return P2WP_OK;
}

static enum p2wp_result transact(uint8_t version, uint8_t type,
                                 uint8_t sequence, const uint8_t *payload,
                                 uint16_t payload_length,
                                 p2wp_response_t *reply) {
  uint8_t attempt;
  uint16_t deadline = platform_clock() + P2WP_LINK_TIMEOUT_TICKS;

  build_request(version, type, sequence, payload, payload_length);
  for (attempt = 0u; attempt != P2WP_ATTEMPTS; ++attempt) {
    enum p2wp_result result;
    if (!send_frame(request, P2WP_HEADER_SIZE + payload_length, deadline) ||
        !receive_frame(deadline)) {
      continue;
    }
    result = validate_response(version, type, sequence, reply);
    if (result != P2WP_INVALID) {
      return result;
    }
  }
  return deadline_reached(deadline) ? P2WP_TIMEOUT : P2WP_INVALID;
}

static enum p2wp_result validate_hello(p2wp_session_t *session,
                                       const p2wp_response_t *reply) {
  uint8_t capabilities;
  uint8_t version;
  uint16_t limit;

  if (reply->payload_length != 8u || reply->payload[0] != 'P' ||
      reply->payload[1] != '2' || reply->payload[2] != 'W' ||
      reply->payload[3] != 'P') {
    return P2WP_INVALID;
  }
  version = reply->payload[4];
  capabilities = reply->payload[5];
  limit = (uint16_t)reply->payload[6] | ((uint16_t)reply->payload[7] << 8);
  if (version < P2WP_MIN_VERSION || version > P2WP_MAX_VERSION ||
      (capabilities & 0x0eu) != 0x0eu || limit < P2WP_HOST_LIMIT) {
    return P2WP_INVALID;
  }
  session->version = version;
  session->capabilities = capabilities;
  session->receive_limit = limit;
  session->next_sequence = 1u;
  return P2WP_OK;
}

enum p2wp_result p2wp_hello(p2wp_session_t *session) {
  static const uint8_t payload[8] = {
      'P', '2', 'W', 'P', P2WP_MIN_VERSION, P2WP_MAX_VERSION, P2WP_HOST_LIMIT,
      0,
  };
  p2wp_response_t reply;
  enum p2wp_result result;

  if (session == 0) {
    return P2WP_INVALID;
  }
  result = transact(P2WP_BOOTSTRAP_VERSION, P2WP_TYPE_HELLO, 0u, payload,
                    sizeof(payload), &reply);
  if (result == P2WP_REMOTE_ERROR &&
      reply.remote_error == P2WP_ERROR_UNSUPPORTED_VERSION) {
    return P2WP_INCOMPATIBLE;
  }
  return result == P2WP_OK ? validate_hello(session, &reply) : result;
}

enum p2wp_result p2wp_request(p2wp_session_t *session, uint8_t type,
                              const uint8_t *payload, uint16_t payload_length,
                              p2wp_response_t *reply) {
  enum p2wp_result result;

  if (session == 0 || reply == 0 || payload_length > P2WP_HOST_LIMIT ||
      (payload_length != 0u && payload == 0)) {
    return P2WP_INVALID;
  }
  result = transact(session->version, type, session->next_sequence, payload,
                    payload_length, reply);
  if (result == P2WP_OK || result == P2WP_REMOTE_ERROR) {
    ++session->next_sequence;
  }
  return result;
}

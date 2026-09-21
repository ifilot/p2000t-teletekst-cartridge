#ifndef P2000T_P2WP_H
#define P2000T_P2WP_H

#include <stdint.h>

enum p2wp_result {
  P2WP_OK = 0,
  P2WP_TIMEOUT = 1,
  P2WP_INVALID = 2,
  P2WP_INCOMPATIBLE = 3,
  P2WP_REMOTE_ERROR = 4,
};

typedef struct {
  uint8_t version;
  uint8_t capabilities;
  uint16_t receive_limit;
  uint8_t next_sequence;
} p2wp_session_t;

typedef struct {
  uint8_t flags;
  uint8_t type;
  uint8_t sequence;
  uint16_t payload_length;
  const uint8_t *payload;
  uint8_t remote_error;
} p2wp_response_t;

enum {
  P2WP_TYPE_ECHO = 0x02,
  P2WP_TYPE_DEVICE_INFO = 0x04,
  P2WP_TYPE_TELETEKST_FETCH_START = 0x30,
  P2WP_TYPE_TELETEKST_FETCH_STATUS = 0x31,
  P2WP_TYPE_TELETEKST_FETCH_ROWS = 0x32,
  P2WP_TYPE_TELETEKST_CUSTOM_URL_LOAD = 0x33,
  P2WP_TYPE_TELETEKST_CUSTOM_URL_SAVE = 0x34,
  P2WP_CAPABILITY_DEVICE_INFO = 1u << 4,
};

enum p2wp_result p2wp_hello(p2wp_session_t *session);
enum p2wp_result p2wp_request(p2wp_session_t *session, uint8_t type,
                              const uint8_t *payload, uint16_t payload_length,
                              p2wp_response_t *reply);
uint16_t p2wp_crc16(const uint8_t *data, uint16_t length);

#endif

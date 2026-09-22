/**
 * @file p2wp.h
 * @brief Public data types and requests for the cartridge P2WP client.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the P2000T Teletekst cartridge and is licensed under
 * version 3 of the GNU General Public License. See the repository LICENSE.
 */

#ifndef P2000T_P2WP_H_
#define P2000T_P2WP_H_

#include <stdint.h>

/**
 * @brief Result of a local or remote P2WP transaction.
 */
enum p2wp_result {
  P2WP_OK = 0,
  P2WP_TIMEOUT = 1,
  P2WP_INVALID = 2,
  P2WP_INCOMPATIBLE = 3,
  P2WP_REMOTE_ERROR = 4,
};

/**
 * @brief Negotiated state retained across P2WP requests.
 */
typedef struct {
  uint8_t version;        /**< Negotiated protocol revision. */
  uint8_t capabilities;   /**< Peripheral capability bits. */
  uint16_t receive_limit; /**< Peripheral receive-payload limit. */
  uint8_t next_sequence;  /**< Sequence byte for the next request. */
} p2wp_session_t;

/**
 * @brief Parsed view of a response in the shared receive buffer.
 */
typedef struct {
  uint8_t flags;           /**< Raw response flag byte. */
  uint8_t type;            /**< Echoed request type. */
  uint8_t sequence;        /**< Echoed request sequence. */
  uint16_t payload_length; /**< Number of response payload bytes. */
  const uint8_t *payload;  /**< View into the shared response buffer. */
  uint8_t remote_error;    /**< First payload byte for remote errors. */
} p2wp_response_t;

enum {
  P2WP_TYPE_ECHO = 0x02,
  P2WP_TYPE_DEVICE_INFO = 0x04,
  P2WP_TYPE_TELETEKST_FETCH_START = 0x30,
  P2WP_TYPE_TELETEKST_FETCH_STATUS = 0x31,
  P2WP_TYPE_TELETEKST_FETCH_ROWS = 0x32,
  P2WP_TYPE_TELETEKST_CUSTOM_URL_LOAD = 0x33,
  P2WP_TYPE_TELETEKST_CUSTOM_URL_SAVE = 0x34,
  P2WP_TYPE_TELETEKST_SETTINGS_LOAD = 0x35,
  P2WP_TYPE_TELETEKST_SETTINGS_SAVE = 0x36,
  P2WP_CAPABILITY_DEVICE_INFO = 1u << 4,
};

/**
 * @brief Negotiates the newest mutually supported P2WP revision.
 * @param[out] session Session initialized after a valid HELLO response.
 * @return Negotiation status.
 */
enum p2wp_result p2wp_hello(p2wp_session_t *session);

/**
 * @brief Executes one request using the session's next sequence number.
 * @param session Active negotiated session.
 * @param type Request type byte.
 * @param payload Optional request payload.
 * @param payload_length Number of payload bytes, at most 240.
 * @param[out] reply Parsed response view backed by the shared receive buffer.
 * @return Transaction or remote-error status.
 */
enum p2wp_result p2wp_request(p2wp_session_t *session, uint8_t type,
                              const uint8_t *payload, uint16_t payload_length,
                              p2wp_response_t *reply);

/**
 * @brief Calculates the P2WP CRC-16/CCITT-FALSE checksum.
 * @param data Bytes to checksum.
 * @param length Number of bytes.
 * @return Calculated 16-bit checksum.
 */
uint16_t p2wp_crc16(const uint8_t *data, uint16_t length);

#endif  // P2000T_P2WP_H_

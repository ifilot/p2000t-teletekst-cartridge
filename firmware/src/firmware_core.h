#ifndef P2WP_FIRMWARE_CORE_H
#define P2WP_FIRMWARE_CORE_H

#include "p2wp.h"

#include <stdbool.h>
#include <stdint.h>

/** Zero denotes a successful platform command; other values are P2WP errors. */
#define P2WP_FIRMWARE_COMMAND_OK 0u

typedef uint8_t (*p2wp_firmware_command_fn)(
    void *context,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
);

/**
 * Platform operations used by the portable production command processor.
 *
 * The core validates command-independent framing and each command's public
 * payload shape before invoking these functions. Implementations perform only
 * the platform state transition or snapshot and populate a successful response.
 */
typedef struct {
    uint8_t (*capabilities)(void *context);
    p2wp_firmware_command_fn version_check_start;
    p2wp_firmware_command_fn version_check_status;
    p2wp_firmware_command_fn wifi_scan_start;
    p2wp_firmware_command_fn wifi_scan_status;
    p2wp_firmware_command_fn wifi_scan_result;
    p2wp_firmware_command_fn wifi_connect;
    p2wp_firmware_command_fn wifi_status;
    p2wp_firmware_command_fn wifi_profile_status;
    p2wp_firmware_command_fn wifi_profile_connect;
    p2wp_firmware_command_fn wifi_profile_save;
    p2wp_firmware_command_fn wifi_profile_delete;
    p2wp_firmware_command_fn teletekst_fetch_start;
    p2wp_firmware_command_fn teletekst_fetch_status;
    p2wp_firmware_command_fn teletekst_fetch_rows;
    p2wp_firmware_command_fn teletekst_custom_url_load;
    p2wp_firmware_command_fn teletekst_custom_url_save;
    p2wp_firmware_command_fn teletekst_settings_load;
    p2wp_firmware_command_fn teletekst_settings_save;
    void (*clear_sensitive)(void *context);
} p2wp_firmware_operations_t;

/** Portable session and retry state shared by Pico firmware and emulator. */
typedef struct {
    const p2wp_firmware_operations_t *operations;
    void *platform_context;
    uint8_t hardware_model;
    uint8_t protocol_minimum;
    uint8_t protocol_maximum;
    uint8_t session_version;
    bool session_valid;
    bool cached_request_valid;
    uint8_t cached_request_type;
    uint8_t cached_request_sequence;
    uint16_t cached_request_identity;
    p2wp_frame_t cached_response;
} p2wp_firmware_core_t;

/** Initialize a command processor for one firmware instance. */
void p2wp_firmware_core_init(
    p2wp_firmware_core_t *core,
    const p2wp_firmware_operations_t *operations,
    void *platform_context,
    uint8_t hardware_model
);

/** Reset negotiation and retry state while retaining platform configuration. */
void p2wp_firmware_core_reset(p2wp_firmware_core_t *core);

/** Override the supported range, primarily for compatibility simulation. */
void p2wp_firmware_core_set_protocol_range(
    p2wp_firmware_core_t *core,
    uint8_t minimum,
    uint8_t maximum
);

/**
 * Process one decoded request using the production dispatcher.
 *
 * A response is always produced. Password bytes in recognized sensitive
 * requests are erased before this function returns.
 */
void p2wp_firmware_core_handle(
    p2wp_firmware_core_t *core,
    p2wp_frame_t *request,
    p2wp_frame_t *response
);

#endif

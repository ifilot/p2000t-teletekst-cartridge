#ifndef P2WP_CUSTOM_URL_STORE_H
#define P2WP_CUSTOM_URL_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "custom_endpoint.h"

typedef enum {
    CUSTOM_URL_STORE_OK = 0,
    CUSTOM_URL_STORE_NOT_FOUND = 1,
    CUSTOM_URL_STORE_CORRUPT = 2,
    CUSTOM_URL_STORE_FAILED = 3,
    CUSTOM_URL_STORE_INVALID_DATA = 4,
    CUSTOM_URL_STORE_UNCHANGED = 5,
} custom_url_store_result_t;

/** Load the last custom Teletekst URL from the penultimate flash sector. */
custom_url_store_result_t custom_url_store_load(
    char url[CUSTOM_ENDPOINT_URL_MAX + 1u],
    uint8_t *url_length
);

/**
 * Store a custom URL only when it differs from the valid record in flash.
 *
 * CUSTOM_URL_STORE_UNCHANGED guarantees that no erase/program cycle occurred.
 */
custom_url_store_result_t custom_url_store_save(
    const char *url,
    size_t url_length
);

/** Load the persisted auto-start menu source, or 0xff when disabled. */
custom_url_store_result_t custom_url_store_settings_load(
    uint8_t *auto_start_source
);

/** Persist an auto-start menu source (0-3), or 0xff to disable it. */
custom_url_store_result_t custom_url_store_settings_save(
    uint8_t auto_start_source
);

#endif

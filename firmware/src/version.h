#ifndef P2WP_FIRMWARE_VERSION_H
#define P2WP_FIRMWARE_VERSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Firmware release version. Keep this independent from P2WP_VERSION, which
// identifies the wire-protocol revision.
#define P2WP_FIRMWARE_VERSION_MAJOR 0u
#define P2WP_FIRMWARE_VERSION_MINOR 5u
#define P2WP_FIRMWARE_VERSION_PATCH 0u
#define P2WP_FIRMWARE_VERSION_STRING "v0.5.0"

typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
} p2wp_release_version_t;

/** Extract a three-component `tag_name` from GitHub release JSON. */
bool p2wp_parse_latest_release(
    const char *json,
    size_t length,
    p2wp_release_version_t *version
);

#endif

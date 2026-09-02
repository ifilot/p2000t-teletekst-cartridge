#include "version.h"

#include <string.h>

/** Parse one decimal uint8 component and advance @p position. */
static bool parse_component(
    const char *text,
    size_t length,
    size_t *position,
    uint8_t *value
) {
    if (*position == length || text[*position] < '0' || text[*position] > '9') {
        return false;
    }
    unsigned parsed = 0u;
    do {
        parsed = parsed * 10u + (unsigned)(text[*position] - '0');
        if (parsed > UINT8_MAX) {
            return false;
        }
        ++*position;
    } while (*position < length &&
             text[*position] >= '0' && text[*position] <= '9');
    *value = (uint8_t)parsed;
    return true;
}

/** @copydoc p2wp_parse_latest_release */
bool p2wp_parse_latest_release(
    const char *json,
    size_t length,
    p2wp_release_version_t *version
) {
    static const char field[] = "\"tag_name\"";
    if (json == NULL || version == NULL || length < sizeof(field)) {
        return false;
    }

    size_t position = 0u;
    while (position + sizeof(field) - 1u <= length &&
           memcmp(json + position, field, sizeof(field) - 1u) != 0) {
        ++position;
    }
    if (position + sizeof(field) - 1u > length) {
        return false;
    }
    position += sizeof(field) - 1u;
    while (position < length &&
           (json[position] == ' ' || json[position] == '\t' ||
            json[position] == '\r' || json[position] == '\n')) {
        ++position;
    }
    if (position == length || json[position++] != ':') {
        return false;
    }
    while (position < length &&
           (json[position] == ' ' || json[position] == '\t' ||
            json[position] == '\r' || json[position] == '\n')) {
        ++position;
    }
    if (position == length || json[position++] != '"') {
        return false;
    }
    if (position < length && (json[position] == 'v' || json[position] == 'V')) {
        ++position;
    }

    p2wp_release_version_t parsed;
    if (!parse_component(json, length, &position, &parsed.major) ||
        position == length || json[position++] != '.' ||
        !parse_component(json, length, &position, &parsed.minor) ||
        position == length || json[position++] != '.' ||
        !parse_component(json, length, &position, &parsed.patch) ||
        position == length || json[position] != '"') {
        return false;
    }
    *version = parsed;
    return true;
}

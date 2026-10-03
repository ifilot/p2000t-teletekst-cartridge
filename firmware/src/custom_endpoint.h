#ifndef P2WP_CUSTOM_ENDPOINT_H
#define P2WP_CUSTOM_ENDPOINT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CUSTOM_ENDPOINT_URL_MAX 96u
#define CUSTOM_ENDPOINT_HOST_MAX 63u
#define CUSTOM_ENDPOINT_BASE_PATH_MAX 63u
#define CUSTOM_ENDPOINT_REQUEST_PATH_MAX 96u

typedef struct {
    bool tls;
    char host[CUSTOM_ENDPOINT_HOST_MAX + 1u];
    uint16_t port;
    char base_path[CUSTOM_ENDPOINT_BASE_PATH_MAX + 1u];
} custom_endpoint_t;

/** Parse an HTTP(S) base URL without credentials, query, or fragment. */
bool custom_endpoint_parse(
    const char *url,
    size_t url_length,
    custom_endpoint_t *endpoint
);

/** Append /json/PAGE[-SUBPAGE] to a parsed endpoint's optional base path. */
bool custom_endpoint_page_path(
    const custom_endpoint_t *endpoint,
    uint16_t page,
    uint8_t subpage,
    char *path,
    size_t capacity
);

#define PETSCII_CHANNEL_COUNT 35u
#define PETSCII_CATALOGUE 0xffu

/** Resolve a predefined PetsciiProxy channel's base URL. */
bool petscii_channel_url(uint8_t channel, char *url, size_t capacity);

/** Render one of four catalogue screens for the P2000T. */
bool petscii_catalogue(uint8_t group, uint8_t screen[960]);

#endif

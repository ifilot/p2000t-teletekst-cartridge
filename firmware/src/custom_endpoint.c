#include "custom_endpoint.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool prefix_equal(const char *value, const char *prefix, size_t length) {
    for (size_t index = 0u; index < length; ++index) {
        if (tolower((unsigned char)value[index]) !=
            tolower((unsigned char)prefix[index])) {
            return false;
        }
    }
    return true;
}

bool custom_endpoint_parse(
    const char *url,
    size_t url_length,
    custom_endpoint_t *endpoint
) {
    if (url == NULL || endpoint == NULL || url_length == 0u ||
        url_length > CUSTOM_ENDPOINT_URL_MAX) {
        return false;
    }

    size_t position;
    uint16_t default_port;
    bool tls;
    if (url_length >= 7u && prefix_equal(url, "http://", 7u)) {
        position = 7u;
        default_port = 80u;
        tls = false;
    } else if (url_length >= 8u && prefix_equal(url, "https://", 8u)) {
        position = 8u;
        default_port = 443u;
        tls = true;
    } else {
        return false;
    }

    const size_t authority_start = position;
    while (position < url_length && url[position] != '/') {
        const unsigned char character = (unsigned char)url[position];
        if (isspace(character) || character == '@' || character == '?' ||
            character == '#') {
            return false;
        }
        ++position;
    }
    const size_t authority_end = position;
    if (authority_end == authority_start) {
        return false;
    }

    size_t host_end = authority_end;
    uint32_t port = default_port;
    const char *colon = NULL;
    for (size_t index = authority_start; index < authority_end; ++index) {
        if (url[index] == ':') {
            if (colon != NULL) {
                return false; /* Bracketless IPv6 is deliberately unsupported. */
            }
            colon = url + index;
        }
    }
    if (colon != NULL) {
        host_end = (size_t)(colon - url);
        if (host_end == authority_start || host_end + 1u == authority_end) {
            return false;
        }
        port = 0u;
        for (size_t index = host_end + 1u; index < authority_end; ++index) {
            if (url[index] < '0' || url[index] > '9') {
                return false;
            }
            port = port * 10u + (uint32_t)(url[index] - '0');
            if (port > 65535u) {
                return false;
            }
        }
        if (port == 0u) {
            return false;
        }
    }

    const size_t host_length = host_end - authority_start;
    if (host_length == 0u || host_length > CUSTOM_ENDPOINT_HOST_MAX) {
        return false;
    }
    for (size_t index = authority_start; index < host_end; ++index) {
        const unsigned char character = (unsigned char)url[index];
        if (!isalnum(character) && character != '.' && character != '-' &&
            character != '_') {
            return false;
        }
    }

    size_t path_end = url_length;
    while (path_end > position + 1u && url[path_end - 1u] == '/') {
        --path_end;
    }
    const size_t path_length = path_end - position;
    if (path_length > CUSTOM_ENDPOINT_BASE_PATH_MAX) {
        return false;
    }
    for (size_t index = position; index < path_end; ++index) {
        const unsigned char character = (unsigned char)url[index];
        if (isspace(character) || character == '?' || character == '#') {
            return false;
        }
    }

    memset(endpoint, 0, sizeof(*endpoint));
    endpoint->tls = tls;
    endpoint->port = (uint16_t)port;
    memcpy(endpoint->host, url + authority_start, host_length);
    endpoint->host[host_length] = '\0';
    if (path_length != 0u) {
        memcpy(endpoint->base_path, url + position, path_length);
    }
    endpoint->base_path[path_length] = '\0';
    return true;
}

bool custom_endpoint_page_path(
    const custom_endpoint_t *endpoint,
    uint16_t page,
    uint8_t subpage,
    char *path,
    size_t capacity
) {
    if (endpoint == NULL || path == NULL || capacity == 0u ||
        page < 100u || page > 899u || subpage > 99u) {
        return false;
    }
    const int length = subpage == 0u
        ? snprintf(path, capacity, "%s/json/%u", endpoint->base_path, page)
        : snprintf(
            path,
            capacity,
            "%s/json/%u-%u",
            endpoint->base_path,
            page,
            subpage
        );
    return length >= 0 && (size_t)length < capacity;
}

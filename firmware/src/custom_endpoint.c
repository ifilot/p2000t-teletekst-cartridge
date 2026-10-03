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
    /* PetsciiProxy serves /CHANNEL/PAGE-SUBPAGE rather than NOS JSON routes. */
    if (strlen(endpoint->host) == 15u &&
        prefix_equal(endpoint->host, "petsciiproxy.nl", 15u) &&
        endpoint->base_path[0] == '/' && endpoint->base_path[1] != '\0') {
        const int length = snprintf(path, capacity, "%s/%u-%u",
                                    endpoint->base_path, page, subpage);
        return length >= 0 && (size_t)length < capacity;
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

/** Stable P2WP/8 channel indices; inactive MTVA is intentionally excluded. */
static const char *const petscii_channels[PETSCII_CHANNEL_COUNT] = {
    "NOS-TT",
    "NOSNEWS",
    "BMN1",
    "ARD-TEXT",
    "ZDF-TEXT",
    "ZDFINFO",
    "ZDFNEO",
    "3SAT",
    "WDR-TEXT",
    "HR-TEXT",
    "SWR-BW",
    "SWR-RP",
    "ORF1",
    "ORF2",
    "ORF3",
    "ORFSPORT",
    "CEEFAX",
    "TEEFAX",
    "CHUNKYTEXT",
    "WEBFAX1",
    "WEBFAX2",
    "SPARK",
    "TEKSTI-TV",
    "HBN-TEKSTI-TV",
    "SVT-TEXT",
    "DR-TEKST-TV",
    "SRF1",
    "SRF2",
    "SRFINFO",
    "RTS1",
    "RTS2",
    "RSILA1",
    "RSILA2",
    "FORUM64",
    "TELETEXT64",
};

bool petscii_channel_url(uint8_t channel, char *url, size_t capacity) {
    if (channel >= PETSCII_CHANNEL_COUNT || url == NULL) return false;
    const int length = snprintf(url, capacity, "http://petsciiproxy.nl:8080/%s",
                                petscii_channels[channel]);
    return length >= 0 && (size_t)length < capacity;
}

bool petscii_catalogue(uint8_t group, uint8_t screen[960]) {
    if (group >= 4u || screen == NULL) return false;
    memset(screen, ' ', 960u);
    memcpy(screen + 40u, "\004\035\007 INTERNATIONALE TELETEKST", 28u);
    memcpy(screen + 120u, " PETSCIIPROXY.NL", 15u);
    for (uint8_t i = 0u; i < 9u && group * 9u + i < PETSCII_CHANNEL_COUNT; ++i) {
        uint8_t *row = screen + (5u + i) * 40u;
        row[1] = '1' + i;
        memcpy(row + 4u, petscii_channels[group * 9u + i],
               strlen(petscii_channels[group * 9u + i]));
    }
    memcpy(screen + 640u, " P / N: VORIGE / VOLGENDE LIJST", 29u);
    memcpy(screen + 720u, " 1-9: KIES ZENDER   STOP: TERUG", 29u);
    screen[3u * 40u + 30u] = '1' + group;
    memcpy(screen + 3u * 40u + 31u, "/4", 2u);
    return true;
}

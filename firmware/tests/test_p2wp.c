#include "p2wp.h"
#include "firmware_core.h"
#include "teletekst.h"
#include "custom_endpoint.h"
#include "version.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Verify CRC-16/CCITT-FALSE against its standard check vector.
 */
static void test_crc(void) {
    static const uint8_t input[] = "123456789";
    assert(p2wp_crc16(input, sizeof(input) - 1u) == 0x29b1u);
}

/** Verify newest-common-version selection and incompatible ranges. */
static void test_version_negotiation(void) {
    assert(p2wp_select_version(2u, 5u, 2u, 5u) == 5u);
    assert(p2wp_select_version(2u, 4u, 2u, 5u) == 4u);
    assert(p2wp_select_version(2u, 3u, 2u, 5u) == 3u);
    assert(p2wp_select_version(2u, 2u, 2u, 5u) == 2u);
    assert(p2wp_select_version(2u, 3u, 1u, 1u) == 0u);
    assert(p2wp_select_version(2u, 3u, 4u, 4u) == 0u);
    assert(p2wp_select_version(3u, 2u, 2u, 3u) == 0u);
}

static void test_custom_endpoint(void) {
    custom_endpoint_t endpoint;
    assert(custom_endpoint_parse(
        "http://terra:8080", 17u, &endpoint
    ));
    assert(!endpoint.tls);
    assert(strcmp(endpoint.host, "terra") == 0);
    assert(endpoint.port == 8080u);
    assert(strcmp(endpoint.base_path, "") == 0);

    static const char secure[] = "https://192.168.1.20/teletext/";
    assert(custom_endpoint_parse(secure, sizeof(secure) - 1u, &endpoint));
    assert(endpoint.tls && endpoint.port == 443u);
    assert(strcmp(endpoint.host, "192.168.1.20") == 0);
    assert(strcmp(endpoint.base_path, "/teletext") == 0);
    char path[CUSTOM_ENDPOINT_REQUEST_PATH_MAX];
    assert(custom_endpoint_page_path(
        &endpoint, 200u, 2u, path, sizeof(path)
    ));
    assert(strcmp(path, "/teletext/json/200-2") == 0);

    assert(!custom_endpoint_parse("terra:8080", 10u, &endpoint));
    assert(!custom_endpoint_parse("http://user@terra", 17u, &endpoint));
    assert(!custom_endpoint_parse("http://terra:0", 14u, &endpoint));
    assert(!custom_endpoint_parse("http://terra/path?q=1", 21u, &endpoint));
}

/** Verify bounded GitHub release-tag parsing and malformed input rejection. */
static void test_release_version_parsing(void) {
    static const char response[] =
        "{\n  \"url\":\"ignored\",\n  \"tag_name\" : \"v12.34.5\",\n"
        "  \"name\":\"Patch\"\n}";
    p2wp_release_version_t version = {0};
    assert(p2wp_parse_latest_release(
        response,
        sizeof(response) - 1u,
        &version
    ));
    assert(version.major == 12u && version.minor == 34u && version.patch == 5u);
    static const char short_tag[] = "{\"tag_name\":\"v1.2\"}";
    assert(!p2wp_parse_latest_release(
        short_tag,
        sizeof(short_tag) - 1u,
        &version
    ));
    static const char overflow[] = "{\"tag_name\":\"v256.2.3\"}";
    assert(!p2wp_parse_latest_release(
        overflow,
        sizeof(overflow) - 1u,
        &version
    ));
}

/**
 * @brief Verify frame encoding, escaping, streaming decode, and field recovery.
 */
static void test_round_trip(void) {
    p2wp_frame_t source = {
        .version = P2WP_VERSION,
        .flags = P2WP_FLAG_RESPONSE,
        .type = P2WP_TYPE_ECHO,
        .sequence = 0x7e,
        .payload_length = 6,
        .payload = {0x00, 0x7d, 0x7e, 0xff, 0xaa, 0x55},
    };
    uint8_t encoded[P2WP_MAX_ENCODED];
    const size_t encoded_length = p2wp_encode(&source, encoded, sizeof(encoded));
    assert(encoded_length != 0u);

    p2wp_parser_t parser;
    p2wp_frame_t decoded;
    p2wp_parser_init(&parser);
    p2wp_parse_result_t result = P2WP_PARSE_NONE;
    for (size_t index = 0; index < encoded_length; ++index) {
        result = p2wp_parser_feed(&parser, encoded[index], &decoded);
    }

    assert(result == P2WP_PARSE_FRAME);
    assert(decoded.version == source.version);
    assert(decoded.flags == source.flags);
    assert(decoded.type == source.type);
    assert(decoded.sequence == source.sequence);
    assert(decoded.payload_length == source.payload_length);
    assert(memcmp(decoded.payload, source.payload, source.payload_length) == 0);
}

/**
 * @brief Confirm that a single-byte mutation is rejected by CRC validation.
 */
static void test_corruption(void) {
    p2wp_frame_t source = {
        .version = P2WP_VERSION,
        .type = P2WP_TYPE_ECHO,
        .payload_length = 1,
        .payload = {0x42},
    };
    uint8_t encoded[P2WP_MAX_ENCODED];
    const size_t encoded_length = p2wp_encode(&source, encoded, sizeof(encoded));
    assert(encoded_length > 5u);
    encoded[4] ^= 0x01u;

    p2wp_parser_t parser;
    p2wp_frame_t decoded;
    p2wp_parser_init(&parser);
    p2wp_parse_result_t result = P2WP_PARSE_NONE;
    for (size_t index = 0; index < encoded_length; ++index) {
        result = p2wp_parser_feed(&parser, encoded[index], &decoded);
    }
    assert(result == P2WP_PARSE_ERROR);
}

/**
 * @brief Verify maximum password payloads containing both escaped byte values.
 */
static void test_profile_save_round_trip(void) {
    p2wp_frame_t source = {
        .version = P2WP_VERSION,
        .type = P2WP_TYPE_WIFI_PROFILE_SAVE,
        .sequence = 0x20,
        .payload_length = 1u + 63u,
    };
    source.payload[0] = 63u;
    for (size_t index = 1u; index < source.payload_length; ++index) {
        source.payload[index] = index % 2u == 0u
            ? P2WP_DELIMITER
            : P2WP_ESCAPE;
    }

    uint8_t encoded[P2WP_MAX_ENCODED];
    const size_t encoded_length = p2wp_encode(
        &source,
        encoded,
        sizeof(encoded)
    );
    assert(encoded_length != 0u);

    p2wp_parser_t parser;
    p2wp_frame_t decoded;
    p2wp_parser_init(&parser);
    p2wp_parse_result_t result = P2WP_PARSE_NONE;
    for (size_t index = 0u; index < encoded_length; ++index) {
        result = p2wp_parser_feed(&parser, encoded[index], &decoded);
    }
    assert(result == P2WP_PARSE_FRAME);
    assert(decoded.type == P2WP_TYPE_WIFI_PROFILE_SAVE);
    assert(decoded.payload_length == source.payload_length);
    assert(memcmp(decoded.payload, source.payload, source.payload_length) == 0);
}

/**
 * @brief Append a NUL-terminated fragment to a bounded test JSON buffer.
 *
 * @param[in,out] buffer Destination JSON buffer.
 * @param capacity Total capacity of @p buffer.
 * @param[in,out] length Current and resulting text length.
 * @param text NUL-terminated fragment to append.
 */
static void append_text(
    char *buffer,
    size_t capacity,
    size_t *length,
    const char *text
) {
    const size_t text_length = strlen(text);
    assert(text_length < capacity - *length);
    memcpy(buffer + *length, text, text_length);
    *length += text_length;
    buffer[*length] = '\0';
}

/**
 * @brief Base64-encode a multiple-of-three byte fixture into a JSON buffer.
 *
 * @param[in,out] buffer Destination JSON buffer.
 * @param capacity Total capacity of @p buffer.
 * @param[in,out] length Current and resulting text length.
 * @param data Binary fixture bytes.
 * @param data_length Number of bytes in @p data.
 */
static void append_base64(
    char *buffer,
    size_t capacity,
    size_t *length,
    const uint8_t *data,
    size_t data_length
) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    assert(data_length % 3u == 0u);
    for (size_t offset = 0u; offset < data_length; offset += 3u) {
        const uint32_t group =
            ((uint32_t)data[offset] << 16u) |
            ((uint32_t)data[offset + 1u] << 8u) |
            data[offset + 2u];
        char encoded[5] = {
            alphabet[(group >> 18u) & 0x3fu],
            alphabet[(group >> 12u) & 0x3fu],
            alphabet[(group >> 6u) & 0x3fu],
            alphabet[group & 0x3fu],
            '\0',
        };
        append_text(buffer, capacity, length, encoded);
    }
}

/**
 * @brief Construct a complete 25-row NOS-style response fixture.
 *
 * The fixture covers nested styles, entities, bare ampersands, mosaic graphics,
 * background colours, and a mode transition without a free control-code cell.
 *
 * @param[out] json Destination buffer.
 * @param capacity Capacity of @p json.
 * @return Number of JSON bytes produced.
 */
static size_t build_teletekst_json(char *json, size_t capacity) {
    size_t length = 0u;
    append_text(
        json,
        capacity,
        &length,
        "{\"nextSubPage\":\"100-2\",\"content\":\""
    );

    for (unsigned column = 0u; column < 21u; ++column) {
        append_text(json, capacity, &length, " ");
    }
    append_text(
        json,
        capacity,
        &length,
        "<span class=\\\"green \\\"> NOS Teletekst</span>"
        "<span class=\\\"yellow \\\"> 100 </span>\\n"
    );

    append_text(
        json,
        capacity,
        &length,
        "&#xF020;<span class=\\\"blue bg-blue \\\">&#xF020;</span>"
        "<span class=\\\"bg-blue \\\">&#xF020;&#xF03c;"
    );
    for (unsigned column = 0u; column < 34u; ++column) {
        append_text(json, capacity, &length, "&#xF020;");
    }
    // A mode switch with no blank control cell, as seen on NOS page 200.
    append_text(json, capacity, &length, "&#xF035;X</span>\\n");

    append_text(json, capacity, &length, " Belgi&euml; &");
    for (unsigned column = 0u; column < 31u; ++column) {
        append_text(json, capacity, &length, " ");
    }
    append_text(json, capacity, &length, "\\n");

    append_text(
        json,
        capacity,
        &length,
        " <span class=\\\"black bg-white \\\">X</span>"
    );
    for (unsigned column = 2u; column < TELETEKST_COLUMNS; ++column) {
        append_text(json, capacity, &length, " ");
    }
    append_text(json, capacity, &length, "\\n");

    append_text(
        json,
        capacity,
        &length,
        " #&pound;_&#x2190;&#x2192;"
    );
    for (unsigned column = 6u; column < TELETEKST_COLUMNS; ++column) {
        append_text(json, capacity, &length, " ");
    }
    append_text(json, capacity, &length, "\\n");

    for (unsigned row = 5u; row < TELETEKST_SOURCE_ROWS; ++row) {
        for (unsigned column = 0u; column < TELETEKST_COLUMNS; ++column) {
            append_text(json, capacity, &length, " ");
        }
        append_text(json, capacity, &length, "\\n");
    }
    append_text(json, capacity, &length, "\"}");
    return length;
}

typedef struct {
    uint8_t glyph;
    uint8_t foreground;
    uint8_t background;
    uint8_t graphics;
} rendered_cell_t;

/**
 * @brief Interpret one compiled row into visual cells for semantic assertions.
 *
 * @param input Forty SAA5050 display bytes.
 * @param[out] rendered Resulting glyph and colour state for every cell.
 */
static void render_saa5050_row(
    const uint8_t input[TELETEKST_COLUMNS],
    rendered_cell_t rendered[TELETEKST_COLUMNS]
) {
    uint8_t foreground = 7u;
    uint8_t background = 0u;
    uint8_t graphics = 0u;
    for (size_t column = 0u; column < TELETEKST_COLUMNS; ++column) {
        const uint8_t value = input[column];
        rendered[column].glyph = ' ';
        rendered[column].foreground = foreground;
        rendered[column].background = background;
        rendered[column].graphics = graphics;
        if ((value & 0x7fu) < 0x20u) {
            const uint8_t control = value & 0x7fu;
            if (control >= 1u && control <= 7u) {
                foreground = control;
                graphics = 0u;
            } else if (control >= 0x11u && control <= 0x17u) {
                foreground = control - 0x10u;
                graphics = 1u;
            } else if (control == 0x1cu) {
                background = 0u;
                rendered[column].background = background;
            } else if (control == 0x1du) {
                background = foreground;
                rendered[column].background = background;
            }
            continue;
        }
        rendered[column].glyph = value & 0x7fu;
        if ((value & 0x80u) != 0u) {
            rendered[column].foreground = background;
            rendered[column].background = foreground;
        }
    }
}

/**
 * @brief Verify NOS content parsing and SAA5050 row compilation semantics.
 */
static void test_teletekst_conversion(void) {
    char json[16384];
    const size_t json_length = build_teletekst_json(json, sizeof(json));
    uint8_t screen[TELETEKST_SCREEN_SIZE];
    uint8_t next_subpage = 0u;
    assert(teletekst_decode_nos_json(
        json,
        json_length,
        100u,
        screen,
        &next_subpage
    ));
    assert(next_subpage == 2u);

    rendered_cell_t row[TELETEKST_COLUMNS];
    render_saa5050_row(screen, row);
    assert(row[22].glyph == 'N');
    assert(row[22].foreground == 2u);
    assert(row[22].background == 0u);
    assert(row[36].glyph == '1');
    assert(row[36].foreground == 3u);

    render_saa5050_row(screen + TELETEKST_COLUMNS, row);
    assert(row[3].glyph == 0x3cu);
    assert(row[3].foreground == 7u);
    assert(row[3].background == 4u);
    assert(row[3].graphics == 1u);
    assert(row[39].glyph == 'X');
    assert(row[39].graphics == 0u);

    render_saa5050_row(screen + 2u * TELETEKST_COLUMNS, row);
    assert(row[1].glyph == 'B');
    assert(row[6].glyph == 'e');
    assert(row[8].glyph == '&');

    render_saa5050_row(screen + 3u * TELETEKST_COLUMNS, row);
    assert(row[1].glyph == 'X');
    assert(row[1].foreground == 0u);
    assert(row[1].background == 7u);

    render_saa5050_row(screen + 4u * TELETEKST_COLUMNS, row);
    assert(row[1].glyph == 0x5fu); // hash is not ASCII 23h on the P2000T
    assert(row[2].glyph == 0x23u); // 23h is the pound glyph
    assert(row[3].glyph == 0x60u); // closest available horizontal bar
    assert(row[4].glyph == 0x5bu); // left arrow
    assert(row[5].glyph == 0x5du); // right arrow
}

/**
 * @brief Verify exact binaryDisplay preference and malformed base64 rejection.
 */
static void test_exact_binary_display(void) {
    FILE *file = fopen("data/engineering.bin", "rb");
    assert(file != NULL);
    uint8_t expected[TELETEKST_SCREEN_SIZE];
    assert(fread(expected, 1u, sizeof(expected), file) == sizeof(expected));
    assert(fgetc(file) == EOF);
    assert(fclose(file) == 0);

    char json[2048];
    size_t length = 0u;
    append_text(
        json,
        sizeof(json),
        &length,
        "{\"nextSubPage\":\"\",\"binaryDisplay\":\""
    );
    append_base64(json, sizeof(json), &length, expected, sizeof(expected));
    append_text(json, sizeof(json), &length, "\",\"content\":\"ignored\"}");

    uint8_t screen[TELETEKST_SCREEN_SIZE];
    memset(screen, 0, sizeof(screen));
    uint8_t next_subpage = 99u;
    assert(teletekst_decode_nos_json(
        json,
        length,
        888u,
        screen,
        &next_subpage
    ));
    assert(next_subpage == 0u);
    assert(memcmp(screen, expected, sizeof(screen)) == 0);

    static const char malformed[] =
        "{\"nextSubPage\":\"\",\"binaryDisplay\":\"invalid\"}";
    assert(!teletekst_decode_nos_json(
        malformed,
        sizeof(malformed) - 1u,
        888u,
        screen,
        &next_subpage
    ));
}

static void test_navigation_metadata(void) {
    char json[18000];
    const size_t length = build_teletekst_json(json, sizeof(json));
    const char needle[] = "{\"nextSubPage\":\"100-2\"";
    const char replacement[] =
        "{\"prevPage\":\"099\",\"nextPage\":\"101\","
        "\"nextSubPage\":\"100-2\"";
    (void)needle;
    char enriched[18100];
    const size_t replacement_length = sizeof(replacement) - 1u;
    memcpy(enriched, replacement, replacement_length);
    memcpy(
        enriched + replacement_length,
        json + sizeof(needle) - 1u,
        length - (sizeof(needle) - 1u)
    );
    const size_t enriched_length = replacement_length +
        length - (sizeof(needle) - 1u);
    uint8_t screen[TELETEKST_SCREEN_SIZE];
    teletekst_metadata_t metadata;
    assert(!teletekst_decode_json(
        enriched, enriched_length, 100u, screen, &metadata
    )); /* Page 099 is outside the supported range. */

    memcpy(enriched + 13u, "100", 3u);
    assert(teletekst_decode_json(
        enriched, enriched_length, 100u, screen, &metadata
    ));
    assert(metadata.next_subpage == 2u);
    assert(metadata.previous_page == 100u);
    assert(metadata.next_page == 101u);
}

typedef struct {
    unsigned command_calls;
    unsigned capability_calls;
    unsigned sensitive_clears;
    uint8_t command_error;
} fake_firmware_platform_t;

static uint8_t fake_capabilities(void *context) {
    fake_firmware_platform_t *platform = context;
    ++platform->capability_calls;
    return P2WP_CAPABILITY_ECHO | P2WP_CAPABILITY_WIFI |
        P2WP_CAPABILITY_INTERNET | P2WP_CAPABILITY_WIFI_PROFILE |
        P2WP_CAPABILITY_DEVICE_INFO | P2WP_CAPABILITY_VERSION_CHECK;
}

static uint8_t fake_command(
    void *context,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    fake_firmware_platform_t *platform = context;
    ++platform->command_calls;
    response->payload[0] = request->type;
    response->payload_length = 1u;
    return platform->command_error;
}

static void fake_clear_sensitive(void *context) {
    fake_firmware_platform_t *platform = context;
    ++platform->sensitive_clears;
}

static const p2wp_firmware_operations_t fake_operations = {
    .capabilities = fake_capabilities,
    .version_check_start = fake_command,
    .version_check_status = fake_command,
    .wifi_scan_start = fake_command,
    .wifi_scan_status = fake_command,
    .wifi_scan_result = fake_command,
    .wifi_connect = fake_command,
    .wifi_status = fake_command,
    .wifi_profile_status = fake_command,
    .wifi_profile_connect = fake_command,
    .wifi_profile_save = fake_command,
    .wifi_profile_delete = fake_command,
    .teletekst_fetch_start = fake_command,
    .teletekst_fetch_status = fake_command,
    .teletekst_fetch_rows = fake_command,
    .teletekst_custom_url_load = fake_command,
    .teletekst_custom_url_save = fake_command,
    .clear_sensitive = fake_clear_sensitive,
};

static p2wp_frame_t make_hello(uint8_t sequence) {
    p2wp_frame_t request = {
        .version = P2WP_BOOTSTRAP_VERSION,
        .type = P2WP_TYPE_HELLO,
        .sequence = sequence,
        .payload_length = 8u,
        .payload = {'P', '2', 'W', 'P', 2u, 3u, 240u, 0u},
    };
    return request;
}

/** Verify the portable production dispatcher and its platform boundary. */
static void test_firmware_core(void) {
    fake_firmware_platform_t platform = {0};
    p2wp_firmware_core_t core;
    p2wp_firmware_core_init(
        &core,
        &fake_operations,
        &platform,
        P2WP_HARDWARE_PICO_2_W
    );

    p2wp_frame_t request = {
        .version = 3u,
        .type = P2WP_TYPE_DEVICE_INFO,
        .sequence = 1u,
    };
    p2wp_frame_t response;
    p2wp_firmware_core_handle(&core, &request, &response);
    assert((response.flags & P2WP_FLAG_ERROR) != 0u);
    assert(response.payload[0] == P2WP_ERROR_UNSUPPORTED_VERSION);

    request = make_hello(0u);
    p2wp_firmware_core_handle(&core, &request, &response);
    assert(response.flags == P2WP_FLAG_RESPONSE);
    assert(response.payload_length == 8u);
    assert(memcmp(response.payload, "P2WP", 4u) == 0);
    assert(response.payload[4] == 3u);
    assert(response.payload[5] == 0x3fu);
    assert(platform.capability_calls == 1u);

    p2wp_frame_t retry = make_hello(0u);
    p2wp_firmware_core_handle(&core, &retry, &response);
    assert(response.payload[4] == 3u);
    assert(platform.capability_calls == 1u);

    request = (p2wp_frame_t){
        .version = 3u,
        .type = P2WP_TYPE_DEVICE_INFO,
        .sequence = 1u,
    };
    p2wp_firmware_core_handle(&core, &request, &response);
    assert(response.flags == P2WP_FLAG_RESPONSE);
    assert(response.payload_length == 4u);
    assert(response.payload[0] == P2WP_HARDWARE_PICO_2_W);
    assert(response.payload[1] == P2WP_FIRMWARE_VERSION_MAJOR);
    assert(response.payload[2] == P2WP_FIRMWARE_VERSION_MINOR);
    assert(response.payload[3] == P2WP_FIRMWARE_VERSION_PATCH);

    request = (p2wp_frame_t){
        .version = 3u,
        .type = P2WP_TYPE_WIFI_CONNECT,
        .sequence = 2u,
        .payload_length = 4u,
        .payload = {0u, 2u, 'p', 'w'},
    };
    p2wp_firmware_core_handle(&core, &request, &response);
    assert(response.flags == P2WP_FLAG_RESPONSE);
    assert(platform.command_calls == 1u);
    assert(platform.sensitive_clears == 1u);
    assert(request.payload[2] == 0u && request.payload[3] == 0u);

    request = (p2wp_frame_t){
        .version = 3u,
        .type = P2WP_TYPE_WIFI_CONNECT,
        .sequence = 2u,
        .payload_length = 4u,
        .payload = {0u, 2u, 'p', 'w'},
    };
    p2wp_firmware_core_handle(&core, &request, &response);
    assert(platform.command_calls == 1u);
    assert(platform.sensitive_clears == 2u);
    assert(request.payload[2] == 0u && request.payload[3] == 0u);

    request = (p2wp_frame_t){
        .version = 3u,
        .type = P2WP_TYPE_WIFI_STATUS,
        .sequence = 2u,
    };
    p2wp_firmware_core_handle(&core, &request, &response);
    assert((response.flags & P2WP_FLAG_ERROR) != 0u);
    assert(response.payload[0] == P2WP_ERROR_SEQUENCE_CONFLICT);

    request = (p2wp_frame_t){
        .version = 3u,
        .type = P2WP_TYPE_TELETEKST_FETCH_START,
        .sequence = 3u,
        .payload_length = 4u,
        .payload = {99u, 0u, 0u, P2WP_TELETEKST_SOURCE_NOS},
    };
    p2wp_firmware_core_handle(&core, &request, &response);
    assert((response.flags & P2WP_FLAG_ERROR) != 0u);
    assert(response.payload[0] == P2WP_ERROR_INVALID_PAYLOAD);
    assert(platform.command_calls == 1u);

    request = (p2wp_frame_t){
        .version = 3u,
        .type = 0xfeu,
        .sequence = 4u,
    };
    p2wp_firmware_core_handle(&core, &request, &response);
    assert((response.flags & P2WP_FLAG_ERROR) != 0u);
    assert(response.payload[0] == P2WP_ERROR_UNKNOWN_TYPE);

    memset(&platform, 0, sizeof(platform));
    p2wp_firmware_core_init(
        &core,
        &fake_operations,
        &platform,
        P2WP_HARDWARE_PICO_2_W
    );
    request = (p2wp_frame_t){
        .version = P2WP_BOOTSTRAP_VERSION,
        .type = P2WP_TYPE_HELLO,
        .payload_length = 8u,
        .payload = {'P', '2', 'W', 'P', 2u, 5u, 240u, 0u},
    };
    p2wp_firmware_core_handle(&core, &request, &response);
    assert(response.payload[4] == 5u);

    request = (p2wp_frame_t){
        .version = 5u,
        .type = P2WP_TYPE_TELETEKST_CUSTOM_URL_LOAD,
        .sequence = 1u,
    };
    p2wp_firmware_core_handle(&core, &request, &response);
    assert(response.flags == P2WP_FLAG_RESPONSE);
    assert(platform.command_calls == 1u);

    request = (p2wp_frame_t){
        .version = 5u,
        .type = P2WP_TYPE_TELETEKST_CUSTOM_URL_SAVE,
        .sequence = 2u,
        .payload_length = 18u,
        .payload = {17u, 'h', 't', 't', 'p', ':', '/', '/', 't', 'e', 'r',
                    'r', 'a', ':', '8', '0', '8', '0'},
    };
    p2wp_firmware_core_handle(&core, &request, &response);
    assert(response.flags == P2WP_FLAG_RESPONSE);
    assert(platform.command_calls == 2u);

    request.sequence = 3u;
    request.payload_length = 17u;
    p2wp_firmware_core_handle(&core, &request, &response);
    assert((response.flags & P2WP_FLAG_ERROR) != 0u);
    assert(response.payload[0] == P2WP_ERROR_INVALID_PAYLOAD);
    assert(platform.command_calls == 2u);
}

/**
 * @brief Run all native framing and Teletekst decoder regression tests.
 *
 * @return Zero after all assertions pass.
 */
int main(void) {
    test_crc();
    test_version_negotiation();
    test_release_version_parsing();
    test_round_trip();
    test_corruption();
    test_profile_save_round_trip();
    test_custom_endpoint();
    test_teletekst_conversion();
    test_exact_binary_display();
    test_navigation_metadata();
    test_firmware_core();
    puts("p2wp tests passed");
    return 0;
}

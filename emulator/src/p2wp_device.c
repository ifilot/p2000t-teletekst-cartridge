#include "p2wp_device.h"
#include "../../firmware/src/firmware_core.h"
#include "../../firmware/src/version.h"

#include <stdbool.h>
#include <string.h>

enum {
    TX_PORT = 0x40,
    RX_PORT = 0x41,
    STATUS_PORT = 0x42,
    SCREEN_SIZE = 960,
    CHUNK_SIZE = 240,
};

static p2wp_parser_t parser;
static p2wp_firmware_core_t firmware_core;
static p2wp_fetch_fn fetch_page;
static void *fetch_context;
static uint8_t encoded[P2WP_MAX_ENCODED];
static size_t encoded_length;
static size_t encoded_position;
static uint8_t screen[SCREEN_SIZE];
static uint8_t next_subpage;
static uint8_t local_clock[7];
static uint16_t previous_page, next_page;
static uint8_t fetch_state;
static uint8_t fetch_error;
static uint8_t profile_state;
static uint8_t protocol_maximum = P2WP_MAX_VERSION;
static uint8_t status_length_override;

static uint8_t emulator_capabilities(void *context) {
    (void)context;
    return protocol_maximum >= 3u
        ? P2WP_CAPABILITY_ECHO | P2WP_CAPABILITY_WIFI |
            P2WP_CAPABILITY_INTERNET | P2WP_CAPABILITY_WIFI_PROFILE |
            P2WP_CAPABILITY_DEVICE_INFO | P2WP_CAPABILITY_VERSION_CHECK
        : P2WP_CAPABILITY_ECHO | P2WP_CAPABILITY_WIFI |
            P2WP_CAPABILITY_INTERNET | P2WP_CAPABILITY_WIFI_PROFILE;
}

static uint8_t version_check_start(
    void *context,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    (void)context;
    (void)request;
    response->payload_length = 0u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t version_check_status(
    void *context,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    (void)context;
    (void)request;
    response->payload[0] = P2WP_VERSION_CHECK_COMPLETE;
    response->payload[1] = 0u;
    response->payload[2] = P2WP_FIRMWARE_VERSION_MAJOR;
    response->payload[3] = P2WP_FIRMWARE_VERSION_MINOR;
    response->payload[4] = P2WP_FIRMWARE_VERSION_PATCH;
    response->payload_length = 5u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t wifi_scan_start(
    void *context,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    (void)context;
    (void)request;
    response->payload_length = 0u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t wifi_scan_status(
    void *context,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    (void)context;
    (void)request;
    response->payload[0] = 2u;
    response->payload[1] = 1u;
    response->payload[2] = 1u;
    response->payload_length = 3u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t wifi_scan_result(
    void *context,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    (void)context;
    static const char ssid[] = "Emulated WiFi";
    response->payload[0] = request->payload[0];
    response->payload[1] = (uint8_t)-35;
    response->payload[2] = 0u;
    response->payload[3] = sizeof(ssid) - 1u;
    memcpy(response->payload + 4u, ssid, sizeof(ssid) - 1u);
    response->payload_length = 4u + sizeof(ssid) - 1u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t wifi_connect(
    void *context,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    (void)context;
    (void)request;
    response->payload_length = 0u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t wifi_status(
    void *context,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    (void)context;
    (void)request;
    response->payload[0] = 2u;
    response->payload_length = 1u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t wifi_profile_status(
    void *context,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    (void)context;
    (void)request;
    response->payload[0] = profile_state;
    response->payload[1] = 0u;
    response->payload_length = 2u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t wifi_profile_connect(
    void *context,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    (void)context;
    (void)request;
    response->payload_length = 0u;
    return profile_state != 0u
        ? P2WP_FIRMWARE_COMMAND_OK
        : P2WP_ERROR_WIFI_BUSY;
}

static uint8_t wifi_profile_save(
    void *context,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    (void)context;
    (void)request;
    profile_state = 1u;
    response->payload_length = 0u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t wifi_profile_delete(
    void *context,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    (void)context;
    (void)request;
    profile_state = 0u;
    response->payload_length = 0u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t teletekst_fetch_start(
    void *context,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    (void)context;
    const uint16_t page =
        (uint16_t)request->payload[0] |
        ((uint16_t)request->payload[1] << 8u);
    char custom_url[97] = {0};
    if (request->payload[3] == P2WP_TELETEKST_SOURCE_CUSTOM) {
        memcpy(custom_url, request->payload + 5u, request->payload[4]);
    }
    previous_page = 0u;
    next_page = 0u;
    fetch_error = fetch_page != NULL && fetch_page(
        fetch_context,
        request->payload[3],
        custom_url[0] != '\0' ? custom_url : NULL,
        page,
        request->payload[2],
        screen,
        &next_subpage,
        &previous_page,
        &next_page,
        local_clock
    ) == 0 ? 0u : 7u;
    fetch_state = fetch_error != 0u ? 4u : 3u;
    response->payload_length = 0u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t teletekst_fetch_status(
    void *context,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    (void)context;
    response->payload[0] = fetch_state;
    response->payload[1] = fetch_error;
    response->payload[4] = next_subpage;
    if (request->version >= 3u || status_length_override >= 9u) {
        memcpy(response->payload + 5u, local_clock, 3u);
        response->payload[8] = fetch_error != 0u ? 0u : 1u;
        memcpy(response->payload + 9u, local_clock + 3u, 4u);
        if (request->version >= 4u) {
            response->payload[13] = (uint8_t)previous_page;
            response->payload[14] = (uint8_t)(previous_page >> 8u);
            response->payload[15] = (uint8_t)next_page;
            response->payload[16] = (uint8_t)(next_page >> 8u);
        }
        response->payload_length = status_length_override != 0u
            ? status_length_override
            : (request->version >= 4u ? 17u : 13u);
    } else {
        response->payload_length = status_length_override != 0u
            ? status_length_override
            : 5u;
    }
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t teletekst_fetch_rows(
    void *context,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    (void)context;
    memcpy(
        response->payload,
        screen + (size_t)request->payload[0] * CHUNK_SIZE,
        CHUNK_SIZE
    );
    response->payload_length = CHUNK_SIZE;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static const p2wp_firmware_operations_t emulator_operations = {
    .capabilities = emulator_capabilities,
    .version_check_start = version_check_start,
    .version_check_status = version_check_status,
    .wifi_scan_start = wifi_scan_start,
    .wifi_scan_status = wifi_scan_status,
    .wifi_scan_result = wifi_scan_result,
    .wifi_connect = wifi_connect,
    .wifi_status = wifi_status,
    .wifi_profile_status = wifi_profile_status,
    .wifi_profile_connect = wifi_profile_connect,
    .wifi_profile_save = wifi_profile_save,
    .wifi_profile_delete = wifi_profile_delete,
    .teletekst_fetch_start = teletekst_fetch_start,
    .teletekst_fetch_status = teletekst_fetch_status,
    .teletekst_fetch_rows = teletekst_fetch_rows,
};

void p2wp_device_init(p2wp_fetch_fn fetch, void *context) {
    fetch_page = fetch;
    fetch_context = context;
    p2wp_firmware_core_init(
        &firmware_core,
        &emulator_operations,
        NULL,
        P2WP_HARDWARE_PICO_2_W
    );
    p2wp_device_reset();
}

void p2wp_device_set_protocol_range(uint8_t minimum, uint8_t maximum) {
    protocol_maximum = maximum;
    p2wp_firmware_core_set_protocol_range(
        &firmware_core,
        minimum,
        maximum
    );
    p2wp_device_reset();
}

void p2wp_device_set_status_length(uint8_t length) {
    status_length_override = length;
}

void p2wp_device_reset(void) {
    p2wp_parser_init(&parser);
    p2wp_firmware_core_reset(&firmware_core);
    encoded_length = 0u;
    encoded_position = 0u;
    fetch_state = 0u;
    fetch_error = 0u;
    next_subpage = 0u;
    previous_page = 0u;
    next_page = 0u;
    profile_state = 0u;
    memset(screen, 0, sizeof(screen));
}

void p2wp_device_out(uint8_t port, uint8_t value) {
    if (port != TX_PORT) {
        return;
    }
    p2wp_frame_t request;
    const p2wp_parse_result_t result = p2wp_parser_feed(
        &parser,
        value,
        &request
    );
    if (result != P2WP_PARSE_FRAME) {
        return;
    }
    p2wp_frame_t response;
    p2wp_firmware_core_handle(&firmware_core, &request, &response);
    encoded_length = p2wp_encode(&response, encoded, sizeof(encoded));
    encoded_position = 0u;
}

uint8_t p2wp_device_in(uint8_t port) {
    if (port == STATUS_PORT) {
        return 2u | (encoded_position < encoded_length ? 1u : 0u);
    }
    if (port == RX_PORT && encoded_position < encoded_length) {
        return encoded[encoded_position++];
    }
    return 0xffu;
}

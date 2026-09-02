#include "firmware_core.h"

#include "teletekst.h"
#include "version.h"
#include "wifi_profile.h"

#include <string.h>

enum {
    WIFI_MAX_PASSWORD = 63u,
    TELETEKST_CHUNK_COUNT = 4u,
    TELETEKST_CUSTOM_URL_MAX = 96u,
};

static void make_error(
    const p2wp_frame_t *request,
    p2wp_frame_t *response,
    uint8_t error
) {
    memset(response, 0, sizeof(*response));
    response->version = request->version;
    response->flags = P2WP_FLAG_RESPONSE | P2WP_FLAG_ERROR;
    response->type = request->type;
    response->sequence = request->sequence;
    response->payload_length = 1u;
    response->payload[0] = error;
}

static void make_success(
    const p2wp_firmware_core_t *core,
    const p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    memset(response, 0, sizeof(*response));
    response->version = request->type == P2WP_TYPE_HELLO
        ? P2WP_BOOTSTRAP_VERSION
        : core->session_version;
    response->flags = P2WP_FLAG_RESPONSE;
    response->type = request->type;
    response->sequence = request->sequence;
}

static bool empty_request(const p2wp_frame_t *request) {
    return request->payload_length == 0u;
}

static bool valid_hello(const p2wp_frame_t *request) {
    static const uint8_t magic[] = {'P', '2', 'W', 'P'};
    return request->payload_length == 8u &&
        memcmp(request->payload, magic, sizeof(magic)) == 0 &&
        request->payload[4] <= request->payload[5];
}

static p2wp_firmware_command_fn command_for_type(
    const p2wp_firmware_operations_t *operations,
    uint8_t type
) {
    if (operations == NULL) {
        return NULL;
    }
    switch (type) {
        case P2WP_TYPE_VERSION_CHECK_START:
            return operations->version_check_start;
        case P2WP_TYPE_VERSION_CHECK_STATUS:
            return operations->version_check_status;
        case P2WP_TYPE_WIFI_SCAN_START:
            return operations->wifi_scan_start;
        case P2WP_TYPE_WIFI_SCAN_STATUS:
            return operations->wifi_scan_status;
        case P2WP_TYPE_WIFI_SCAN_RESULT:
            return operations->wifi_scan_result;
        case P2WP_TYPE_WIFI_CONNECT:
            return operations->wifi_connect;
        case P2WP_TYPE_WIFI_STATUS:
            return operations->wifi_status;
        case P2WP_TYPE_WIFI_PROFILE_STATUS:
            return operations->wifi_profile_status;
        case P2WP_TYPE_WIFI_PROFILE_CONNECT:
            return operations->wifi_profile_connect;
        case P2WP_TYPE_WIFI_PROFILE_SAVE:
            return operations->wifi_profile_save;
        case P2WP_TYPE_WIFI_PROFILE_DELETE:
            return operations->wifi_profile_delete;
        case P2WP_TYPE_TELETEKST_FETCH_START:
            return operations->teletekst_fetch_start;
        case P2WP_TYPE_TELETEKST_FETCH_STATUS:
            return operations->teletekst_fetch_status;
        case P2WP_TYPE_TELETEKST_FETCH_ROWS:
            return operations->teletekst_fetch_rows;
        default:
            return NULL;
    }
}

static bool platform_payload_is_valid(const p2wp_frame_t *request) {
    switch (request->type) {
        case P2WP_TYPE_VERSION_CHECK_START:
        case P2WP_TYPE_VERSION_CHECK_STATUS:
        case P2WP_TYPE_WIFI_SCAN_START:
        case P2WP_TYPE_WIFI_SCAN_STATUS:
        case P2WP_TYPE_WIFI_STATUS:
        case P2WP_TYPE_WIFI_PROFILE_STATUS:
        case P2WP_TYPE_WIFI_PROFILE_CONNECT:
        case P2WP_TYPE_WIFI_PROFILE_DELETE:
        case P2WP_TYPE_TELETEKST_FETCH_STATUS:
            return empty_request(request);

        case P2WP_TYPE_WIFI_SCAN_RESULT:
            return request->payload_length == 1u;

        case P2WP_TYPE_WIFI_CONNECT:
            return request->payload_length >= 2u &&
                request->payload[1] <= WIFI_MAX_PASSWORD &&
                request->payload_length ==
                    (uint16_t)(2u + request->payload[1]);

        case P2WP_TYPE_WIFI_PROFILE_SAVE:
            return request->payload_length >= 1u &&
                request->payload[0] <= WIFI_PROFILE_MAX_PASSWORD &&
                request->payload_length ==
                    (uint16_t)(1u + request->payload[0]);

        case P2WP_TYPE_TELETEKST_FETCH_START: {
            if (request->payload_length < 4u) {
                return false;
            }
            const uint16_t page =
                (uint16_t)request->payload[0] |
                ((uint16_t)request->payload[1] << 8u);
            if (page < 100u || page > 899u || request->payload[2] > 99u) {
                return false;
            }
            if (request->payload[3] <= P2WP_TELETEKST_SOURCE_P2000T) {
                return request->payload_length == 4u;
            }
            if (request->payload[3] != P2WP_TELETEKST_SOURCE_CUSTOM ||
                request->version < 4u || request->payload_length < 5u) {
                return false;
            }
            const uint8_t url_length = request->payload[4];
            return url_length > 0u && url_length <= TELETEKST_CUSTOM_URL_MAX &&
                request->payload_length == (uint16_t)(5u + url_length);
        }

        case P2WP_TYPE_TELETEKST_FETCH_ROWS:
            return request->payload_length == 1u &&
                request->payload[0] < TELETEKST_CHUNK_COUNT;

        default:
            return false;
    }
}

static void clear_sensitive_request(
    p2wp_firmware_core_t *core,
    p2wp_frame_t *request
) {
    size_t offset = 0u;
    size_t length = 0u;
    if (request->type == P2WP_TYPE_WIFI_CONNECT &&
        request->payload_length >= 2u) {
        offset = 2u;
        length = request->payload[1];
    } else if (request->type == P2WP_TYPE_WIFI_PROFILE_SAVE &&
               request->payload_length >= 1u) {
        offset = 1u;
        length = request->payload[0];
    }
    const size_t available = request->payload_length > offset
        ? request->payload_length - offset
        : 0u;
    if (length > available) {
        length = available;
    }
    volatile uint8_t *secret = request->payload + offset;
    while (length-- != 0u) {
        *secret++ = 0u;
    }
    if (offset != 0u && core->operations != NULL &&
        core->operations->clear_sensitive != NULL) {
        core->operations->clear_sensitive(core->platform_context);
    }
}

void p2wp_firmware_core_init(
    p2wp_firmware_core_t *core,
    const p2wp_firmware_operations_t *operations,
    void *platform_context,
    uint8_t hardware_model
) {
    memset(core, 0, sizeof(*core));
    core->operations = operations;
    core->platform_context = platform_context;
    core->hardware_model = hardware_model;
    core->protocol_minimum = P2WP_MIN_VERSION;
    core->protocol_maximum = P2WP_MAX_VERSION;
    p2wp_firmware_core_reset(core);
}

void p2wp_firmware_core_reset(p2wp_firmware_core_t *core) {
    core->session_version = P2WP_BOOTSTRAP_VERSION;
    core->session_valid = false;
    core->cached_request_valid = false;
}

void p2wp_firmware_core_set_protocol_range(
    p2wp_firmware_core_t *core,
    uint8_t minimum,
    uint8_t maximum
) {
    core->protocol_minimum = minimum;
    core->protocol_maximum = maximum;
    p2wp_firmware_core_reset(core);
}

void p2wp_firmware_core_handle(
    p2wp_firmware_core_t *core,
    p2wp_frame_t *request,
    p2wp_frame_t *response
) {
    bool cache_response = false;
    const uint16_t identity = p2wp_frame_identity(request);
    if (core->cached_request_valid &&
        request->type == core->cached_request_type &&
        request->sequence == core->cached_request_sequence &&
        identity == core->cached_request_identity) {
        *response = core->cached_response;
        goto finish;
    }

    if ((request->flags & P2WP_FLAG_RESPONSE) != 0u) {
        make_error(request, response, P2WP_ERROR_INVALID_PAYLOAD);
        goto finish;
    }
    if (request->type == P2WP_TYPE_HELLO) {
        if (request->version != P2WP_BOOTSTRAP_VERSION) {
            make_error(request, response, P2WP_ERROR_UNSUPPORTED_VERSION);
            goto finish;
        }
    } else if (!core->session_valid ||
               request->version != core->session_version) {
        make_error(request, response, P2WP_ERROR_UNSUPPORTED_VERSION);
        goto finish;
    }
    if (request->type != P2WP_TYPE_HELLO &&
        core->cached_request_valid &&
        request->sequence == core->cached_request_sequence) {
        make_error(request, response, P2WP_ERROR_SEQUENCE_CONFLICT);
        goto finish;
    }

    make_success(core, request, response);
    switch (request->type) {
        case P2WP_TYPE_HELLO: {
            if (!valid_hello(request)) {
                make_error(request, response, P2WP_ERROR_INVALID_PAYLOAD);
                break;
            }
            const uint8_t selected = p2wp_select_version(
                request->payload[4],
                request->payload[5],
                core->protocol_minimum,
                core->protocol_maximum
            );
            if (selected == 0u) {
                make_error(request, response, P2WP_ERROR_UNSUPPORTED_VERSION);
                break;
            }
            const uint16_t host_limit =
                (uint16_t)request->payload[6] |
                ((uint16_t)request->payload[7] << 8u);
            const uint16_t negotiated_limit = host_limit < P2WP_MAX_PAYLOAD
                ? host_limit
                : P2WP_MAX_PAYLOAD;
            if (negotiated_limit == 0u) {
                make_error(request, response, P2WP_ERROR_INVALID_PAYLOAD);
                break;
            }
            static const uint8_t magic[] = {'P', '2', 'W', 'P'};
            memcpy(response->payload, magic, sizeof(magic));
            response->payload[4] = selected;
            response->payload[5] = core->operations != NULL &&
                core->operations->capabilities != NULL
                    ? core->operations->capabilities(core->platform_context)
                    : P2WP_CAPABILITY_ECHO;
            response->payload[6] = (uint8_t)negotiated_limit;
            response->payload[7] = (uint8_t)(negotiated_limit >> 8u);
            response->payload_length = 8u;
            core->cached_request_valid = false;
            core->session_version = selected;
            core->session_valid = true;
            cache_response = true;
            break;
        }

        case P2WP_TYPE_DEVICE_INFO:
            if (!empty_request(request)) {
                make_error(request, response, P2WP_ERROR_INVALID_PAYLOAD);
                break;
            }
            response->payload[0] = core->hardware_model;
            response->payload[1] = P2WP_FIRMWARE_VERSION_MAJOR;
            response->payload[2] = P2WP_FIRMWARE_VERSION_MINOR;
            response->payload[3] = P2WP_FIRMWARE_VERSION_PATCH;
            response->payload_length = 4u;
            cache_response = true;
            break;

        case P2WP_TYPE_ECHO:
            response->payload_length = request->payload_length;
            if (request->payload_length != 0u) {
                memcpy(
                    response->payload,
                    request->payload,
                    request->payload_length
                );
            }
            cache_response = true;
            break;

        default: {
            const p2wp_firmware_command_fn command = command_for_type(
                core->operations,
                request->type
            );
            if (command == NULL) {
                make_error(request, response, P2WP_ERROR_UNKNOWN_TYPE);
                break;
            }
            if (!platform_payload_is_valid(request)) {
                make_error(request, response, P2WP_ERROR_INVALID_PAYLOAD);
                break;
            }
            const uint8_t error = command(
                core->platform_context,
                request,
                response
            );
            if (error != P2WP_FIRMWARE_COMMAND_OK) {
                make_error(request, response, error);
                break;
            }
            cache_response = true;
            break;
        }
    }

    if (cache_response) {
        core->cached_request_type = request->type;
        core->cached_request_sequence = request->sequence;
        core->cached_request_identity = identity;
        core->cached_response = *response;
        core->cached_request_valid = true;
    }

finish:
    clear_sensitive_request(core, request);
}

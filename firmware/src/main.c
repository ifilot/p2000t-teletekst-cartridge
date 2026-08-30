#include "p2wp.h"
#include "firmware_core.h"
#include "version.h"
#include "teletekst.h"
#include "wifi_profile.h"
#include "github_ca.h"
#include "nos_teletekst_ca.h"
#include "p2000t_teletekst_ca.h"

#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "lwip/altcp_tls.h"
#include "lwip/apps/http_client.h"
#include "lwip/dns.h"
#include "lwip/udp.h"
#include "mbedtls/ssl.h"
#include "mbedtls/platform_util.h"
#include "pico/cyw43_arch.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "pico/time.h"

enum {
    GPIO_HOST_DATA_BASE = 0,
    GPIO_PICO_DATA_BASE = 8,
    GPIO_TX_FULL = 16,
    GPIO_TX_CLEAR_N = 17,
    GPIO_RX_READY = 18,
    GPIO_RX_ACK_CLEAR_N = 19,
    GPIO_RX_ACK_PENDING = 20,
    GPIO_WIFI_UP = 21,
    GPIO_BUSY = 22,
    GPIO_ERROR = 26,
};

#define HOST_DATA_MASK 0x000000ffu
#define PICO_DATA_MASK 0x0000ff00u
#define BYTE_TIMEOUT_MS 1000u
#define PARTIAL_FRAME_TIMEOUT_MS 1000u
#define WIFI_DEADLINE_CHECK_MS 10u
#define WIFI_INIT_TIMEOUT_MS 15000u
#define WIFI_SCAN_TIMEOUT_MS 15000u
#define WIFI_SCAN_QUEUE_TIMEOUT_MS \
    (WIFI_INIT_TIMEOUT_MS + WIFI_SCAN_TIMEOUT_MS)
#define WIFI_CONNECT_TIMEOUT_MS 30000u
#define WIFI_FAILURE_CONFIRM_MS 2000u
#define WIFI_MAX_RESULTS 9u
#define WIFI_MAX_PASSWORD 63u
#define TELETEKST_NOS_HOST "teletekst-data.nos.nl"
#define TELETEKST_P2000T_HOST "teletekst.philips-p2000t.nl"
#define TELETEKST_NOS_PORT 443u
#define TELETEKST_P2000T_PORT 443u
#define TELETEKST_ROWS_PER_CHUNK 6u
#define TELETEKST_CHUNK_SIZE \
    (TELETEKST_COLUMNS * TELETEKST_ROWS_PER_CHUNK)
#define TELETEKST_CHUNK_COUNT \
    (TELETEKST_DISPLAY_ROWS / TELETEKST_ROWS_PER_CHUNK)
#define NTP_HOST "time.cloudflare.com"
#define NTP_PORT 123u
#define NTP_PACKET_SIZE 48u
#define NTP_UNIX_EPOCH_OFFSET 2208988800u
#define NTP_RETRY_MS 30000u
#define NTP_RESYNC_MS (6u * 60u * 60u * 1000u)
#define VERSION_HOST "api.github.com"
#define VERSION_PORT 443u
#define VERSION_PATH "/repos/ifilot/p2000t-teletekst-cartridge/releases/latest"
#define VERSION_HTTP_BODY_MAX 16384u

enum wifi_scan_state {
    WIFI_SCAN_IDLE = 0,
    WIFI_SCAN_RUNNING = 1,
    WIFI_SCAN_COMPLETE = 2,
    WIFI_SCAN_FAILED = 3,
};

enum wifi_init_state {
    WIFI_INIT_STARTING = 0,
    WIFI_INIT_READY = 1,
    WIFI_INIT_FAILED = 2,
};

enum wifi_connection_state {
    WIFI_DISCONNECTED = 0,
    WIFI_CONNECTING = 1,
    WIFI_CONNECTED = 2,
    WIFI_NO_NETWORK = 3,
    WIFI_BAD_AUTH = 4,
    WIFI_CONNECTION_FAILED = 5,
};

enum wifi_security {
    WIFI_SECURITY_OPEN = 0,
    WIFI_SECURITY_PSK = 1,
    WIFI_SECURITY_UNSUPPORTED = 2,
};

enum teletekst_fetch_state {
    TELETEKST_FETCH_IDLE = 0,
    TELETEKST_FETCH_CONNECTING = 1,
    TELETEKST_FETCH_RECEIVING = 2,
    TELETEKST_FETCH_COMPLETE = 3,
    TELETEKST_FETCH_FAILED = 4,
};

enum teletekst_error {
    TELETEKST_ERROR_NONE = 0,
    TELETEKST_ERROR_NOT_CONNECTED = 1,
    TELETEKST_ERROR_TLS_CONFIG = 2,
    TELETEKST_ERROR_REQUEST_START = 3,
    TELETEKST_ERROR_NETWORK = 4,
    TELETEKST_ERROR_HTTP_STATUS = 5,
    TELETEKST_ERROR_TOO_LARGE = 6,
    TELETEKST_ERROR_INVALID_DATA = 7,
    TELETEKST_ERROR_PAGE_NOT_FOUND = 8,
};

enum version_check_error {
    VERSION_CHECK_ERROR_NONE = 0,
    VERSION_CHECK_ERROR_NOT_CONNECTED = 1,
    VERSION_CHECK_ERROR_TLS_CONFIG = 2,
    VERSION_CHECK_ERROR_REQUEST_START = 3,
    VERSION_CHECK_ERROR_NETWORK = 4,
    VERSION_CHECK_ERROR_HTTP_STATUS = 5,
    VERSION_CHECK_ERROR_INVALID_DATA = 6,
};

enum wifi_profile_state {
    WIFI_PROFILE_STATE_ABSENT = 0,
    WIFI_PROFILE_STATE_READY = 1,
    WIFI_PROFILE_STATE_BUSY = 2,
};

enum wifi_profile_operation {
    WIFI_PROFILE_OPERATION_NONE = 0,
    WIFI_PROFILE_OPERATION_CONNECT = 1,
    WIFI_PROFILE_OPERATION_SAVE = 2,
    WIFI_PROFILE_OPERATION_DELETE = 3,
};

typedef struct {
    uint8_t ssid_length;
    uint8_t ssid[32];
    int16_t rssi;
    uint8_t security;
    uint32_t auth;
} wifi_result_t;

static p2wp_parser_t parser;
static p2wp_frame_t request;
static uint8_t encoded_response[P2WP_MAX_ENCODED];
static p2wp_firmware_core_t firmware_core;
static mutex_t wifi_mutex;
static uint8_t wifi_init_state;
static uint8_t wifi_scan_state;
static uint8_t wifi_connection_state;
static bool wifi_scan_requested;
static bool wifi_connect_requested;
static wifi_result_t wifi_results[WIFI_MAX_RESULTS];
static uint8_t wifi_result_count;
static absolute_time_t wifi_init_deadline;
static absolute_time_t wifi_scan_deadline;
static absolute_time_t wifi_connect_deadline;
static absolute_time_t wifi_failure_deadline;
static char wifi_ssid[33];
static char wifi_password[WIFI_MAX_PASSWORD + 1u];
static uint32_t wifi_connect_auth;
static uint8_t stored_profile_state;
static uint8_t stored_profile_error;
static uint8_t stored_profile_operation;
static uint8_t stored_profile_ssid[WIFI_PROFILE_MAX_SSID];
static uint8_t stored_profile_ssid_length;
static uint8_t stored_profile_password[WIFI_PROFILE_MAX_PASSWORD];
static uint8_t stored_profile_password_length;
static uint32_t stored_profile_auth;
static bool wifi_scan_active;
static bool wifi_connection_active;
static int wifi_pending_failure_status;
static uint8_t teletekst_fetch_state;
static uint8_t teletekst_error;
static bool teletekst_fetch_requested;
static bool teletekst_http_overflow;
static bool teletekst_tls_cleanup_pending;
static size_t teletekst_http_length;
static char teletekst_http_body[TELETEKST_HTTP_BODY_MAX + 1u];
static uint8_t teletekst_screen[TELETEKST_SCREEN_SIZE];
static uint16_t teletekst_requested_page;
static uint8_t teletekst_requested_subpage;
static uint8_t teletekst_requested_source;
static uint8_t teletekst_next_subpage;
static char teletekst_path[32];
static struct altcp_tls_config *teletekst_tls_config;
static const char *teletekst_tls_hostname;
static altcp_allocator_t teletekst_tls_allocator;
static httpc_connection_t teletekst_http_settings;
static struct udp_pcb *ntp_pcb;
static bool ntp_request_active;
static bool clock_valid;
static uint32_t clock_unix_seconds;
static absolute_time_t clock_reference;
static absolute_time_t ntp_next_attempt;
static absolute_time_t ntp_request_deadline;
static uint8_t version_check_state;
static uint8_t version_check_error;
static bool version_check_requested;
static bool version_http_overflow;
static bool version_tls_cleanup_pending;
static size_t version_http_length;
static char version_http_body[VERSION_HTTP_BODY_MAX + 1u];
static p2wp_release_version_t latest_release_version;
static struct altcp_tls_config *version_tls_config;
static altcp_allocator_t version_tls_allocator;
static httpc_connection_t version_http_settings;

#if defined(PICO_RP2350) && PICO_RP2350
static const uint8_t hardware_model = P2WP_HARDWARE_PICO_2_W;
#else
static const uint8_t hardware_model = P2WP_HARDWARE_PICO_W;
#endif

/** Return the synchronized Unix time, or zero until the first NTP reply. */
static uint32_t clock_now(void) {
    uint32_t seconds = 0u;
    mutex_enter_blocking(&wifi_mutex);
    if (clock_valid) {
        const int64_t elapsed = absolute_time_diff_us(
            clock_reference,
            get_absolute_time()
        );
        if (elapsed >= 0) {
            seconds = clock_unix_seconds + (uint32_t)(elapsed / 1000000);
        }
    }
    mutex_exit(&wifi_mutex);
    return seconds;
}

/** Convert a Unix timestamp to Dutch local date and time (CET/CEST). */
static bool clock_local_components(uint32_t unix_time, uint8_t result[7]) {
    if (unix_time == 0u) {
        memset(result, 0, 7u);
        return false;
    }
    const uint32_t days = unix_time / 86400u;
    uint32_t first_day = 0u;
    unsigned year = 1970u;
    while (true) {
        const bool leap = (year % 4u == 0u && year % 100u != 0u) ||
                          year % 400u == 0u;
        const uint32_t year_days = leap ? 366u : 365u;
        if (days < first_day + year_days) {
            break;
        }
        first_day += year_days;
        ++year;
    }
    const bool leap = (year % 4u == 0u && year % 100u != 0u) ||
                      year % 400u == 0u;
    // Sunday is zero; Unix day zero (1970-01-01) was Thursday.
    const uint32_t march_31 = first_day + 31u + 28u + (leap ? 1u : 0u) + 30u;
    const uint32_t october_31 = first_day + 304u;
    const uint32_t dst_start_day = march_31 - ((march_31 + 4u) % 7u);
    const uint32_t dst_end_day = october_31 - ((october_31 + 4u) % 7u);
    const uint32_t dst_start = dst_start_day * 86400u + 3600u;
    const uint32_t dst_end = dst_end_day * 86400u + 3600u;
    const uint32_t offset = 3600u +
        (unix_time >= dst_start && unix_time < dst_end ? 3600u : 0u);
    const uint32_t local_time = unix_time + offset;
    const uint32_t local_days = local_time / 86400u;
    const uint32_t day_seconds = local_time % 86400u;
    first_day = 0u;
    year = 1970u;
    while (true) {
        const bool local_leap =
            (year % 4u == 0u && year % 100u != 0u) || year % 400u == 0u;
        const uint32_t year_days = local_leap ? 366u : 365u;
        if (local_days < first_day + year_days) {
            break;
        }
        first_day += year_days;
        ++year;
    }
    const bool local_leap =
        (year % 4u == 0u && year % 100u != 0u) || year % 400u == 0u;
    static const uint8_t month_days[12] = {
        31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u
    };
    uint32_t day_of_year = local_days - first_day;
    unsigned month = 0u;
    while (month < 11u) {
        uint32_t days_in_month = month_days[month];
        if (month == 1u && local_leap) {
            ++days_in_month;
        }
        if (day_of_year < days_in_month) {
            break;
        }
        day_of_year -= days_in_month;
        ++month;
    }
    result[0] = (uint8_t)(day_seconds / 3600u);
    result[1] = (uint8_t)((day_seconds / 60u) % 60u);
    result[2] = (uint8_t)(day_seconds % 60u);
    result[3] = (uint8_t)(day_of_year + 1u);
    result[4] = (uint8_t)(month + 1u);
    result[5] = (uint8_t)(year - 2000u);
    // Sunday is zero; Unix day zero (1970-01-01) was Thursday.
    result[6] = (uint8_t)((local_days + 4u) % 7u);
    return true;
}

/** Send one minimal NTP client request to a resolved server address. */
static void ntp_send_request(const ip_addr_t *address) {
    if (ntp_pcb == NULL) {
        return;
    }
    struct pbuf *packet = pbuf_alloc(PBUF_TRANSPORT, NTP_PACKET_SIZE, PBUF_RAM);
    if (packet == NULL) {
        ntp_next_attempt = make_timeout_time_ms(NTP_RETRY_MS);
        return;
    }
    memset(packet->payload, 0, NTP_PACKET_SIZE);
    ((uint8_t *)packet->payload)[0] = 0x23u; // NTP v4 client request.
    if (udp_sendto(ntp_pcb, packet, address, NTP_PORT) == ERR_OK) {
        ntp_request_active = true;
        ntp_request_deadline = make_timeout_time_ms(NTP_RETRY_MS);
    } else {
        ntp_next_attempt = make_timeout_time_ms(NTP_RETRY_MS);
    }
    pbuf_free(packet);
}

/** Continue an NTP request after asynchronous DNS resolution. */
static void ntp_dns_found(const char *name, const ip_addr_t *address, void *arg) {
    (void)name;
    (void)arg;
    if (address == NULL) {
        ntp_request_active = false;
        ntp_next_attempt = make_timeout_time_ms(NTP_RETRY_MS);
        return;
    }
    ntp_send_request(address);
}

/** Accept an NTP transmit timestamp and establish the Pico's wall clock. */
static void ntp_receive(
    void *arg,
    struct udp_pcb *pcb,
    struct pbuf *packet,
    const ip_addr_t *address,
    u16_t port
) {
    (void)arg;
    (void)pcb;
    (void)address;
    (void)port;
    uint8_t timestamp[4];
    if (packet->tot_len >= NTP_PACKET_SIZE &&
        pbuf_copy_partial(packet, timestamp, sizeof(timestamp), 40u) ==
            sizeof(timestamp)) {
        const uint32_t ntp_seconds =
            ((uint32_t)timestamp[0] << 24u) |
            ((uint32_t)timestamp[1] << 16u) |
            ((uint32_t)timestamp[2] << 8u) |
            (uint32_t)timestamp[3];
        if (ntp_seconds >= NTP_UNIX_EPOCH_OFFSET) {
            mutex_enter_blocking(&wifi_mutex);
            clock_unix_seconds = ntp_seconds - NTP_UNIX_EPOCH_OFFSET;
            clock_reference = get_absolute_time();
            clock_valid = true;
            mutex_exit(&wifi_mutex);
            ntp_next_attempt = make_timeout_time_ms(NTP_RESYNC_MS);
        }
    }
    ntp_request_active = false;
    pbuf_free(packet);
}

/** Start or periodically resynchronize the wall clock while Wi-Fi is up. */
static void ntp_service(bool connected) {
    if (!connected) {
        return;
    }
    if (ntp_request_active && time_reached(ntp_request_deadline)) {
        ntp_request_active = false;
        ntp_next_attempt = make_timeout_time_ms(NTP_RETRY_MS);
    }
    if (ntp_request_active || !time_reached(ntp_next_attempt)) {
        return;
    }
    if (ntp_pcb == NULL) {
        ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
        if (ntp_pcb == NULL) {
            ntp_next_attempt = make_timeout_time_ms(NTP_RETRY_MS);
            return;
        }
        udp_recv(ntp_pcb, ntp_receive, NULL);
    }
    ip_addr_t address;
    const err_t result = dns_gethostbyname(NTP_HOST, &address, ntp_dns_found, NULL);
    if (result == ERR_OK) {
        ntp_send_request(&address);
    } else if (result != ERR_INPROGRESS) {
        ntp_next_attempt = make_timeout_time_ms(NTP_RETRY_MS);
    } else {
        ntp_request_active = true;
        ntp_request_deadline = make_timeout_time_ms(NTP_RETRY_MS);
    }
}

/**
 * @brief Publish a terminal Teletekst fetch error to the mailbox core.
 *
 * @param error TELETEKST_ERROR_* value describing the failure.
 */
static void teletekst_set_failed(uint8_t error) {
    mutex_enter_blocking(&wifi_mutex);
    teletekst_fetch_state = TELETEKST_FETCH_FAILED;
    teletekst_error = error;
    mutex_exit(&wifi_mutex);
}

/**
 * @brief Allocate an lwIP TLS connection and configure hostname validation.
 *
 * @param arg TLS configuration supplied through the lwIP allocator interface.
 * @param ip_type lwIP address-family selector.
 * @return Configured connection, or NULL when allocation/setup fails.
 */
static struct altcp_pcb *teletekst_tls_alloc(void *arg, u8_t ip_type) {
    struct altcp_pcb *connection = altcp_tls_alloc(arg, ip_type);
    if (connection == NULL) {
        return NULL;
    }
    mbedtls_ssl_context *context = altcp_tls_context(connection);
    if (context == NULL || teletekst_tls_hostname == NULL ||
        mbedtls_ssl_set_hostname(context, teletekst_tls_hostname) != 0) {
        altcp_abort(connection);
        return NULL;
    }
    return connection;
}

/**
 * @brief Record HTTP response metadata after lwIP has parsed the headers.
 *
 * @param connection Active HTTP client state.
 * @param argument Caller context; unused.
 * @param headers Header pbuf chain; unused after lwIP validation.
 * @param header_length Encoded header length; unused.
 * @param content_length Declared body length, or UINT32_MAX when unknown.
 * @return ERR_OK so lwIP continues delivering the response body.
 */
static err_t teletekst_headers_done(
    httpc_state_t *connection,
    void *argument,
    struct pbuf *headers,
    u16_t header_length,
    u32_t content_length
) {
    (void)connection;
    (void)argument;
    (void)headers;
    (void)header_length;

    mutex_enter_blocking(&wifi_mutex);
    teletekst_fetch_state = TELETEKST_FETCH_RECEIVING;
    if (content_length != UINT32_MAX &&
        content_length > TELETEKST_HTTP_BODY_MAX) {
        teletekst_http_overflow = true;
    }
    mutex_exit(&wifi_mutex);
    return ERR_OK;
}

/**
 * @brief Append one received pbuf chain to the bounded Teletekst body buffer.
 *
 * The callback always acknowledges and frees the pbuf. Overflow is remembered
 * so the completion callback can return error 06 without writing out of bounds.
 *
 * @param argument Caller context; unused.
 * @param connection Active TLS connection used for receive-window accounting.
 * @param data Received pbuf chain, or NULL when no data is supplied.
 * @param error lwIP receive status; completion handles the final result.
 * @return ERR_OK after consuming the supplied data.
 */
static err_t teletekst_receive(
    void *argument,
    struct altcp_pcb *connection,
    struct pbuf *data,
    err_t error
) {
    (void)argument;
    (void)error;
    if (data == NULL) {
        return ERR_OK;
    }

    mutex_enter_blocking(&wifi_mutex);
    if (data->tot_len > TELETEKST_HTTP_BODY_MAX - teletekst_http_length) {
        teletekst_http_overflow = true;
    } else {
        for (struct pbuf *part = data; part != NULL; part = part->next) {
            memcpy(
                teletekst_http_body + teletekst_http_length,
                part->payload,
                part->len
            );
            teletekst_http_length += part->len;
        }
    }
    teletekst_fetch_state = TELETEKST_FETCH_RECEIVING;
    mutex_exit(&wifi_mutex);

    altcp_recved(connection, data->tot_len);
    pbuf_free(data);
    return ERR_OK;
}

/**
 * @brief Finalize an asynchronous HTTP request and decode a successful body.
 *
 * @param argument Caller context; unused.
 * @param result lwIP HTTP client result.
 * @param received_length Length reported by lwIP; internal accounting is used.
 * @param server_status HTTP status code.
 * @param error lwIP error detail; @p result determines the public error.
 */
static void teletekst_request_done(
    void *argument,
    httpc_result_t result,
    u32_t received_length,
    u32_t server_status,
    err_t error
) {
    (void)argument;
    (void)received_length;
    (void)error;

    teletekst_tls_cleanup_pending = true;
    if (result != HTTPC_RESULT_OK) {
        teletekst_set_failed(TELETEKST_ERROR_NETWORK);
        return;
    }
    if (server_status == 404u) {
        teletekst_set_failed(TELETEKST_ERROR_PAGE_NOT_FOUND);
        return;
    }
    if (server_status != 200u) {
        teletekst_set_failed(TELETEKST_ERROR_HTTP_STATUS);
        return;
    }

    mutex_enter_blocking(&wifi_mutex);
    const bool overflow = teletekst_http_overflow;
    const size_t body_length = teletekst_http_length;
    const uint16_t page = teletekst_requested_page;
    mutex_exit(&wifi_mutex);
    if (overflow) {
        teletekst_set_failed(TELETEKST_ERROR_TOO_LARGE);
        return;
    }

    teletekst_http_body[body_length] = '\0';
    uint8_t next_subpage = 0u;
    if (!teletekst_decode_nos_json(
            teletekst_http_body,
            body_length,
            page,
            teletekst_screen,
            &next_subpage
        )) {
        teletekst_set_failed(TELETEKST_ERROR_INVALID_DATA);
        return;
    }

    mutex_enter_blocking(&wifi_mutex);
    teletekst_next_subpage = next_subpage;
    teletekst_error = TELETEKST_ERROR_NONE;
    teletekst_fetch_state = TELETEKST_FETCH_COMPLETE;
    mutex_exit(&wifi_mutex);
}

/**
 * @brief Free a completed request's TLS configuration outside its callback.
 *
 * lwIP still owns connection objects during the result callback, so cleanup is
 * deferred until cyw43_arch_poll() has returned.
 */
static void teletekst_cleanup_tls(void) {
    if (!teletekst_tls_cleanup_pending) {
        return;
    }
    teletekst_tls_cleanup_pending = false;
    if (teletekst_tls_config != NULL) {
        altcp_tls_free_config(teletekst_tls_config);
        teletekst_tls_config = NULL;
    }
}

/**
 * @brief Start a Teletekst request queued by the mailbox core.
 *
 * This core-0 routine selects the endpoint and trust root, formats the API
 * path, resets bounded body state, and registers asynchronous lwIP callbacks.
 */
static void teletekst_start_requested_fetch(void) {
    mutex_enter_blocking(&wifi_mutex);
    const bool requested = teletekst_fetch_requested;
    teletekst_fetch_requested = false;
    const bool connected = wifi_connection_state == WIFI_CONNECTED;
    const uint16_t page = teletekst_requested_page;
    const uint8_t subpage = teletekst_requested_subpage;
    const uint8_t source = teletekst_requested_source;
    mutex_exit(&wifi_mutex);
    if (!requested) {
        return;
    }
    if (!connected) {
        teletekst_set_failed(TELETEKST_ERROR_NOT_CONNECTED);
        return;
    }

    const int path_length = subpage == 0u
        ? snprintf(teletekst_path, sizeof(teletekst_path), "/json/%u", page)
        : snprintf(
            teletekst_path,
            sizeof(teletekst_path),
            "/json/%u-%u",
            page,
            subpage
        );
    if (path_length < 0 || (size_t)path_length >= sizeof(teletekst_path)) {
        teletekst_set_failed(TELETEKST_ERROR_INVALID_DATA);
        return;
    }

    memset(&teletekst_http_settings, 0, sizeof(teletekst_http_settings));
    const char *host;
    uint16_t port;
    const uint8_t *root_ca;
    size_t root_ca_size;
    if (source == P2WP_TELETEKST_SOURCE_NOS) {
        host = TELETEKST_NOS_HOST;
        port = TELETEKST_NOS_PORT;
        root_ca = nos_teletekst_root_ca;
        root_ca_size = sizeof(nos_teletekst_root_ca);
    } else {
        host = TELETEKST_P2000T_HOST;
        port = TELETEKST_P2000T_PORT;
        root_ca = p2000t_teletekst_root_ca;
        root_ca_size = sizeof(p2000t_teletekst_root_ca);
    }
    teletekst_tls_hostname = host;
    teletekst_tls_config = altcp_tls_create_config_client(
        root_ca,
        root_ca_size
    );
    if (teletekst_tls_config == NULL) {
        teletekst_set_failed(TELETEKST_ERROR_TLS_CONFIG);
        return;
    }
    teletekst_tls_allocator.alloc = teletekst_tls_alloc;
    teletekst_tls_allocator.arg = teletekst_tls_config;
    teletekst_http_settings.altcp_allocator = &teletekst_tls_allocator;
    teletekst_http_settings.result_fn = teletekst_request_done;
    teletekst_http_settings.headers_done_fn = teletekst_headers_done;

    mutex_enter_blocking(&wifi_mutex);
    teletekst_http_length = 0u;
    teletekst_http_overflow = false;
    mutex_exit(&wifi_mutex);
    const err_t result = httpc_get_file_dns(
        host,
        port,
        teletekst_path,
        &teletekst_http_settings,
        teletekst_receive,
        NULL,
        NULL
    );
    if (result != ERR_OK) {
        if (teletekst_tls_config != NULL) {
            altcp_tls_free_config(teletekst_tls_config);
            teletekst_tls_config = NULL;
        }
        teletekst_set_failed(TELETEKST_ERROR_REQUEST_START);
    }
}

/** Publish a terminal firmware-version lookup error. */
static void version_set_failed(uint8_t error) {
    mutex_enter_blocking(&wifi_mutex);
    version_check_state = P2WP_VERSION_CHECK_FAILED;
    version_check_error = error;
    mutex_exit(&wifi_mutex);
}

/** Allocate a GitHub TLS connection with hostname validation enabled. */
static struct altcp_pcb *version_tls_alloc(void *arg, u8_t ip_type) {
    struct altcp_pcb *connection = altcp_tls_alloc(arg, ip_type);
    if (connection == NULL) {
        return NULL;
    }
    mbedtls_ssl_context *context = altcp_tls_context(connection);
    if (context == NULL ||
        mbedtls_ssl_set_hostname(context, VERSION_HOST) != 0) {
        altcp_abort(connection);
        return NULL;
    }
    return connection;
}

/** Accept GitHub response headers; body storage is bounded in the receiver. */
static err_t version_headers_done(
    httpc_state_t *connection,
    void *argument,
    struct pbuf *headers,
    u16_t header_length,
    u32_t content_length
) {
    (void)connection;
    (void)argument;
    (void)headers;
    (void)header_length;
    (void)content_length;
    return ERR_OK;
}

/** Collect a bounded prefix of the GitHub release response. */
static err_t version_receive(
    void *argument,
    struct altcp_pcb *connection,
    struct pbuf *data,
    err_t error
) {
    (void)argument;
    (void)error;
    if (data == NULL) {
        return ERR_OK;
    }

    mutex_enter_blocking(&wifi_mutex);
    if (data->tot_len > VERSION_HTTP_BODY_MAX - version_http_length) {
        version_http_overflow = true;
    } else {
        for (struct pbuf *part = data; part != NULL; part = part->next) {
            memcpy(
                version_http_body + version_http_length,
                part->payload,
                part->len
            );
            version_http_length += part->len;
        }
    }
    mutex_exit(&wifi_mutex);

    altcp_recved(connection, data->tot_len);
    pbuf_free(data);
    return ERR_OK;
}

/** Validate the GitHub response and publish its latest semantic version. */
static void version_request_done(
    void *argument,
    httpc_result_t result,
    u32_t received_length,
    u32_t server_status,
    err_t error
) {
    (void)argument;
    (void)received_length;
    (void)error;
    version_tls_cleanup_pending = true;
    if (result != HTTPC_RESULT_OK) {
        version_set_failed(VERSION_CHECK_ERROR_NETWORK);
        return;
    }
    if (server_status != 200u) {
        version_set_failed(VERSION_CHECK_ERROR_HTTP_STATUS);
        return;
    }

    mutex_enter_blocking(&wifi_mutex);
    const size_t body_length = version_http_length;
    mutex_exit(&wifi_mutex);
    version_http_body[body_length] = '\0';
    p2wp_release_version_t parsed;
    if (!p2wp_parse_latest_release(
            version_http_body,
            body_length,
            &parsed
        )) {
        version_set_failed(VERSION_CHECK_ERROR_INVALID_DATA);
        return;
    }

    mutex_enter_blocking(&wifi_mutex);
    latest_release_version = parsed;
    version_check_error = VERSION_CHECK_ERROR_NONE;
    version_check_state = P2WP_VERSION_CHECK_COMPLETE;
    mutex_exit(&wifi_mutex);
}

/** Free version-check TLS state after lwIP releases its connection. */
static void version_cleanup_tls(void) {
    if (!version_tls_cleanup_pending) {
        return;
    }
    version_tls_cleanup_pending = false;
    if (version_tls_config != NULL) {
        altcp_tls_free_config(version_tls_config);
        version_tls_config = NULL;
    }
}

/** Start an explicitly queued latest-release lookup. */
static void version_start_requested_check(void) {
    mutex_enter_blocking(&wifi_mutex);
    const bool requested = version_check_requested;
    version_check_requested = false;
    const bool connected = wifi_connection_state == WIFI_CONNECTED;
    mutex_exit(&wifi_mutex);
    if (!requested) {
        return;
    }
    if (!connected) {
        version_set_failed(VERSION_CHECK_ERROR_NOT_CONNECTED);
        return;
    }

    memset(&version_http_settings, 0, sizeof(version_http_settings));
    version_tls_config = altcp_tls_create_config_client(
        github_root_ca,
        sizeof(github_root_ca)
    );
    if (version_tls_config == NULL) {
        version_set_failed(VERSION_CHECK_ERROR_TLS_CONFIG);
        return;
    }
    version_tls_allocator.alloc = version_tls_alloc;
    version_tls_allocator.arg = version_tls_config;
    version_http_settings.altcp_allocator = &version_tls_allocator;
    version_http_settings.result_fn = version_request_done;
    version_http_settings.headers_done_fn = version_headers_done;

    mutex_enter_blocking(&wifi_mutex);
    version_http_length = 0u;
    version_http_overflow = false;
    mutex_exit(&wifi_mutex);
    const err_t result = httpc_get_file_dns(
        VERSION_HOST,
        VERSION_PORT,
        VERSION_PATH,
        &version_http_settings,
        version_receive,
        NULL,
        NULL
    );
    if (result != ERR_OK) {
        altcp_tls_free_config(version_tls_config);
        version_tls_config = NULL;
        version_set_failed(VERSION_CHECK_ERROR_REQUEST_START);
    }
}

/**
 * @brief Reduce a CYW43 scan authentication bit mask to the P2WP security enum.
 *
 * @param auth_mode Authentication flags returned by the CYW43 scan.
 * @return WIFI_SECURITY_OPEN, WIFI_SECURITY_PSK, or WIFI_SECURITY_UNSUPPORTED.
 */
static uint8_t wifi_security_from_auth(uint8_t auth_mode) {
    if (auth_mode == 0u) {
        return WIFI_SECURITY_OPEN;
    }
    if ((auth_mode & (2u | 4u)) != 0u) {
        return WIFI_SECURITY_PSK;
    }
    return WIFI_SECURITY_UNSUPPORTED;
}

/**
 * @brief Select the CYW43 connection mode corresponding to scan flags.
 *
 * @param auth_mode Authentication flags returned by the CYW43 scan.
 * @return CYW43_AUTH_* value suitable for a connection request.
 */
static uint32_t wifi_auth_from_scan(uint8_t auth_mode) {
    if ((auth_mode & 4u) != 0u) {
        return (auth_mode & 2u) != 0u
            ? CYW43_AUTH_WPA2_MIXED_PSK
            : CYW43_AUTH_WPA2_AES_PSK;
    }
    if ((auth_mode & 2u) != 0u) {
        return CYW43_AUTH_WPA_TKIP_PSK;
    }
    return CYW43_AUTH_OPEN;
}

/**
 * @brief Collect, deduplicate, and rank one asynchronous Wi-Fi scan result.
 *
 * Duplicate SSIDs retain the strongest access point. When the bounded result
 * array is full, a stronger new result replaces the weakest entry.
 *
 * @param environment Scan callback context; unused.
 * @param result Result supplied by CYW43, or NULL at callback completion.
 * @return Zero as required by the CYW43 scan callback contract.
 */
static int wifi_scan_callback(
    void *environment,
    const cyw43_ev_scan_result_t *result
) {
    (void)environment;
    if (result == NULL || result->ssid_len == 0u || result->ssid_len > 32u) {
        return 0;
    }

    mutex_enter_blocking(&wifi_mutex);
    if (wifi_scan_state != WIFI_SCAN_RUNNING) {
        mutex_exit(&wifi_mutex);
        return 0;
    }
    uint8_t index = 0u;
    for (; index < wifi_result_count; ++index) {
        if (wifi_results[index].ssid_length == result->ssid_len &&
            memcmp(wifi_results[index].ssid, result->ssid, result->ssid_len) == 0) {
            if (result->rssi <= wifi_results[index].rssi) {
                mutex_exit(&wifi_mutex);
                return 0;
            }
            break;
        }
    }

    if (index == wifi_result_count) {
        if (wifi_result_count < WIFI_MAX_RESULTS) {
            ++wifi_result_count;
        } else {
            uint8_t weakest = 0u;
            for (uint8_t candidate = 1u; candidate < WIFI_MAX_RESULTS; ++candidate) {
                if (wifi_results[candidate].rssi < wifi_results[weakest].rssi) {
                    weakest = candidate;
                }
            }
            if (result->rssi <= wifi_results[weakest].rssi) {
                mutex_exit(&wifi_mutex);
                return 0;
            }
            index = weakest;
        }
    }

    wifi_result_t *destination = &wifi_results[index];
    destination->ssid_length = result->ssid_len;
    memcpy(destination->ssid, result->ssid, result->ssid_len);
    destination->rssi = result->rssi;
    destination->security = wifi_security_from_auth(result->auth_mode);
    destination->auth = wifi_auth_from_scan(result->auth_mode);
    mutex_exit(&wifi_mutex);
    return 0;
}

/**
 * @brief Sort collected Wi-Fi results by descending signal strength.
 */
static void wifi_sort_results(void) {
    for (uint8_t index = 1u; index < wifi_result_count; ++index) {
        wifi_result_t value = wifi_results[index];
        uint8_t position = index;
        while (position != 0u && wifi_results[position - 1u].rssi < value.rssi) {
            wifi_results[position] = wifi_results[position - 1u];
            --position;
        }
        wifi_results[position] = value;
    }
}

/**
 * @brief Poll CYW43/lwIP and advance scan and connection state machines.
 *
 * This function runs only on core 0 because the polled CYW43 context and its
 * callbacks are core-affine.
 */
static void wifi_service(void) {
    cyw43_arch_poll();
    // A completed HTTP callback runs inside poll(). The connection has been
    // released when poll returns, so its TLS configuration is now safe to free.
    teletekst_cleanup_tls();
    version_cleanup_tls();

    if (wifi_scan_active && !cyw43_wifi_scan_active(&cyw43_state)) {
        mutex_enter_blocking(&wifi_mutex);
        if (wifi_scan_state == WIFI_SCAN_RUNNING) {
            wifi_sort_results();
            wifi_scan_state = WIFI_SCAN_COMPLETE;
        }
        mutex_exit(&wifi_mutex);
        wifi_scan_active = false;
    }

    mutex_enter_blocking(&wifi_mutex);
    const uint8_t connection_state = wifi_connection_state;
    mutex_exit(&wifi_mutex);

    if (wifi_connection_active && connection_state == WIFI_CONNECTING) {
        const int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
        uint8_t new_state = WIFI_CONNECTING;
        if (status == CYW43_LINK_UP) {
            new_state = WIFI_CONNECTED;
        } else if (time_reached(wifi_connect_deadline)) {
            new_state = WIFI_CONNECTION_FAILED;
        } else if (status == CYW43_LINK_NONET ||
                   status == CYW43_LINK_BADAUTH ||
                   status == CYW43_LINK_FAIL) {
            // A failed authentication event can be followed by a successful
            // one while the CYW43 driver is still joining. Do not expose a
            // transient negative link state as a final result.
            if (wifi_pending_failure_status != status) {
                wifi_pending_failure_status = status;
                wifi_failure_deadline =
                    make_timeout_time_ms(WIFI_FAILURE_CONFIRM_MS);
            } else if (time_reached(wifi_failure_deadline)) {
                if (status == CYW43_LINK_NONET) {
                    new_state = WIFI_NO_NETWORK;
                } else if (status == CYW43_LINK_BADAUTH) {
                    new_state = WIFI_BAD_AUTH;
                } else {
                    new_state = WIFI_CONNECTION_FAILED;
                }
            }
        } else {
            wifi_pending_failure_status = 0;
        }

        if (new_state != WIFI_CONNECTING) {
            // Return the driver to a clean station state after a completed
            // failure so a cartridge-side retry can start immediately.
            if (new_state != WIFI_CONNECTED) {
                (void)cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
            }
            wifi_pending_failure_status = 0;
            mutex_enter_blocking(&wifi_mutex);
            wifi_connection_state = new_state;
            mutex_exit(&wifi_mutex);
            wifi_connection_active = new_state == WIFI_CONNECTED;
        }
    } else if (wifi_connection_active && connection_state == WIFI_CONNECTED &&
               cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) !=
                   CYW43_LINK_UP) {
        mutex_enter_blocking(&wifi_mutex);
        wifi_connection_state = WIFI_DISCONNECTED;
        mutex_exit(&wifi_mutex);
        wifi_connection_active = false;
    }

    mutex_enter_blocking(&wifi_mutex);
    const bool connected = wifi_connection_state == WIFI_CONNECTED;
    mutex_exit(&wifi_mutex);
    ntp_service(connected);
    gpio_put(GPIO_WIFI_UP, connected);
}

/**
 * @brief Start a Wi-Fi scan queued by the mailbox core.
 *
 * A failed driver call is converted into the public WIFI_SCAN_FAILED state.
 */
static void wifi_start_requested_scan(void) {
    mutex_enter_blocking(&wifi_mutex);
    const bool requested = wifi_scan_requested;
    wifi_scan_requested = false;
    mutex_exit(&wifi_mutex);
    if (!requested) {
        return;
    }

    cyw43_wifi_scan_options_t options = {0};
    if (cyw43_wifi_scan(
            &cyw43_state,
            &options,
            NULL,
            wifi_scan_callback
        ) == 0) {
        mutex_enter_blocking(&wifi_mutex);
        wifi_scan_deadline = make_timeout_time_ms(WIFI_SCAN_TIMEOUT_MS);
        mutex_exit(&wifi_mutex);
        wifi_scan_active = true;
        return;
    }

    mutex_enter_blocking(&wifi_mutex);
    wifi_scan_state = WIFI_SCAN_FAILED;
    mutex_exit(&wifi_mutex);
}

/**
 * @brief Start an asynchronous Wi-Fi connection queued by the mailbox core.
 *
 * Passwords are copied under the mutex and wiped from both shared and local
 * storage as soon as the CYW43 API has consumed them.
 */
static void wifi_start_requested_connection(void) {
    char ssid[sizeof(wifi_ssid)];
    char password[sizeof(wifi_password)];
    uint32_t auth;

    mutex_enter_blocking(&wifi_mutex);
    const bool requested = wifi_connect_requested;
    wifi_connect_requested = false;
    if (requested) {
        memcpy(ssid, wifi_ssid, sizeof(ssid));
        memcpy(password, wifi_password, sizeof(password));
        auth = wifi_connect_auth;
        mbedtls_platform_zeroize(wifi_password, sizeof(wifi_password));
    }
    mutex_exit(&wifi_mutex);
    if (!requested) {
        return;
    }

    const char *password_argument = password[0] == '\0' ? NULL : password;
    const int result = cyw43_arch_wifi_connect_async(
        ssid,
        password_argument,
        auth
    );
    mbedtls_platform_zeroize(password, sizeof(password));
    if (result == 0) {
        wifi_connect_deadline = make_timeout_time_ms(WIFI_CONNECT_TIMEOUT_MS);
        wifi_pending_failure_status = 0;
        wifi_connection_active = true;
        return;
    }

    mutex_enter_blocking(&wifi_mutex);
    wifi_connection_state = WIFI_CONNECTION_FAILED;
    mutex_exit(&wifi_mutex);
}

/**
 * @brief Execute one queued encrypted-profile operation on core 0.
 *
 * Flash-safe execution requires cooperation from both cores. This routine also
 * wipes temporary plaintext credentials and publishes the resulting state.
 */
static void wifi_profile_service(void) {
    uint8_t ssid[WIFI_PROFILE_MAX_SSID] = {0};
    uint8_t password[WIFI_PROFILE_MAX_PASSWORD] = {0};
    uint8_t ssid_length = 0u;
    uint8_t password_length = 0u;
    uint32_t auth = 0u;

    mutex_enter_blocking(&wifi_mutex);
    const uint8_t operation = stored_profile_operation;
    if (operation == WIFI_PROFILE_OPERATION_NONE) {
        mutex_exit(&wifi_mutex);
        return;
    }
    stored_profile_operation = WIFI_PROFILE_OPERATION_NONE;
    if (operation == WIFI_PROFILE_OPERATION_SAVE) {
        ssid_length = stored_profile_ssid_length;
        password_length = stored_profile_password_length;
        auth = stored_profile_auth;
        memcpy(ssid, stored_profile_ssid, ssid_length);
        memcpy(password, stored_profile_password, password_length);
    }
    mbedtls_platform_zeroize(
        stored_profile_password,
        sizeof(stored_profile_password)
    );
    stored_profile_password_length = 0u;
    mutex_exit(&wifi_mutex);

    wifi_profile_result_t result = WIFI_PROFILE_INVALID_DATA;
    if (operation == WIFI_PROFILE_OPERATION_CONNECT) {
        wifi_profile_credentials_t credentials;
        result = wifi_profile_load(&credentials);
        if (result == WIFI_PROFILE_OK) {
            mutex_enter_blocking(&wifi_mutex);
            memcpy(wifi_ssid, credentials.ssid, credentials.ssid_length + 1u);
            memcpy(
                wifi_password,
                credentials.password,
                credentials.password_length + 1u
            );
            wifi_connect_auth = credentials.auth;
            wifi_connection_state = WIFI_CONNECTING;
            wifi_connect_requested = true;
            mutex_exit(&wifi_mutex);
            gpio_put(GPIO_WIFI_UP, false);
        }
        mbedtls_platform_zeroize(&credentials, sizeof(credentials));
    } else if (operation == WIFI_PROFILE_OPERATION_SAVE) {
        result = wifi_profile_store(
            ssid,
            ssid_length,
            password,
            password_length,
            auth
        );
    } else if (operation == WIFI_PROFILE_OPERATION_DELETE) {
        result = wifi_profile_erase();
    }

    memset(ssid, 0, sizeof(ssid));
    mbedtls_platform_zeroize(password, sizeof(password));
    mutex_enter_blocking(&wifi_mutex);
    stored_profile_error = result;
    stored_profile_state = wifi_profile_present()
        ? WIFI_PROFILE_STATE_READY
        : WIFI_PROFILE_STATE_ABSENT;
    mutex_exit(&wifi_mutex);
}

/**
 * @brief Initialize CYW43 and run the permanent core-0 network service loop.
 *
 * The polled CYW43 context is core-affine. Initialization, callbacks, and all
 * subsequent Wi-Fi API calls therefore remain on core 0. Core 1 independently
 * services the P2000 mailbox.
 */
static void wifi_radio_main(void) {
    bool available =
        cyw43_arch_init_with_country(CYW43_COUNTRY_NETHERLANDS) == 0;
    if (available) {
        cyw43_arch_enable_sta_mode();
        // cyw43_arch_enable_sta_mode() returns void, including when the
        // underlying radio setup fails. Check that the STA interface really
        // came up before advertising the driver as ready.
        available = (cyw43_state.itf_state & (1u << CYW43_ITF_STA)) != 0u;
    }

    mutex_enter_blocking(&wifi_mutex);
    // Core 0 may already have declared a startup timeout while the blocking
    // radio bring-up was in progress. Do not turn that failure back into a
    // late success; a new session/reset gives initialization a clean retry.
    if (wifi_init_state != WIFI_INIT_FAILED) {
        wifi_init_state = available ? WIFI_INIT_READY : WIFI_INIT_FAILED;
    } else {
        available = false;
    }
    if (!available) {
        wifi_scan_state = WIFI_SCAN_FAILED;
        wifi_connection_state = WIFI_CONNECTION_FAILED;
        teletekst_fetch_state = TELETEKST_FETCH_FAILED;
        teletekst_error = TELETEKST_ERROR_NOT_CONNECTED;
        wifi_scan_requested = false;
        wifi_connect_requested = false;
        teletekst_fetch_requested = false;
        version_check_requested = false;
    }
    mutex_exit(&wifi_mutex);

    if (!available) {
        gpio_put(GPIO_WIFI_UP, false);
        while (true) {
            tight_loop_contents();
        }
    }

    while (true) {
        // Poll first so completed HTTP requests release their old TLS
        // configuration before a newly queued fetch can allocate another one.
        wifi_service();
        wifi_profile_service();
        wifi_start_requested_scan();
        wifi_start_requested_connection();
        version_start_requested_check();
        teletekst_start_requested_fetch();
        tight_loop_contents();
    }
}

/**
 * @brief Enforce radio-initialization and queued-scan deadlines from core 1.
 *
 * This keeps local-link status requests responsive even if core 0 is blocked
 * inside CYW43 initialization.
 */
static void wifi_check_deadlines(void) {
    mutex_enter_blocking(&wifi_mutex);
    if (wifi_init_state == WIFI_INIT_STARTING &&
        time_reached(wifi_init_deadline)) {
        wifi_init_state = WIFI_INIT_FAILED;
        if (wifi_scan_state == WIFI_SCAN_RUNNING) {
            wifi_scan_state = WIFI_SCAN_FAILED;
        }
        wifi_connection_state = WIFI_CONNECTION_FAILED;
        teletekst_fetch_state = TELETEKST_FETCH_FAILED;
        teletekst_error = TELETEKST_ERROR_NOT_CONNECTED;
        wifi_scan_requested = false;
        wifi_connect_requested = false;
        teletekst_fetch_requested = false;
        version_check_requested = false;
    } else if (wifi_scan_state == WIFI_SCAN_RUNNING &&
               time_reached(wifi_scan_deadline)) {
        wifi_scan_state = WIFI_SCAN_FAILED;
        wifi_scan_requested = false;
    }
    const bool failed = wifi_init_state == WIFI_INIT_FAILED;
    mutex_exit(&wifi_mutex);

    if (failed) {
        gpio_put(GPIO_WIFI_UP, false);
    }
}

/**
 * @brief Wait for a GPIO input to reach a requested logic level.
 *
 * @param gpio GPIO number to sample.
 * @param value Expected logic level.
 * @param timeout_ms Maximum wait in milliseconds.
 * @return true when the level is observed before the deadline.
 */
static bool wait_for_gpio(uint gpio, bool value, uint32_t timeout_ms) {
    const absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    while (gpio_get(gpio) != value) {
        if (time_reached(deadline)) {
            return false;
        }
        tight_loop_contents();
    }
    return true;
}

/**
 * @brief Initialize one GPIO output without producing an unwanted pulse.
 *
 * @param gpio GPIO number to configure.
 * @param initial_value Logic level applied before output mode is enabled.
 */
static void init_output(uint gpio, bool initial_value) {
    gpio_init(gpio);
    gpio_put(gpio, initial_value);
    gpio_set_dir(gpio, GPIO_OUT);
}

/**
 * @brief Configure the parallel mailbox bus and clear stale handshake state.
 *
 * Data directions follow the external latches: GPIO0-7 are host-to-Pico input,
 * while GPIO8-15 are Pico-to-host output.
 */
static void mailbox_init(void) {
    for (uint gpio = GPIO_HOST_DATA_BASE; gpio < GPIO_HOST_DATA_BASE + 8u; ++gpio) {
        gpio_init(gpio);
        gpio_set_dir(gpio, GPIO_IN);
        gpio_disable_pulls(gpio);
    }

    for (uint gpio = GPIO_PICO_DATA_BASE; gpio < GPIO_PICO_DATA_BASE + 8u; ++gpio) {
        init_output(gpio, false);
    }

    gpio_init(GPIO_TX_FULL);
    gpio_set_dir(GPIO_TX_FULL, GPIO_IN);
    gpio_disable_pulls(GPIO_TX_FULL);

    gpio_init(GPIO_RX_ACK_PENDING);
    gpio_set_dir(GPIO_RX_ACK_PENDING, GPIO_IN);
    gpio_disable_pulls(GPIO_RX_ACK_PENDING);

    init_output(GPIO_TX_CLEAR_N, true);
    init_output(GPIO_RX_READY, false);
    init_output(GPIO_RX_ACK_CLEAR_N, true);
    init_output(GPIO_WIFI_UP, false);
    init_output(GPIO_BUSY, false);
    init_output(GPIO_ERROR, false);

    // Clear bytes or acknowledgements left behind by a Pico-only reset.
    gpio_put(GPIO_TX_CLEAR_N, false);
    gpio_put(GPIO_RX_ACK_CLEAR_N, false);
    sleep_us(10);
    gpio_put(GPIO_TX_CLEAR_N, true);
    gpio_put(GPIO_RX_ACK_CLEAR_N, true);
}

/**
 * @brief Non-blockingly receive one latched host-to-Pico byte.
 *
 * @param[out] byte Destination for the sampled data bus.
 * @return true when a byte was present and acknowledged.
 */
static bool mailbox_try_receive(uint8_t *byte) {
    if (!gpio_get(GPIO_TX_FULL)) {
        return false;
    }

    // The byte latch and TX_FULL are clocked by the same /WR edge. A short
    // settling interval also keeps an asynchronous edge away from the sample.
    busy_wait_us_32(1);
    *byte = (uint8_t)(gpio_get_all() & HOST_DATA_MASK);

    gpio_put(GPIO_TX_CLEAR_N, false);
    (void)wait_for_gpio(GPIO_TX_FULL, false, 1);
    gpio_put(GPIO_TX_CLEAR_N, true);
    return true;
}

/**
 * @brief Send one byte to the P2000T using the acknowledged GPIO handshake.
 *
 * @param byte Value to drive on the Pico-to-host data bus.
 * @return true after the host acknowledges and releases the byte.
 */
static bool mailbox_send_byte(uint8_t byte) {
    if (!wait_for_gpio(GPIO_RX_ACK_PENDING, false, BYTE_TIMEOUT_MS)) {
        return false;
    }

    gpio_put_masked(PICO_DATA_MASK, (uint32_t)byte << GPIO_PICO_DATA_BASE);
    busy_wait_us_32(1);
    gpio_put(GPIO_RX_READY, true);

    if (!wait_for_gpio(GPIO_RX_ACK_PENDING, true, BYTE_TIMEOUT_MS)) {
        gpio_put(GPIO_RX_READY, false);
        return false;
    }

    // READY must fall before the acknowledgement is cleared, otherwise the
    // host could accept the same byte for a second time.
    gpio_put(GPIO_RX_READY, false);
    gpio_put(GPIO_RX_ACK_CLEAR_N, false);
    const bool cleared = wait_for_gpio(GPIO_RX_ACK_PENDING, false, 1);
    gpio_put(GPIO_RX_ACK_CLEAR_N, true);
    return cleared;
}

/**
 * @brief Send a complete encoded frame over the byte mailbox.
 *
 * @param data Encoded frame bytes.
 * @param length Number of bytes to send.
 * @return false when any byte handshake times out.
 */
static bool mailbox_send(const uint8_t *data, size_t length) {
    for (size_t index = 0; index < length; ++index) {
        if (!mailbox_send_byte(data[index])) {
            return false;
        }
    }
    return true;
}


static uint8_t pico_capabilities(void *context) {
    (void)context;
    mutex_enter_blocking(&wifi_mutex);
    const bool wifi_capable = wifi_init_state != WIFI_INIT_FAILED;
    mutex_exit(&wifi_mutex);
    return P2WP_CAPABILITY_ECHO |
        P2WP_CAPABILITY_DEVICE_INFO |
        P2WP_CAPABILITY_VERSION_CHECK |
        (wifi_capable
            ? P2WP_CAPABILITY_WIFI | P2WP_CAPABILITY_INTERNET |
                P2WP_CAPABILITY_WIFI_PROFILE
            : 0u);
}

static uint8_t pico_version_check_start(
    void *context,
    const p2wp_frame_t *frame,
    p2wp_frame_t *response
) {
    (void)context;
    (void)frame;
    mutex_enter_blocking(&wifi_mutex);
    version_check_state = P2WP_VERSION_CHECK_RUNNING;
    version_check_error = VERSION_CHECK_ERROR_NONE;
    memset(&latest_release_version, 0, sizeof(latest_release_version));
    version_check_requested = true;
    mutex_exit(&wifi_mutex);
    response->payload_length = 0u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t pico_version_check_status(
    void *context,
    const p2wp_frame_t *frame,
    p2wp_frame_t *response
) {
    (void)context;
    (void)frame;
    mutex_enter_blocking(&wifi_mutex);
    response->payload[0] = version_check_state;
    response->payload[1] = version_check_error;
    response->payload[2] = latest_release_version.major;
    response->payload[3] = latest_release_version.minor;
    response->payload[4] = latest_release_version.patch;
    mutex_exit(&wifi_mutex);
    response->payload_length = 5u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t pico_wifi_scan_start(
    void *context,
    const p2wp_frame_t *frame,
    p2wp_frame_t *response
) {
    (void)context;
    (void)frame;
    mutex_enter_blocking(&wifi_mutex);
    if (wifi_init_state == WIFI_INIT_FAILED) {
        mutex_exit(&wifi_mutex);
        return P2WP_ERROR_WIFI_UNAVAILABLE;
    }
    if (wifi_scan_state == WIFI_SCAN_RUNNING) {
        mutex_exit(&wifi_mutex);
        return P2WP_ERROR_WIFI_BUSY;
    }
    wifi_result_count = 0u;
    wifi_scan_state = WIFI_SCAN_RUNNING;
    wifi_scan_deadline = make_timeout_time_ms(WIFI_SCAN_QUEUE_TIMEOUT_MS);
    wifi_scan_requested = true;
    mutex_exit(&wifi_mutex);
    response->payload_length = 0u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t pico_wifi_scan_status(
    void *context,
    const p2wp_frame_t *frame,
    p2wp_frame_t *response
) {
    (void)context;
    (void)frame;
    mutex_enter_blocking(&wifi_mutex);
    response->payload[0] = wifi_scan_state;
    response->payload[1] = wifi_result_count;
    response->payload[2] = wifi_init_state;
    mutex_exit(&wifi_mutex);
    response->payload_length = 3u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t pico_wifi_scan_result(
    void *context,
    const p2wp_frame_t *frame,
    p2wp_frame_t *response
) {
    (void)context;
    mutex_enter_blocking(&wifi_mutex);
    if (wifi_init_state == WIFI_INIT_FAILED) {
        mutex_exit(&wifi_mutex);
        return P2WP_ERROR_WIFI_UNAVAILABLE;
    }
    if (wifi_scan_state != WIFI_SCAN_COMPLETE ||
        frame->payload[0] >= wifi_result_count) {
        mutex_exit(&wifi_mutex);
        return P2WP_ERROR_INVALID_PAYLOAD;
    }
    const uint8_t index = frame->payload[0];
    const wifi_result_t *result = &wifi_results[index];
    response->payload[0] = index;
    int16_t rssi = result->rssi;
    if (rssi < -127) {
        rssi = -127;
    } else if (rssi > 0) {
        rssi = 0;
    }
    response->payload[1] = (uint8_t)(int8_t)rssi;
    response->payload[2] = result->security;
    response->payload[3] = result->ssid_length;
    memcpy(response->payload + 4u, result->ssid, result->ssid_length);
    response->payload_length = 4u + result->ssid_length;
    mutex_exit(&wifi_mutex);
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t pico_wifi_connect(
    void *context,
    const p2wp_frame_t *frame,
    p2wp_frame_t *response
) {
    (void)context;
    mutex_enter_blocking(&wifi_mutex);
    if (wifi_init_state == WIFI_INIT_FAILED) {
        mutex_exit(&wifi_mutex);
        return P2WP_ERROR_WIFI_UNAVAILABLE;
    }
    if (wifi_scan_state != WIFI_SCAN_COMPLETE ||
        frame->payload[0] >= wifi_result_count) {
        mutex_exit(&wifi_mutex);
        return P2WP_ERROR_INVALID_PAYLOAD;
    }
    const wifi_result_t *result = &wifi_results[frame->payload[0]];
    const uint8_t password_length = frame->payload[1];
    if (result->security == WIFI_SECURITY_UNSUPPORTED ||
        (result->security == WIFI_SECURITY_OPEN && password_length != 0u) ||
        (result->security == WIFI_SECURITY_PSK &&
         (password_length < 8u || password_length > WIFI_MAX_PASSWORD))) {
        mutex_exit(&wifi_mutex);
        return P2WP_ERROR_INVALID_PAYLOAD;
    }
    memcpy(wifi_ssid, result->ssid, result->ssid_length);
    wifi_ssid[result->ssid_length] = '\0';
    memcpy(wifi_password, frame->payload + 2u, password_length);
    wifi_password[password_length] = '\0';
    wifi_connect_auth = result->auth;
    wifi_connection_state = WIFI_CONNECTING;
    wifi_connect_requested = true;
    mutex_exit(&wifi_mutex);
    gpio_put(GPIO_WIFI_UP, false);
    response->payload_length = 0u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t pico_wifi_status(
    void *context,
    const p2wp_frame_t *frame,
    p2wp_frame_t *response
) {
    (void)context;
    (void)frame;
    mutex_enter_blocking(&wifi_mutex);
    if (wifi_init_state == WIFI_INIT_FAILED) {
        mutex_exit(&wifi_mutex);
        return P2WP_ERROR_WIFI_UNAVAILABLE;
    }
    response->payload[0] = wifi_connection_state;
    mutex_exit(&wifi_mutex);
    response->payload_length = 1u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t pico_wifi_profile_status(
    void *context,
    const p2wp_frame_t *frame,
    p2wp_frame_t *response
) {
    (void)context;
    (void)frame;
    mutex_enter_blocking(&wifi_mutex);
    response->payload[0] = stored_profile_state;
    response->payload[1] = stored_profile_error;
    mutex_exit(&wifi_mutex);
    response->payload_length = 2u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t pico_wifi_profile_connect(
    void *context,
    const p2wp_frame_t *frame,
    p2wp_frame_t *response
) {
    (void)context;
    (void)frame;
    mutex_enter_blocking(&wifi_mutex);
    if (stored_profile_state != WIFI_PROFILE_STATE_READY ||
        stored_profile_operation != WIFI_PROFILE_OPERATION_NONE) {
        mutex_exit(&wifi_mutex);
        return P2WP_ERROR_WIFI_BUSY;
    }
    if (wifi_connection_state == WIFI_CONNECTED) {
        stored_profile_error = WIFI_PROFILE_OK;
        mutex_exit(&wifi_mutex);
        response->payload_length = 0u;
        return P2WP_FIRMWARE_COMMAND_OK;
    }
    stored_profile_error = WIFI_PROFILE_OK;
    stored_profile_state = WIFI_PROFILE_STATE_BUSY;
    stored_profile_operation = WIFI_PROFILE_OPERATION_CONNECT;
    mutex_exit(&wifi_mutex);
    response->payload_length = 0u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t pico_wifi_profile_save(
    void *context,
    const p2wp_frame_t *frame,
    p2wp_frame_t *response
) {
    (void)context;
    mutex_enter_blocking(&wifi_mutex);
    if (stored_profile_state == WIFI_PROFILE_STATE_BUSY ||
        stored_profile_operation != WIFI_PROFILE_OPERATION_NONE) {
        mutex_exit(&wifi_mutex);
        return P2WP_ERROR_WIFI_BUSY;
    }
    if (wifi_connection_state != WIFI_CONNECTED) {
        mutex_exit(&wifi_mutex);
        return P2WP_ERROR_WIFI_UNAVAILABLE;
    }
    stored_profile_password_length = frame->payload[0];
    stored_profile_ssid_length = (uint8_t)strlen(wifi_ssid);
    stored_profile_auth = wifi_connect_auth;
    memcpy(
        stored_profile_password,
        frame->payload + 1u,
        stored_profile_password_length
    );
    memcpy(stored_profile_ssid, wifi_ssid, stored_profile_ssid_length);
    stored_profile_error = WIFI_PROFILE_OK;
    stored_profile_state = WIFI_PROFILE_STATE_BUSY;
    stored_profile_operation = WIFI_PROFILE_OPERATION_SAVE;
    mutex_exit(&wifi_mutex);
    response->payload_length = 0u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t pico_wifi_profile_delete(
    void *context,
    const p2wp_frame_t *frame,
    p2wp_frame_t *response
) {
    (void)context;
    (void)frame;
    mutex_enter_blocking(&wifi_mutex);
    if (stored_profile_state == WIFI_PROFILE_STATE_BUSY ||
        stored_profile_operation != WIFI_PROFILE_OPERATION_NONE) {
        mutex_exit(&wifi_mutex);
        return P2WP_ERROR_WIFI_BUSY;
    }
    stored_profile_error = WIFI_PROFILE_OK;
    stored_profile_state = WIFI_PROFILE_STATE_BUSY;
    stored_profile_operation = WIFI_PROFILE_OPERATION_DELETE;
    mutex_exit(&wifi_mutex);
    response->payload_length = 0u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t pico_teletekst_fetch_start(
    void *context,
    const p2wp_frame_t *frame,
    p2wp_frame_t *response
) {
    (void)context;
    const uint16_t page =
        (uint16_t)frame->payload[0] |
        ((uint16_t)frame->payload[1] << 8u);
    mutex_enter_blocking(&wifi_mutex);
    if (wifi_init_state == WIFI_INIT_FAILED ||
        wifi_connection_state != WIFI_CONNECTED) {
        mutex_exit(&wifi_mutex);
        return P2WP_ERROR_WIFI_UNAVAILABLE;
    }
    if (teletekst_fetch_requested ||
        teletekst_fetch_state == TELETEKST_FETCH_CONNECTING ||
        teletekst_fetch_state == TELETEKST_FETCH_RECEIVING) {
        mutex_exit(&wifi_mutex);
        return P2WP_ERROR_WIFI_BUSY;
    }
    teletekst_requested_page = page;
    teletekst_requested_subpage = frame->payload[2];
    teletekst_requested_source = frame->payload[3];
    teletekst_next_subpage = 0u;
    teletekst_http_length = 0u;
    teletekst_http_overflow = false;
    teletekst_error = TELETEKST_ERROR_NONE;
    teletekst_fetch_state = TELETEKST_FETCH_CONNECTING;
    teletekst_fetch_requested = true;
    mutex_exit(&wifi_mutex);
    response->payload_length = 0u;
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t pico_teletekst_fetch_status(
    void *context,
    const p2wp_frame_t *frame,
    p2wp_frame_t *response
) {
    (void)context;
    mutex_enter_blocking(&wifi_mutex);
    response->payload[0] = teletekst_fetch_state;
    response->payload[1] = teletekst_error;
    size_t received = teletekst_http_length;
    if (received > UINT16_MAX) {
        received = UINT16_MAX;
    }
    response->payload[2] = (uint8_t)received;
    response->payload[3] = (uint8_t)(received >> 8u);
    response->payload[4] = teletekst_next_subpage;
    mutex_exit(&wifi_mutex);
    if (frame->version >= 3u) {
        uint8_t local_clock[7];
        const bool clock_is_valid = clock_local_components(
            clock_now(),
            local_clock
        );
        memcpy(response->payload + 5u, local_clock, 3u);
        response->payload[8] = clock_is_valid ? 1u : 0u;
        memcpy(response->payload + 9u, local_clock + 3u, 4u);
        response->payload_length = 13u;
    } else {
        response->payload_length = 5u;
    }
    return P2WP_FIRMWARE_COMMAND_OK;
}

static uint8_t pico_teletekst_fetch_rows(
    void *context,
    const p2wp_frame_t *frame,
    p2wp_frame_t *response
) {
    (void)context;
    mutex_enter_blocking(&wifi_mutex);
    if (teletekst_fetch_state != TELETEKST_FETCH_COMPLETE) {
        mutex_exit(&wifi_mutex);
        return P2WP_ERROR_INVALID_PAYLOAD;
    }
    response->payload_length = TELETEKST_CHUNK_SIZE;
    memcpy(
        response->payload,
        teletekst_screen +
            (size_t)frame->payload[0] * TELETEKST_CHUNK_SIZE,
        TELETEKST_CHUNK_SIZE
    );
    mutex_exit(&wifi_mutex);
    return P2WP_FIRMWARE_COMMAND_OK;
}

static void pico_clear_sensitive(void *context) {
    (void)context;
    mbedtls_platform_zeroize(parser.body, sizeof(parser.body));
}

static const p2wp_firmware_operations_t pico_firmware_operations = {
    .capabilities = pico_capabilities,
    .version_check_start = pico_version_check_start,
    .version_check_status = pico_version_check_status,
    .wifi_scan_start = pico_wifi_scan_start,
    .wifi_scan_status = pico_wifi_scan_status,
    .wifi_scan_result = pico_wifi_scan_result,
    .wifi_connect = pico_wifi_connect,
    .wifi_status = pico_wifi_status,
    .wifi_profile_status = pico_wifi_profile_status,
    .wifi_profile_connect = pico_wifi_profile_connect,
    .wifi_profile_save = pico_wifi_profile_save,
    .wifi_profile_delete = pico_wifi_profile_delete,
    .teletekst_fetch_start = pico_teletekst_fetch_start,
    .teletekst_fetch_status = pico_teletekst_fetch_status,
    .teletekst_fetch_rows = pico_teletekst_fetch_rows,
    .clear_sensitive = pico_clear_sensitive,
};

static void handle_request(p2wp_frame_t *frame) {
    p2wp_frame_t response;
    p2wp_firmware_core_handle(&firmware_core, frame, &response);
    const size_t length = p2wp_encode(
        &response,
        encoded_response,
        sizeof(encoded_response)
    );
    if (length == 0u || !mailbox_send(encoded_response, length)) {
        gpio_put(GPIO_ERROR, true);
    }
}

/**
 * @brief Run the permanent core-1 mailbox and protocol service loop.
 *
 * Partial frames are discarded after a finite timeout so a reset or interrupted
 * transaction cannot leave the streaming parser wedged indefinitely.
 */
static void mailbox_core_main(void) {
    if (!flash_safe_execute_core_init()) {
        gpio_put(GPIO_ERROR, true);
    }
    absolute_time_t last_byte_time = get_absolute_time();
    absolute_time_t next_wifi_deadline_check = get_absolute_time();

    while (true) {
        if (time_reached(next_wifi_deadline_check)) {
            wifi_check_deadlines();
            next_wifi_deadline_check =
                make_timeout_time_ms(WIFI_DEADLINE_CHECK_MS);
        }
        uint8_t byte;
        if (mailbox_try_receive(&byte)) {
            last_byte_time = get_absolute_time();
            const p2wp_parse_result_t result =
                p2wp_parser_feed(&parser, byte, &request);

            if (result == P2WP_PARSE_ERROR) {
                gpio_put(GPIO_ERROR, true);
            } else if (result == P2WP_PARSE_FRAME) {
                gpio_put(GPIO_ERROR, false);
                gpio_put(GPIO_BUSY, true);
                handle_request(&request);
                gpio_put(GPIO_BUSY, false);
            }
        } else if (parser.active && parser.length != 0u &&
                   absolute_time_diff_us(last_byte_time, get_absolute_time()) >
                       (int64_t)PARTIAL_FRAME_TIMEOUT_MS * 1000) {
            p2wp_parser_init(&parser);
            gpio_put(GPIO_ERROR, true);
        } else {
            tight_loop_contents();
        }
    }
}

/**
 * @brief Initialize shared state, launch the mailbox core, and service Wi-Fi.
 *
 * @return This firmware entry point does not return.
 */
int main(void) {
    mailbox_init();
    mutex_init(&wifi_mutex);
    p2wp_firmware_core_init(
        &firmware_core,
        &pico_firmware_operations,
        NULL,
        hardware_model
    );
    version_check_state = P2WP_VERSION_CHECK_IDLE;
    version_check_error = VERSION_CHECK_ERROR_NONE;
    wifi_init_state = WIFI_INIT_STARTING;
    stored_profile_state = wifi_profile_present()
        ? WIFI_PROFILE_STATE_READY
        : WIFI_PROFILE_STATE_ABSENT;
    stored_profile_error = WIFI_PROFILE_OK;
    wifi_init_deadline = make_timeout_time_ms(WIFI_INIT_TIMEOUT_MS);
    p2wp_parser_init(&parser);

    // Start the local link before the blocking radio bring-up. This keeps
    // HELLO and status polling responsive even if CYW43 hardware is absent.
    multicore_launch_core1(mailbox_core_main);
    wifi_radio_main();
}

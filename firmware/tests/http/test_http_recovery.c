/**
 * @file test_http_recovery.c
 * @brief Exercise production Pico fetch handlers against the SDK HTTP client.
 *
 * DNS, TCP, TLS and Pico hardware are deterministic adapters. HTTP parsing,
 * request ownership, timeout handling and page decoding use production code.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#define main pico_main
#include "../../src/main.c"
#undef main
#include "lwip/priv/altcp_priv.h"
#include <stdlib.h>
int64_t test_now;
static struct altcp_pcb *test_connection;
static dns_found_callback pending_dns;
static void *pending_dns_arg;
static bool resolve_immediately;
static ip_addr_t address;
static unsigned test_port;
static unsigned allocations;
static unsigned frees;
static unsigned tls_allocations;
static unsigned tls_frees;
static err_t connect_error;
static err_t dns_error;
static char last_request[256];
static void test_abort(struct altcp_pcb *pcb) {
    altcp_err_fn callback = pcb->err;
    void *arg = pcb->arg;
    free(pcb);
    frees++;
    test_connection = NULL;
    if (callback)
        callback(arg, ERR_ABRT);
}
static err_t test_close(struct altcp_pcb *pcb) {
    free(pcb);
    frees++;
    test_connection = NULL;
    return ERR_OK;
}
static err_t test_connect(struct altcp_pcb *pcb, const ip_addr_t *addr,
                          u16_t port, altcp_connected_fn callback) {
    (void)addr;
    pcb->connected = callback;
    test_port = port;
    return connect_error;
}
static void test_recved(struct altcp_pcb *pcb, u16_t len) {
    (void)pcb;
    (void)len;
}
static void test_poll(struct altcp_pcb *pcb, u8_t interval) {
    (void)pcb;
    (void)interval;
}
static err_t test_write(struct altcp_pcb *pcb, const void *data, u16_t len,
                        u8_t flags) {
    (void)pcb;
    (void)flags;
    assert(len < sizeof(last_request));
    memcpy(last_request, data, len);
    last_request[len] = '\0';
    return ERR_OK;
}
static err_t test_output(struct altcp_pcb *pcb) {
    (void)pcb;
    return ERR_OK;
}
static const struct altcp_functions test_functions = {
    .abort = test_abort,
    .close = test_close,
    .connect = test_connect,
    .recved = test_recved,
    .set_poll = test_poll,
    .write = test_write,
    .output = test_output,
};
struct altcp_pcb *altcp_tcp_new_ip_type(u8_t type) {
    (void)type;
    assert(test_connection == NULL);
    test_connection = calloc(1, sizeof(*test_connection));
    test_connection->fns = &test_functions;
    allocations++;
    return test_connection;
}
struct altcp_pcb *altcp_tcp_alloc(void *arg, u8_t type) {
    (void)arg;
    return altcp_tcp_new_ip_type(type);
}
struct altcp_pcb *altcp_tls_alloc(void *arg, u8_t type) {
    (void)arg;
    return altcp_tcp_new_ip_type(type);
}
void *altcp_tls_context(struct altcp_pcb *pcb) { return pcb; }
struct altcp_tls_config *altcp_tls_create_config_client(const u8_t *ca,
                                                        size_t length) {
    (void)ca;
    (void)length;
    tls_allocations++;
    return (struct altcp_tls_config *)malloc(1);
}
void altcp_tls_free_config(struct altcp_tls_config *config) {
    tls_frees++;
    free(config);
}
int mbedtls_ssl_set_hostname(mbedtls_ssl_context *ctx, const char *host) {
    (void)ctx;
    (void)host;
    return 0;
}
void mbedtls_ssl_set_hs_authmode(mbedtls_ssl_context *ctx, int mode) {
    (void)ctx;
    (void)mode;
}
err_t dns_gethostbyname(const char *host, ip_addr_t *out,
                        dns_found_callback callback, void *arg) {
    (void)host;
    if (dns_error != ERR_OK)
        return dns_error;
    if (resolve_immediately) {
        *out = address;
        return ERR_OK;
    }
    pending_dns = callback;
    pending_dns_arg = arg;
    return ERR_INPROGRESS;
}
static void start(const char *url) {
    p2wp_frame_t request = {.version = 8, .payload_length = 5 + strlen(url)};
    p2wp_frame_t reply;
    request.payload[0] = 100;
    request.payload[3] = P2WP_TELETEKST_SOURCE_CUSTOM;
    request.payload[4] = strlen(url);
    memcpy(request.payload + 5, url, strlen(url));
    assert(pico_teletekst_fetch_start(NULL, &request, &reply) ==
           P2WP_FIRMWARE_COMMAND_OK);
    teletekst_cleanup_tls();
    teletekst_start_requested_fetch();
}
static void timeout(void) {
    test_now += (int64_t)TELETEKST_FETCH_TIMEOUT_MS * 1000;
    teletekst_check_fetch_deadline();
    teletekst_cleanup_tls();
    assert(teletekst_fetch_state == TELETEKST_FETCH_FAILED);
    assert(teletekst_error == TELETEKST_ERROR_TIMEOUT);
    assert(!teletekst_http_active);
    assert(allocations == frees);
    assert(tls_allocations == tls_frees);
}
/** Deliver a real HTTP response through lwIP's parser and firmware decoder. */
static void complete(void) {
    struct altcp_pcb *pcb = test_connection;
    assert(pcb != NULL);
    assert(pcb->connected(pcb->arg, pcb, ERR_OK) == ERR_OK);
    assert(strstr(last_request, "GET /json/100 HTTP/1.1") != NULL);
    assert(strstr(last_request, "Host: server") != NULL);
    char body[1400];
    size_t size = (size_t)sprintf(
        body,
        "{\"nextSubPage\":\"\",\"nextPage\":\"101\",\"binaryDisplay\":\"");
    for (unsigned i = 0; i < 320; ++i) {
        memcpy(body + size, "ICAg", 4);
        size += 4;
    }
    memcpy(body + size, "\"}", 2);
    size += 2;
    char response[1600];
    int header_size =
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", size);
    assert(header_size > 0);
    memcpy(response + header_size, body, size);
    size += (size_t)header_size;
    struct pbuf *data = pbuf_alloc(PBUF_RAW, size, PBUF_RAM);
    assert(data != NULL);
    memcpy(data->payload, response, size);
    assert(pcb->recv(pcb->arg, pcb, data, ERR_OK) == ERR_OK);
    assert(pcb->recv(pcb->arg, pcb, NULL, ERR_OK) == ERR_OK);
    teletekst_cleanup_tls();
    assert(teletekst_fetch_state == TELETEKST_FETCH_COMPLETE);
    assert(teletekst_error == TELETEKST_ERROR_NONE);
    assert(teletekst_next_page == 101);
    for (unsigned i = 0; i < sizeof(teletekst_screen); ++i)
        assert(teletekst_screen[i] == ' ');
    assert(!teletekst_http_active);
    assert(test_connection == NULL);
    assert(allocations == frees);
    assert(tls_allocations == tls_frees);
}

/** An expired DNS result must not connect to the old port or change the page.
 */
static void test_dns_timeout(const char *scheme, bool new_dns_pending,
                             bool found) {
    char url[64];
    resolve_immediately = false;
    snprintf(url, sizeof(url), "%s://server:8082", scheme);
    start(url);
    dns_found_callback old_callback = pending_dns;
    void *old_arg = pending_dns_arg;
    timeout();
    old_callback("server", found ? &address : NULL, old_arg);
    assert(test_connection == NULL);
    assert(teletekst_fetch_state == TELETEKST_FETCH_FAILED);
    resolve_immediately = !new_dns_pending;
    snprintf(url, sizeof(url), "%s://server:8080", scheme);
    start(url);
    old_callback("server", found ? &address : NULL, old_arg);
    assert(teletekst_fetch_state == TELETEKST_FETCH_CONNECTING);
    assert(teletekst_http_active);
    if (new_dns_pending) {
        assert(test_connection == NULL);
        resolve_immediately =
            true; // lwIP caches the answer before its callback.
        pending_dns("server", &address, pending_dns_arg);
    }
    assert(test_port == 8080);
    complete();
    old_callback("server", found ? &address : NULL, old_arg);
    assert(teletekst_fetch_state == TELETEKST_FETCH_COMPLETE);
    assert(teletekst_next_page == 101);
}

int main(void) {
    mutex_init(&wifi_mutex);
    wifi_init_state = WIFI_INIT_READY;
    wifi_connection_state = WIFI_CONNECTED;
    IP4_ADDR(&address, 192, 168, 1, 2);
    for (unsigned tls = 0; tls < 2; ++tls) {
        const char *scheme = tls ? "https" : "http";
        char bad_url[64], good_url[64];
        snprintf(bad_url, sizeof(bad_url), "%s://server:8082", scheme);
        snprintf(good_url, sizeof(good_url), "%s://server:8080", scheme);
        resolve_immediately = true;
        // A silently dropped connection to the wrong port must expire cleanly.
        start(bad_url);
        assert(test_port == 8082);
        timeout();
        start(good_url);
        assert(test_port == 8080);
        complete();
        // A refused connection follows lwIP's error callback, not its deadline.
        start(bad_url);
        altcp_err_fn callback = test_connection->err;
        void *arg = test_connection->arg;
        free(test_connection);
        frees++;
        test_connection = NULL;
        callback(arg, ERR_RST);
        teletekst_cleanup_tls();
        assert(teletekst_fetch_state == TELETEKST_FETCH_FAILED);
        assert(teletekst_error == TELETEKST_ERROR_CONNECTION_CLOSED);
        start(good_url);
        complete();
        // Synchronous failure must release HTTP and TLS allocations as well.
        connect_error = ERR_CONN;
        start(bad_url);
        assert(teletekst_fetch_state == TELETEKST_FETCH_FAILED);
        assert(teletekst_error == TELETEKST_ERROR_CONNECT);
        connect_error = ERR_OK;
        start(good_url);
        complete();
        for (unsigned pending = 0; pending < 2; ++pending) {
            for (unsigned found = 0; found < 2; ++found) {
                test_dns_timeout(scheme, pending, found);
            }
        }
        // An asynchronous DNS failure must release the fetch before retrying.
        resolve_immediately = false;
        start(bad_url);
        pending_dns("server", NULL, pending_dns_arg);
        teletekst_cleanup_tls();
        assert(teletekst_error == TELETEKST_ERROR_DNS);
        resolve_immediately = true;
        start(good_url);
        complete();
        dns_error = ERR_MEM;
        start(bad_url);
        assert(teletekst_error == TELETEKST_ERROR_OUT_OF_MEMORY);
        dns_error = ERR_OK;
        start(good_url);
        complete();
    }
    puts("HTTP recovery tests passed (HTTP/TLS, port correction, DNS, late "
         "replies)");
}

void mbedtls_platform_zeroize(void *p, size_t n) { memset(p, 0, n); }
void gpio_put(uint gpio, bool value) {
    (void)gpio;
    (void)value;
}
custom_url_store_result_t custom_url_store_settings_load(uint8_t *source) {
    *source = 0xff;
    return CUSTOM_URL_STORE_NOT_FOUND;
}
custom_url_store_result_t
custom_url_store_load(char url[CUSTOM_ENDPOINT_URL_MAX + 1u], uint8_t *length) {
    url[0] = 0;
    *length = 0;
    return CUSTOM_URL_STORE_NOT_FOUND;
}

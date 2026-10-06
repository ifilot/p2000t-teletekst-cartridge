#ifndef TEST_PLATFORM_H
#define TEST_PLATFORM_H
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
typedef unsigned uint;
typedef int64_t absolute_time_t;
typedef struct {
    bool locked;
} mutex_t;
extern int64_t test_now;
static inline void mutex_init(mutex_t *m) { m->locked = false; }
static inline void mutex_enter_blocking(mutex_t *m) {
    assert(!m->locked);
    m->locked = true;
}
static inline void mutex_exit(mutex_t *m) {
    assert(m->locked);
    m->locked = false;
}
static inline absolute_time_t get_absolute_time(void) { return test_now; }
static inline absolute_time_t make_timeout_time_ms(uint32_t ms) {
    return test_now + (int64_t)ms * 1000;
}
static inline bool time_reached(absolute_time_t t) { return test_now >= t; }
static inline int64_t absolute_time_diff_us(absolute_time_t a,
                                            absolute_time_t b) {
    return b - a;
}
#define GPIO_IN 0
#define GPIO_OUT 1
void gpio_init(uint gpio);
void gpio_put(uint gpio, bool value);
void gpio_set_dir(uint gpio, bool value);
void gpio_disable_pulls(uint gpio);
bool gpio_get(uint gpio);
uint32_t gpio_get_all(void);
void gpio_put_masked(uint32_t mask, uint32_t value);
void sleep_us(uint64_t us);
void busy_wait_us_32(uint32_t us);
void tight_loop_contents(void);
bool flash_safe_execute_core_init(void);
void multicore_launch_core1(void (*entry)(void));
#define CYW43_AUTH_OPEN 0u
#define CYW43_AUTH_WPA_TKIP_PSK 1u
#define CYW43_AUTH_WPA2_AES_PSK 2u
#define CYW43_AUTH_WPA2_MIXED_PSK 3u
#define CYW43_COUNTRY_NETHERLANDS 0u
#define CYW43_ITF_STA 0u
#define CYW43_LINK_UP 1
#define CYW43_LINK_BADAUTH -1
#define CYW43_LINK_FAIL -2
#define CYW43_LINK_NONET -3
typedef struct {
    uint8_t ssid_len;
    uint8_t ssid[32];
    int16_t rssi;
    uint32_t auth_mode;
} cyw43_ev_scan_result_t;
typedef struct {
    int unused;
} cyw43_wifi_scan_options_t;
typedef struct {
    uint32_t itf_state;
} cyw43_t;
extern cyw43_t cyw43_state;
int cyw43_arch_init_with_country(uint32_t country);
void cyw43_arch_enable_sta_mode(void);
void cyw43_arch_poll(void);
int cyw43_tcpip_link_status(cyw43_t *state, int interface);
int cyw43_wifi_scan_active(cyw43_t *state);
int cyw43_wifi_scan(cyw43_t *state, const cyw43_wifi_scan_options_t *options,
                    void *arg,
                    int (*cb)(void *, const cyw43_ev_scan_result_t *));
int cyw43_arch_wifi_connect_async(const char *ssid, const char *password,
                                  uint32_t auth);
int cyw43_wifi_leave(cyw43_t *state, int interface);
#endif

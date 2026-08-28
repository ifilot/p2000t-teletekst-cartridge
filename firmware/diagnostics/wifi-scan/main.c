#include <stdio.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/time.h"

#define SCAN_TIMEOUT_MS 15000u

static unsigned scan_result_count;

/**
 * @brief Print and count one CYW43 scan result.
 *
 * @param environment Scan callback context; unused.
 * @param result Access point result, or NULL when no result is supplied.
 * @return Zero as required by the CYW43 callback contract.
 */
static int scan_result(void *environment, const cyw43_ev_scan_result_t *result) {
    (void)environment;
    if (result != NULL) {
        ++scan_result_count;
        printf(
            "ssid: %.*s  rssi: %d  channel: %u  auth: %u\n",
            result->ssid_len,
            result->ssid,
            result->rssi,
            result->channel,
            result->auth_mode
        );
    }
    return 0;
}

/**
 * @brief Repeatedly report a terminal diagnostic state and blink the LED.
 *
 * @param status Text printed every two seconds.
 * @param rapid_blink true for a failure blink, false for a steady-ready LED.
 */
static void report_forever(const char *status, bool rapid_blink) {
    bool led = false;
    absolute_time_t next_report = get_absolute_time();
    absolute_time_t next_blink = get_absolute_time();

    while (true) {
        cyw43_arch_poll();
        if (time_reached(next_blink)) {
            led = rapid_blink ? !led : true;
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led);
            next_blink = make_timeout_time_ms(rapid_blink ? 125u : 1000u);
        }
        if (time_reached(next_report)) {
            printf("%s; results=%u\n", status, scan_result_count);
            next_report = make_timeout_time_ms(2000u);
        }
        sleep_ms(1);
    }
}

/**
 * @brief Initialize CYW43, run one scan, and report the terminal result.
 *
 * @return This interactive diagnostic does not return.
 */
int main(void) {
    stdio_init_all();

    // Do not discard the one-time CYW43 bring-up messages before a terminal
    // has opened the USB CDC port. The diagnostic deliberately waits here.
    while (!stdio_usb_connected()) {
        tight_loop_contents();
    }
    sleep_ms(100);
    printf("P2WP standalone Pico W scan diagnostic\n");
    printf("Pico SDK: %s; country: NL\n", PICO_SDK_VERSION_STRING);
    printf("Press SPACE to initialize the radio and start the scan...\n");
    fflush(stdout);

    while (true) {
        const int input = getchar_timeout_us(10000u);
        if (input == ' ') {
            break;
        }
    }
    printf("SPACE received; starting CYW43 initialization\n");

    const int init_result =
        cyw43_arch_init_with_country(CYW43_COUNTRY_NETHERLANDS);
    printf("cyw43_arch_init_with_country returned %d\n", init_result);
    if (init_result != 0) {
        while (true) {
            printf("CYW43 DRIVER INITIALIZATION FAILED: %d\n", init_result);
            sleep_ms(2000);
        }
    }

    cyw43_arch_enable_sta_mode();
    const bool station_ready =
        (cyw43_state.itf_state & (1u << CYW43_ITF_STA)) != 0u;
    printf("station interface ready: %s\n", station_ready ? "yes" : "no");
    if (!station_ready) {
        report_forever("CYW43 STATION START FAILED", true);
    }

    cyw43_wifi_scan_options_t options = {0};
    const int scan_result_code =
        cyw43_wifi_scan(&cyw43_state, &options, NULL, scan_result);
    printf("cyw43_wifi_scan returned %d\n", scan_result_code);
    if (scan_result_code != 0) {
        report_forever("SCAN START FAILED", true);
    }

    bool led = false;
    absolute_time_t next_blink = get_absolute_time();
    const absolute_time_t deadline = make_timeout_time_ms(SCAN_TIMEOUT_MS);
    while (cyw43_wifi_scan_active(&cyw43_state) && !time_reached(deadline)) {
        cyw43_arch_poll();
        if (time_reached(next_blink)) {
            led = !led;
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led);
            next_blink = make_timeout_time_ms(500u);
        }
        sleep_ms(1);
    }

    if (cyw43_wifi_scan_active(&cyw43_state)) {
        report_forever("SCAN TIMED OUT", true);
    }
    if (scan_result_count == 0u) {
        report_forever("SCAN COMPLETE WITH ZERO RESULTS", true);
    }
    report_forever("SCAN COMPLETE", false);
}

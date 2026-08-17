#ifndef P2WP_WIFI_PROFILE_H
#define P2WP_WIFI_PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WIFI_PROFILE_MAX_SSID 32u
#define WIFI_PROFILE_MAX_PASSWORD 63u

typedef enum {
    WIFI_PROFILE_OK = 0,
    WIFI_PROFILE_NOT_FOUND = 1,
    WIFI_PROFILE_CORRUPT = 2,
    WIFI_PROFILE_STORAGE_FAILED = 3,
    WIFI_PROFILE_INVALID_DATA = 4,
} wifi_profile_result_t;

typedef struct {
    uint8_t ssid_length;
    char ssid[WIFI_PROFILE_MAX_SSID + 1u];
    uint8_t password_length;
    char password[WIFI_PROFILE_MAX_PASSWORD + 1u];
    uint32_t auth;
} wifi_profile_credentials_t;

bool wifi_profile_present(void);

wifi_profile_result_t wifi_profile_load(wifi_profile_credentials_t *credentials);

wifi_profile_result_t wifi_profile_store(
    const uint8_t *ssid,
    size_t ssid_length,
    const uint8_t *password,
    size_t password_length,
    uint32_t auth
);

wifi_profile_result_t wifi_profile_erase(void);

#endif

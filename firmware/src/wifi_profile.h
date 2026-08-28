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

/**
 * @brief Check whether the reserved flash sector contains a profile record.
 *
 * This is a quick magic-value check; wifi_profile_load() performs full
 * authentication and structural validation.
 *
 * @return true when a candidate record is present.
 */
bool wifi_profile_present(void);

/**
 * @brief Authenticate, decrypt, and load the stored Wi-Fi credentials.
 *
 * @param[out] credentials NUL-terminated credentials and their explicit lengths.
 * @return Detailed storage, authentication, or validation result.
 */
wifi_profile_result_t wifi_profile_load(wifi_profile_credentials_t *credentials);

/**
 * @brief Encrypt and atomically replace the stored Wi-Fi credentials.
 *
 * Open networks require an empty password. Protected networks require between
 * 8 and WIFI_PROFILE_MAX_PASSWORD password bytes.
 *
 * @param ssid Raw SSID bytes.
 * @param ssid_length Number of bytes in @p ssid.
 * @param password Raw password bytes.
 * @param password_length Number of bytes in @p password.
 * @param auth Pico SDK CYW43 authentication mode.
 * @return Detailed validation, encryption, or flash-storage result.
 */
wifi_profile_result_t wifi_profile_store(
    const uint8_t *ssid,
    size_t ssid_length,
    const uint8_t *password,
    size_t password_length,
    uint32_t auth
);

/**
 * @brief Erase the flash sector reserved for the Wi-Fi profile.
 *
 * @return WIFI_PROFILE_OK on success, otherwise WIFI_PROFILE_STORAGE_FAILED.
 */
wifi_profile_result_t wifi_profile_erase(void);

#endif

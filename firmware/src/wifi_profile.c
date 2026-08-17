#include "wifi_profile.h"

#include <stddef.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "mbedtls/gcm.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/sha256.h"
#include "pico/flash.h"
#include "pico/rand.h"
#include "pico/unique_id.h"

#define WIFI_PROFILE_FORMAT_VERSION 2u
#define WIFI_PROFILE_NONCE_SIZE 12u
#define WIFI_PROFILE_KEY_SIZE 32u
#define WIFI_PROFILE_TAG_SIZE 16u
#define WIFI_PROFILE_FLASH_OFFSET \
    (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define WIFI_PROFILE_FLASH_TIMEOUT_MS 1000u

typedef struct __attribute__((packed)) {
    uint8_t format_version;
    uint8_t ssid_length;
    uint8_t password_length;
    uint8_t reserved;
    uint32_t auth;
    uint8_t ssid[WIFI_PROFILE_MAX_SSID];
    uint8_t password[WIFI_PROFILE_MAX_PASSWORD + 1u];
} wifi_profile_plaintext_t;

typedef struct __attribute__((packed)) {
    uint8_t magic[8];
    uint8_t format_version;
    uint8_t reserved[3];
    uint8_t nonce[WIFI_PROFILE_NONCE_SIZE];
    uint8_t ciphertext[sizeof(wifi_profile_plaintext_t)];
    uint8_t tag[WIFI_PROFILE_TAG_SIZE];
} wifi_profile_record_t;

static const uint8_t wifi_profile_magic[8] = {
    'P', '2', 'W', 'P', 'P', 'R', 'F', '2',
};

static const uint8_t wifi_profile_key_context[] = {
    'P', '2', 'W', 'P', ' ', 'P', 'i', 'c', 'o', ' ', 'W', ' ',
    'p', 'r', 'o', 'f', 'i', 'l', 'e', ' ', 'k', 'e', 'y', ' ', 'v', '2',
};

static uint8_t wifi_profile_flash_page[FLASH_PAGE_SIZE];

_Static_assert(
    sizeof(wifi_profile_record_t) <= FLASH_PAGE_SIZE,
    "Wi-Fi profile record must fit in one flash page"
);
_Static_assert(
    WIFI_PROFILE_FLASH_OFFSET % FLASH_SECTOR_SIZE == 0u,
    "Wi-Fi profile flash offset must be sector aligned"
);

static const wifi_profile_record_t *wifi_profile_flash_record(void) {
    return (const wifi_profile_record_t *)(
        XIP_BASE + WIFI_PROFILE_FLASH_OFFSET
    );
}

static bool wifi_profile_flash_region_available(void) {
    extern char __flash_binary_end;
    return (uintptr_t)&__flash_binary_end - XIP_BASE <=
        WIFI_PROFILE_FLASH_OFFSET;
}

bool wifi_profile_present(void) {
    return wifi_profile_flash_region_available() &&
        memcmp(
            wifi_profile_flash_record()->magic,
            wifi_profile_magic,
            sizeof(wifi_profile_magic)
        ) == 0;
}

static void wifi_profile_random_bytes(uint8_t *destination, size_t length) {
    while (length != 0u) {
        const uint64_t random = get_rand_64();
        const size_t chunk = length < sizeof(random) ? length : sizeof(random);
        memcpy(destination, &random, chunk);
        destination += chunk;
        length -= chunk;
    }
}

// RP2040 has no protected secret storage. This device-bound key therefore
// prevents plaintext strings and casual cross-device copying, but it cannot
// defend against an attacker who can read and analyse the complete flash.
static int wifi_profile_derive_device_key(
    uint8_t key[WIFI_PROFILE_KEY_SIZE]
) {
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);
    mbedtls_sha256_context sha256;
    mbedtls_sha256_init(&sha256);
    int result = mbedtls_sha256_starts_ret(&sha256, 0);
    if (result == 0) {
        result = mbedtls_sha256_update_ret(
            &sha256,
            wifi_profile_key_context,
            sizeof(wifi_profile_key_context)
        );
    }
    if (result == 0) {
        result = mbedtls_sha256_update_ret(
            &sha256,
            board_id.id,
            sizeof(board_id.id)
        );
    }
    if (result == 0) {
        result = mbedtls_sha256_finish_ret(&sha256, key);
    }
    mbedtls_sha256_free(&sha256);
    mbedtls_platform_zeroize(&board_id, sizeof(board_id));
    return result;
}

static size_t wifi_profile_build_aad(
    const wifi_profile_record_t *record,
    uint8_t *aad
) {
    const size_t header_size = offsetof(wifi_profile_record_t, ciphertext);
    memcpy(aad, record, header_size);
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);
    memcpy(aad + header_size, board_id.id, sizeof(board_id.id));
    mbedtls_platform_zeroize(&board_id, sizeof(board_id));
    return header_size + PICO_UNIQUE_BOARD_ID_SIZE_BYTES;
}

typedef struct {
    bool program;
} wifi_profile_flash_operation_t;

static void wifi_profile_flash_update(void *argument) {
    const wifi_profile_flash_operation_t *operation = argument;
    flash_range_erase(WIFI_PROFILE_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    if (operation->program) {
        flash_range_program(
            WIFI_PROFILE_FLASH_OFFSET,
            wifi_profile_flash_page,
            sizeof(wifi_profile_flash_page)
        );
    }
}

static wifi_profile_result_t wifi_profile_commit(bool program) {
    if (!wifi_profile_flash_region_available()) {
        return WIFI_PROFILE_STORAGE_FAILED;
    }
    const wifi_profile_flash_operation_t operation = {.program = program};
    return flash_safe_execute(
        wifi_profile_flash_update,
        (void *)&operation,
        WIFI_PROFILE_FLASH_TIMEOUT_MS
    ) == PICO_OK
        ? WIFI_PROFILE_OK
        : WIFI_PROFILE_STORAGE_FAILED;
}

wifi_profile_result_t wifi_profile_store(
    const uint8_t *ssid,
    size_t ssid_length,
    const uint8_t *password,
    size_t password_length,
    uint32_t auth
) {
    if (ssid == NULL || password == NULL ||
        ssid_length == 0u || ssid_length > WIFI_PROFILE_MAX_SSID ||
        password_length > WIFI_PROFILE_MAX_PASSWORD ||
        (auth == 0u && password_length != 0u) ||
        (auth != 0u && password_length < 8u)) {
        return WIFI_PROFILE_INVALID_DATA;
    }

    wifi_profile_record_t record;
    wifi_profile_plaintext_t plaintext;
    uint8_t key[WIFI_PROFILE_KEY_SIZE];
    uint8_t aad[offsetof(wifi_profile_record_t, ciphertext) +
                PICO_UNIQUE_BOARD_ID_SIZE_BYTES];
    memset(&record, 0, sizeof(record));
    memset(&plaintext, 0, sizeof(plaintext));
    memcpy(record.magic, wifi_profile_magic, sizeof(record.magic));
    record.format_version = WIFI_PROFILE_FORMAT_VERSION;
    wifi_profile_random_bytes(record.nonce, sizeof(record.nonce));

    plaintext.format_version = WIFI_PROFILE_FORMAT_VERSION;
    plaintext.ssid_length = (uint8_t)ssid_length;
    plaintext.password_length = (uint8_t)password_length;
    plaintext.auth = auth;
    memcpy(plaintext.ssid, ssid, ssid_length);
    memcpy(plaintext.password, password, password_length);

    int result = wifi_profile_derive_device_key(key);
    const size_t aad_length = wifi_profile_build_aad(&record, aad);
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    if (result == 0) {
        result = mbedtls_gcm_setkey(
            &gcm,
            MBEDTLS_CIPHER_ID_AES,
            key,
            WIFI_PROFILE_KEY_SIZE * 8u
        );
    }
    if (result == 0) {
        result = mbedtls_gcm_crypt_and_tag(
            &gcm,
            MBEDTLS_GCM_ENCRYPT,
            sizeof(plaintext),
            record.nonce,
            sizeof(record.nonce),
            aad,
            aad_length,
            (const uint8_t *)&plaintext,
            record.ciphertext,
            sizeof(record.tag),
            record.tag
        );
    }
    mbedtls_gcm_free(&gcm);
    mbedtls_platform_zeroize(key, sizeof(key));
    mbedtls_platform_zeroize(aad, sizeof(aad));
    mbedtls_platform_zeroize(&plaintext, sizeof(plaintext));
    if (result != 0) {
        mbedtls_platform_zeroize(&record, sizeof(record));
        return WIFI_PROFILE_STORAGE_FAILED;
    }

    memset(wifi_profile_flash_page, 0xff, sizeof(wifi_profile_flash_page));
    memcpy(wifi_profile_flash_page, &record, sizeof(record));
    mbedtls_platform_zeroize(&record, sizeof(record));
    const wifi_profile_result_t commit_result = wifi_profile_commit(true);
    mbedtls_platform_zeroize(
        wifi_profile_flash_page,
        sizeof(wifi_profile_flash_page)
    );
    return commit_result;
}

wifi_profile_result_t wifi_profile_load(
    wifi_profile_credentials_t *credentials
) {
    if (credentials == NULL) {
        return WIFI_PROFILE_INVALID_DATA;
    }
    memset(credentials, 0, sizeof(*credentials));
    if (!wifi_profile_present()) {
        return WIFI_PROFILE_NOT_FOUND;
    }

    wifi_profile_record_t record;
    wifi_profile_plaintext_t plaintext;
    uint8_t key[WIFI_PROFILE_KEY_SIZE];
    uint8_t aad[offsetof(wifi_profile_record_t, ciphertext) +
                PICO_UNIQUE_BOARD_ID_SIZE_BYTES];
    memcpy(&record, wifi_profile_flash_record(), sizeof(record));
    if (record.format_version != WIFI_PROFILE_FORMAT_VERSION) {
        mbedtls_platform_zeroize(&record, sizeof(record));
        return WIFI_PROFILE_CORRUPT;
    }

    int result = wifi_profile_derive_device_key(key);
    const size_t aad_length = wifi_profile_build_aad(&record, aad);
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    if (result == 0) {
        result = mbedtls_gcm_setkey(
            &gcm,
            MBEDTLS_CIPHER_ID_AES,
            key,
            WIFI_PROFILE_KEY_SIZE * 8u
        );
    }
    if (result == 0) {
        result = mbedtls_gcm_auth_decrypt(
            &gcm,
            sizeof(plaintext),
            record.nonce,
            sizeof(record.nonce),
            aad,
            aad_length,
            record.tag,
            sizeof(record.tag),
            record.ciphertext,
            (uint8_t *)&plaintext
        );
    }
    mbedtls_gcm_free(&gcm);
    mbedtls_platform_zeroize(key, sizeof(key));
    mbedtls_platform_zeroize(aad, sizeof(aad));
    mbedtls_platform_zeroize(&record, sizeof(record));
    if (result != 0) {
        mbedtls_platform_zeroize(&plaintext, sizeof(plaintext));
        return WIFI_PROFILE_CORRUPT;
    }
    if (plaintext.format_version != WIFI_PROFILE_FORMAT_VERSION ||
        plaintext.ssid_length == 0u ||
        plaintext.ssid_length > WIFI_PROFILE_MAX_SSID ||
        plaintext.password_length > WIFI_PROFILE_MAX_PASSWORD ||
        (plaintext.auth == 0u && plaintext.password_length != 0u) ||
        (plaintext.auth != 0u && plaintext.password_length < 8u)) {
        mbedtls_platform_zeroize(&plaintext, sizeof(plaintext));
        return WIFI_PROFILE_CORRUPT;
    }

    credentials->ssid_length = plaintext.ssid_length;
    memcpy(credentials->ssid, plaintext.ssid, plaintext.ssid_length);
    credentials->ssid[plaintext.ssid_length] = '\0';
    credentials->password_length = plaintext.password_length;
    memcpy(
        credentials->password,
        plaintext.password,
        plaintext.password_length
    );
    credentials->password[plaintext.password_length] = '\0';
    credentials->auth = plaintext.auth;
    mbedtls_platform_zeroize(&plaintext, sizeof(plaintext));
    return WIFI_PROFILE_OK;
}

wifi_profile_result_t wifi_profile_erase(void) {
    return wifi_profile_commit(false);
}

#include "custom_url_store.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "pico/flash.h"

#define CUSTOM_URL_FORMAT_VERSION 1u
#define CUSTOM_URL_FLASH_OFFSET \
    (PICO_FLASH_SIZE_BYTES - (2u * FLASH_SECTOR_SIZE))
#define CUSTOM_URL_FLASH_TIMEOUT_MS 1000u

typedef struct __attribute__((packed)) {
    uint8_t magic[8];
    uint8_t format_version;
    uint8_t url_length;
    uint8_t reserved[2];
    uint32_t checksum;
    char url[CUSTOM_ENDPOINT_URL_MAX];
} custom_url_record_t;

static const uint8_t custom_url_magic[8] = {
    'P', '2', 'W', 'P', 'U', 'R', 'L', '1',
};

static uint8_t custom_url_flash_page[FLASH_PAGE_SIZE];

_Static_assert(
    sizeof(custom_url_record_t) <= FLASH_PAGE_SIZE,
    "Custom URL record must fit in one flash page"
);
_Static_assert(
    CUSTOM_URL_FLASH_OFFSET % FLASH_SECTOR_SIZE == 0u,
    "Custom URL flash offset must be sector aligned"
);

static const custom_url_record_t *custom_url_flash_record(void) {
    return (const custom_url_record_t *)(XIP_BASE + CUSTOM_URL_FLASH_OFFSET);
}

static bool custom_url_flash_region_available(void) {
    extern char __flash_binary_end;
    return (uintptr_t)&__flash_binary_end - XIP_BASE <=
        CUSTOM_URL_FLASH_OFFSET;
}

static uint32_t custom_url_checksum(const custom_url_record_t *record) {
    const uint8_t *data = &record->format_version;
    const size_t length = offsetof(custom_url_record_t, checksum) -
        offsetof(custom_url_record_t, format_version);
    uint32_t checksum = 2166136261u;
    for (size_t index = 0u; index < length; ++index) {
        checksum = (checksum ^ data[index]) * 16777619u;
    }
    for (size_t index = 0u; index < record->url_length; ++index) {
        checksum = (checksum ^ (uint8_t)record->url[index]) * 16777619u;
    }
    return checksum;
}

static custom_url_store_result_t custom_url_read_record(
    custom_url_record_t *record
) {
    if (!custom_url_flash_region_available()) {
        return CUSTOM_URL_STORE_FAILED;
    }
    memcpy(record, custom_url_flash_record(), sizeof(*record));
    if (memcmp(record->magic, custom_url_magic, sizeof(custom_url_magic)) != 0) {
        return CUSTOM_URL_STORE_NOT_FOUND;
    }
    if (record->format_version != CUSTOM_URL_FORMAT_VERSION ||
        record->url_length == 0u ||
        record->url_length > CUSTOM_ENDPOINT_URL_MAX ||
        record->checksum != custom_url_checksum(record)) {
        return CUSTOM_URL_STORE_CORRUPT;
    }
    custom_endpoint_t endpoint;
    if (!custom_endpoint_parse(record->url, record->url_length, &endpoint)) {
        return CUSTOM_URL_STORE_CORRUPT;
    }
    return CUSTOM_URL_STORE_OK;
}

custom_url_store_result_t custom_url_store_load(
    char url[CUSTOM_ENDPOINT_URL_MAX + 1u],
    uint8_t *url_length
) {
    if (url == NULL || url_length == NULL) {
        return CUSTOM_URL_STORE_INVALID_DATA;
    }
    *url_length = 0u;
    url[0] = '\0';
    custom_url_record_t record;
    const custom_url_store_result_t result = custom_url_read_record(&record);
    if (result != CUSTOM_URL_STORE_OK) {
        return result;
    }
    memcpy(url, record.url, record.url_length);
    url[record.url_length] = '\0';
    *url_length = record.url_length;
    return CUSTOM_URL_STORE_OK;
}

typedef struct {
    bool program;
} custom_url_flash_operation_t;

static void custom_url_flash_update(void *argument) {
    const custom_url_flash_operation_t *operation = argument;
    flash_range_erase(CUSTOM_URL_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    if (operation->program) {
        flash_range_program(
            CUSTOM_URL_FLASH_OFFSET,
            custom_url_flash_page,
            sizeof(custom_url_flash_page)
        );
    }
}

custom_url_store_result_t custom_url_store_save(
    const char *url,
    size_t url_length
) {
    custom_endpoint_t endpoint;
    if (url == NULL || url_length == 0u ||
        url_length > CUSTOM_ENDPOINT_URL_MAX ||
        !custom_endpoint_parse(url, url_length, &endpoint)) {
        return CUSTOM_URL_STORE_INVALID_DATA;
    }

    custom_url_record_t existing;
    const custom_url_store_result_t existing_result =
        custom_url_read_record(&existing);
    if (existing_result == CUSTOM_URL_STORE_OK &&
        existing.url_length == url_length &&
        memcmp(existing.url, url, url_length) == 0) {
        return CUSTOM_URL_STORE_UNCHANGED;
    }
    if (!custom_url_flash_region_available()) {
        return CUSTOM_URL_STORE_FAILED;
    }

    custom_url_record_t record;
    memset(&record, 0, sizeof(record));
    memcpy(record.magic, custom_url_magic, sizeof(record.magic));
    record.format_version = CUSTOM_URL_FORMAT_VERSION;
    record.url_length = (uint8_t)url_length;
    memcpy(record.url, url, url_length);
    record.checksum = custom_url_checksum(&record);
    memset(custom_url_flash_page, 0xff, sizeof(custom_url_flash_page));
    memcpy(custom_url_flash_page, &record, sizeof(record));

    const custom_url_flash_operation_t operation = {.program = true};
    const int result = flash_safe_execute(
        custom_url_flash_update,
        (void *)&operation,
        CUSTOM_URL_FLASH_TIMEOUT_MS
    );
    memset(custom_url_flash_page, 0, sizeof(custom_url_flash_page));
    return result == PICO_OK
        ? CUSTOM_URL_STORE_OK
        : CUSTOM_URL_STORE_FAILED;
}

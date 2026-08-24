#ifndef P2WP_TELETEKST_H
#define P2WP_TELETEKST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TELETEKST_COLUMNS 40u
#define TELETEKST_SOURCE_ROWS 25u
#define TELETEKST_DISPLAY_ROWS 24u
#define TELETEKST_HTTP_BODY_MAX 16384u
#define TELETEKST_SCREEN_SIZE \
    (TELETEKST_COLUMNS * TELETEKST_DISPLAY_ROWS)

typedef enum {
    TELETEKST_DECODE_OK = 0,
    TELETEKST_DECODE_INVALID_ARGUMENT,
    TELETEKST_DECODE_INVALID_NEXT_SUBPAGE,
    TELETEKST_DECODE_INVALID_BINARY_DISPLAY,
    TELETEKST_DECODE_INVALID_CONTENT,
    TELETEKST_DECODE_UNREPRESENTABLE_ROW,
} teletekst_decode_result_t;

// Convert a Teletekst JSON response into bytes that can be copied directly to
// the P2000T's SAA5050-backed video RAM. Prefer the custom server's exact
// 960-byte base64 binaryDisplay when present; otherwise compile NOS content.
// The 25th NOS row is the Fastext prompt row and is intentionally omitted.
bool teletekst_decode_nos_json(
    const char *json,
    size_t json_length,
    uint16_t requested_page,
    uint8_t screen[TELETEKST_SCREEN_SIZE],
    uint8_t *next_subpage
);

// Detailed host-side diagnostics for replay and regression tools. failed_row
// is one-based for TELETEKST_DECODE_UNREPRESENTABLE_ROW and zero otherwise.
teletekst_decode_result_t teletekst_decode_nos_json_diagnostic(
    const char *json,
    size_t json_length,
    uint16_t requested_page,
    uint8_t screen[TELETEKST_SCREEN_SIZE],
    uint8_t *next_subpage,
    uint8_t *failed_row
);

#endif

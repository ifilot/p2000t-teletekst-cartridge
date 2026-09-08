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

typedef struct {
    uint8_t next_subpage;
    uint16_t previous_page;
    uint16_t next_page;
} teletekst_metadata_t;

/**
 * @brief Decode a compatible JSON response and its navigation metadata.
 *
 * `prevPage` and `nextPage` are optional. Missing or empty values become zero.
 * The legacy `nextSubPage` field remains required for API compatibility.
 */
bool teletekst_decode_json(
    const char *json,
    size_t json_length,
    uint16_t requested_page,
    uint8_t screen[TELETEKST_SCREEN_SIZE],
    teletekst_metadata_t *metadata
);

/**
 * @brief Convert a Teletekst JSON response into SAA5050 display bytes.
 *
 * A valid 960-byte base64 `binaryDisplay` field is preferred. Otherwise the
 * NOS HTML-like `content` field is parsed and compiled. Source row 25 contains
 * Fastext prompts and is intentionally omitted from the 24-row display.
 *
 * @param json JSON response bytes; no terminating NUL is required.
 * @param json_length Number of response bytes in @p json.
 * @param requested_page Page number used to validate subpage metadata.
 * @param[out] screen Destination for exactly TELETEKST_SCREEN_SIZE bytes.
 * @param[out] next_subpage Next subpage number, or zero when none is advertised.
 * @return true when the response is valid and a screen was produced.
 */
bool teletekst_decode_nos_json(
    const char *json,
    size_t json_length,
    uint16_t requested_page,
    uint8_t screen[TELETEKST_SCREEN_SIZE],
    uint8_t *next_subpage
);

/**
 * @brief Decode a response and report the stage responsible for a failure.
 *
 * This is the diagnostic form used by host replay tools. It executes the same
 * decoding path as teletekst_decode_nos_json().
 *
 * @param json JSON response bytes; no terminating NUL is required.
 * @param json_length Number of response bytes in @p json.
 * @param requested_page Page number used to validate subpage metadata.
 * @param[out] screen Destination for exactly TELETEKST_SCREEN_SIZE bytes.
 * @param[out] next_subpage Next subpage number when decoding succeeds.
 * @param[out] failed_row One-based unrepresentable row, or zero for other results.
 * @return Detailed decoder result.
 */
teletekst_decode_result_t teletekst_decode_nos_json_diagnostic(
    const char *json,
    size_t json_length,
    uint16_t requested_page,
    uint8_t screen[TELETEKST_SCREEN_SIZE],
    uint8_t *next_subpage,
    uint8_t *failed_row
);

#endif

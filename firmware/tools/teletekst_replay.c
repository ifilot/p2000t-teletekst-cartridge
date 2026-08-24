#include "teletekst.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *result_name(teletekst_decode_result_t result) {
    switch (result) {
        case TELETEKST_DECODE_OK: return "ok";
        case TELETEKST_DECODE_INVALID_ARGUMENT: return "invalid argument";
        case TELETEKST_DECODE_INVALID_NEXT_SUBPAGE:
            return "invalid nextSubPage";
        case TELETEKST_DECODE_INVALID_BINARY_DISPLAY:
            return "invalid binaryDisplay";
        case TELETEKST_DECODE_INVALID_CONTENT: return "invalid content";
        case TELETEKST_DECODE_UNREPRESENTABLE_ROW:
            return "row cannot be represented by SAA5050 controls";
    }
    return "unknown";
}

static void usage(const char *program) {
    fprintf(
        stderr,
        "usage: %s PAGE [JSON_FILE|-] [SCREEN_FILE]\n",
        program
    );
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        usage(argv[0]);
        return 2;
    }
    char *page_end = NULL;
    errno = 0;
    const unsigned long page = strtoul(argv[1], &page_end, 10);
    if (errno != 0 || page_end == argv[1] || *page_end != '\0' ||
        page < 100u || page > 899u) {
        usage(argv[0]);
        return 2;
    }

    FILE *input = stdin;
    if (argc >= 3 && strcmp(argv[2], "-") != 0) {
        input = fopen(argv[2], "rb");
        if (input == NULL) {
            perror(argv[2]);
            return 2;
        }
    }
    char json[TELETEKST_HTTP_BODY_MAX + 1u];
    const size_t json_length = fread(json, 1u, sizeof(json), input);
    if (ferror(input)) {
        perror("read JSON");
        if (input != stdin) {
            fclose(input);
        }
        return 2;
    }
    if (input != stdin && fclose(input) != 0) {
        perror("close JSON");
        return 2;
    }
    if (json_length > TELETEKST_HTTP_BODY_MAX) {
        fprintf(
            stderr,
            "page %lu: Pico error 06 (response exceeds %u bytes)\n",
            page,
            TELETEKST_HTTP_BODY_MAX
        );
        return 1;
    }

    uint8_t screen[TELETEKST_SCREEN_SIZE];
    uint8_t next_subpage = 0u;
    uint8_t failed_row = 0u;
    const teletekst_decode_result_t result =
        teletekst_decode_nos_json_diagnostic(
            json,
            json_length,
            (uint16_t)page,
            screen,
            &next_subpage,
            &failed_row
        );
    if (result != TELETEKST_DECODE_OK) {
        fprintf(stderr, "page %lu: Pico error 07 (%s", page, result_name(result));
        if (failed_row != 0u) {
            fprintf(stderr, ", row %u", failed_row);
        }
        fputs(")\n", stderr);
        return 1;
    }

    if (argc == 4) {
        FILE *output = fopen(argv[3], "wb");
        if (output == NULL) {
            perror(argv[3]);
            return 2;
        }
        const bool written =
            fwrite(screen, 1u, sizeof(screen), output) == sizeof(screen);
        if (!written || fclose(output) != 0) {
            perror("write screen");
            return 2;
        }
    }
    printf(
        "page %lu: ok (%zu JSON bytes, next subpage %u, %u screen bytes)\n",
        page,
        json_length,
        next_subpage,
        TELETEKST_SCREEN_SIZE
    );
    return 0;
}

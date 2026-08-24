#include "teletekst.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum teletext_colour {
    TT_BLACK = 0,
    TT_RED = 1,
    TT_GREEN = 2,
    TT_YELLOW = 3,
    TT_BLUE = 4,
    TT_MAGENTA = 5,
    TT_CYAN = 6,
    TT_WHITE = 7,
};

enum teletext_mode {
    TT_ALPHA = 0,
    TT_GRAPHICS = 1,
};

typedef struct {
    uint8_t glyph;
    uint8_t foreground;
    uint8_t background;
    uint8_t mode;
} visual_cell_t;

typedef struct {
    uint8_t foreground;
    uint8_t background;
    uint8_t mode;
} display_state_t;

typedef struct {
    const char *position;
    const char *end;
} json_string_reader_t;

typedef enum {
    BINARY_DISPLAY_ABSENT,
    BINARY_DISPLAY_VALID,
    BINARY_DISPLAY_INVALID,
} binary_display_result_t;

typedef struct {
    uint8_t previous_state;
    uint8_t output;
    bool valid;
} predecessor_t;

#define TT_STATE_COUNT (7u * 8u * 2u)
#define TT_INFINITE_COST UINT16_MAX
#define TT_STYLE_STACK_DEPTH 8u
#define TT_TAG_MAX 127u
#define TT_ENTITY_MAX 31u

static visual_cell_t
    decode_cells[TELETEKST_SOURCE_ROWS][TELETEKST_COLUMNS];
static predecessor_t
    compile_history[TELETEKST_COLUMNS][TT_STATE_COUNT];

/**
 * @brief Pack a valid SAA5050 display state into a dynamic-programming index.
 *
 * @param state Foreground, background, and alpha/graphics mode.
 * @return Index in the range zero through TT_STATE_COUNT minus one.
 */
static uint8_t state_index(display_state_t state) {
    return (uint8_t)((((state.mode * 8u) + state.background) * 7u) +
                     state.foreground - 1u);
}

/**
 * @brief Expand a dynamic-programming index into an SAA5050 display state.
 *
 * @param index Packed state index produced by state_index().
 * @return Decoded foreground, background, and mode.
 */
static display_state_t state_from_index(uint8_t index) {
    display_state_t state;
    state.foreground = (uint8_t)(index % 7u) + 1u;
    index /= 7u;
    state.background = (uint8_t)(index % 8u);
    state.mode = (uint8_t)(index / 8u);
    return state;
}

/**
 * @brief Locate the opening contents of a top-level JSON string value.
 *
 * @param json First response byte to search.
 * @param end One-past-the-last response byte.
 * @param key Exact JSON property name.
 * @return Pointer immediately after the value's opening quote, or NULL.
 */
static const char *find_json_string(
    const char *json,
    const char *end,
    const char *key
) {
    const size_t key_length = strlen(key);
    for (const char *cursor = json; cursor + key_length + 2u < end; ++cursor) {
        if (*cursor != '"' ||
            (size_t)(end - cursor) < key_length + 2u ||
            memcmp(cursor + 1u, key, key_length) != 0 ||
            cursor[key_length + 1u] != '"') {
            continue;
        }
        cursor += key_length + 2u;
        while (cursor < end &&
               (*cursor == ' ' || *cursor == '\t' ||
                *cursor == '\r' || *cursor == '\n')) {
            ++cursor;
        }
        if (cursor >= end || *cursor++ != ':') {
            return NULL;
        }
        while (cursor < end &&
               (*cursor == ' ' || *cursor == '\t' ||
                *cursor == '\r' || *cursor == '\n')) {
            ++cursor;
        }
        return cursor < end && *cursor == '"' ? cursor + 1u : NULL;
    }
    return NULL;
}

/**
 * @brief Decode the next code point from a bounded JSON string.
 *
 * @param[in,out] reader Current input position and immutable end pointer.
 * @return Decoded code point, -1 at the closing quote, or -2 for malformed data.
 */
static int32_t json_string_next(json_string_reader_t *reader) {
    if (reader->position >= reader->end) {
        return -2;
    }
    uint8_t value = (uint8_t)*reader->position++;
    if (value == '"') {
        return -1;
    }
    if (value != '\\') {
        return value;
    }
    if (reader->position >= reader->end) {
        return -2;
    }
    value = (uint8_t)*reader->position++;
    switch (value) {
        case '"':
        case '\\':
        case '/':
            return value;
        case 'b':
            return '\b';
        case 'f':
            return '\f';
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        case 'u': {
            uint32_t codepoint = 0u;
            for (unsigned digit = 0; digit < 4u; ++digit) {
                if (reader->position >= reader->end) {
                    return -2;
                }
                const uint8_t hex = (uint8_t)*reader->position++;
                codepoint <<= 4u;
                if (hex >= '0' && hex <= '9') {
                    codepoint |= hex - '0';
                } else if (hex >= 'a' && hex <= 'f') {
                    codepoint |= hex - 'a' + 10u;
                } else if (hex >= 'A' && hex <= 'F') {
                    codepoint |= hex - 'A' + 10u;
                } else {
                    return -2;
                }
            }
            return (int32_t)codepoint;
        }
        default:
            return -2;
    }
}

/**
 * @brief Convert one base64 alphabet character to its six-bit value.
 *
 * @param character Character code to decode.
 * @return Value from zero through 63, or -1 for a non-base64 character.
 */
static int base64_value(int32_t character) {
    if (character >= 'A' && character <= 'Z') {
        return character - 'A';
    }
    if (character >= 'a' && character <= 'z') {
        return character - 'a' + 26;
    }
    if (character >= '0' && character <= '9') {
        return character - '0' + 52;
    }
    if (character == '+') {
        return 62;
    }
    if (character == '/') {
        return 63;
    }
    return -1;
}

/**
 * @brief Parse an optional exact SAA5050 screen image from the JSON response.
 *
 * @param json First response byte.
 * @param end One-past-the-last response byte.
 * @param[out] screen Destination for the decoded 960-byte image.
 * @return Whether the field is absent, valid, or present but invalid.
 */
static binary_display_result_t parse_binary_display(
    const char *json,
    const char *end,
    uint8_t screen[TELETEKST_SCREEN_SIZE]
) {
    const char *value = find_json_string(json, end, "binaryDisplay");
    if (value == NULL) {
        return BINARY_DISPLAY_ABSENT;
    }

    json_string_reader_t reader = {.position = value, .end = end};
    uint32_t group = 0u;
    size_t sextets = 0u;
    size_t output = 0u;
    while (true) {
        const int32_t character = json_string_next(&reader);
        if (character == -1) {
            break;
        }
        const int decoded = base64_value(character);
        if (character < 0 || decoded < 0) {
            return BINARY_DISPLAY_INVALID;
        }
        group = (group << 6u) | (uint32_t)decoded;
        if (++sextets == 4u) {
            if (output > TELETEKST_SCREEN_SIZE - 3u) {
                return BINARY_DISPLAY_INVALID;
            }
            screen[output++] = (uint8_t)(group >> 16u);
            screen[output++] = (uint8_t)(group >> 8u);
            screen[output++] = (uint8_t)group;
            group = 0u;
            sextets = 0u;
        }
    }
    return sextets == 0u && output == TELETEKST_SCREEN_SIZE
        ? BINARY_DISPLAY_VALID
        : BINARY_DISPLAY_INVALID;
}

/**
 * @brief Parse and validate the API's `nextSubPage` field.
 *
 * @param json First response byte.
 * @param end One-past-the-last response byte.
 * @param requested_page Page that must own any advertised subpage.
 * @param[out] next_subpage Parsed subpage, or zero when rotation should stop.
 * @return true when the field is present and valid.
 */
static bool parse_next_subpage(
    const char *json,
    const char *end,
    uint16_t requested_page,
    uint8_t *next_subpage
) {
    const char *value = find_json_string(json, end, "nextSubPage");
    if (value == NULL) {
        return false;
    }
    json_string_reader_t reader = {.position = value, .end = end};
    unsigned page = 0u;
    unsigned subpage = 0u;
    bool separator = false;
    bool have_page = false;
    bool have_subpage = false;
    while (true) {
        const int32_t character = json_string_next(&reader);
        if (character == -1) {
            break;
        }
        if (character < 0) {
            return false;
        }
        if (character == '-') {
            if (separator || !have_page) {
                return false;
            }
            separator = true;
            continue;
        }
        if (character < '0' || character > '9') {
            return false;
        }
        if (!separator) {
            have_page = true;
            page = page * 10u + (unsigned)(character - '0');
        } else {
            have_subpage = true;
            subpage = subpage * 10u + (unsigned)(character - '0');
        }
        if (page > 999u || subpage > UINT8_MAX) {
            return false;
        }
    }
    if (!have_page) {
        *next_subpage = 0u;
        return !separator;
    }
    if (!separator) {
        *next_subpage = 0u;
        return page == 0u || page == requested_page;
    }
    if (!have_subpage || page != requested_page || subpage == 0u) {
        return false;
    }
    *next_subpage = (uint8_t)subpage;
    return true;
}

/**
 * @brief Translate a NOS CSS colour name into an SAA5050 colour index.
 *
 * @param name First character of the class token.
 * @param length Number of characters in the token.
 * @return Colour index, or -1 when the token is not a supported colour.
 */
static int colour_from_class(const char *name, size_t length) {
    static const struct {
        const char *name;
        uint8_t colour;
    } colours[] = {
        {"black", TT_BLACK},   {"red", TT_RED},
        {"green", TT_GREEN},
        {"yellow", TT_YELLOW}, {"blue", TT_BLUE},
        {"magenta", TT_MAGENTA}, {"cyan", TT_CYAN},
        {"white", TT_WHITE},
    };
    for (size_t index = 0u; index < sizeof(colours) / sizeof(colours[0]); ++index) {
        if (strlen(colours[index].name) == length &&
            memcmp(colours[index].name, name, length) == 0) {
            return colours[index].colour;
        }
    }
    return -1;
}

/**
 * @brief Apply foreground and background CSS classes to a visual style.
 *
 * Unknown classes are intentionally ignored because the NOS response can add
 * non-visual classes without changing the teletext representation.
 *
 * @param classes Space-separated class-list bytes.
 * @param length Number of bytes in @p classes.
 * @param[in,out] style Style to update.
 */
static void apply_classes(
    const char *classes,
    size_t length,
    display_state_t *style
) {
    size_t offset = 0u;
    while (offset < length) {
        while (offset < length && classes[offset] == ' ') {
            ++offset;
        }
        const size_t start = offset;
        while (offset < length && classes[offset] != ' ') {
            ++offset;
        }
        if (offset == start) {
            continue;
        }
        const char *name = classes + start;
        size_t name_length = offset - start;
        bool background = false;
        if (name_length > 3u && memcmp(name, "bg-", 3u) == 0) {
            background = true;
            name += 3u;
            name_length -= 3u;
        }
        const int colour = colour_from_class(name, name_length);
        if (colour >= 0) {
            if (background) {
                style->background = (uint8_t)colour;
            } else {
                style->foreground = (uint8_t)colour;
            }
        }
    }
}

/**
 * @brief Apply one HTML-like opening or closing tag to the style stack.
 *
 * Only `span` and `a` affect style; other tags are ignored after validation.
 *
 * @param tag Tag contents without angle brackets.
 * @param length Number of bytes in @p tag.
 * @param[in,out] style Current visual style.
 * @param[in,out] stack Saved styles for nested elements.
 * @param[in,out] depth Current stack depth.
 * @return false for malformed nesting, invalid classes, or stack overflow.
 */
static bool parse_tag(
    const char *tag,
    size_t length,
    display_state_t *style,
    display_state_t stack[TT_STYLE_STACK_DEPTH],
    size_t *depth
) {
    size_t offset = 0u;
    while (offset < length && tag[offset] == ' ') {
        ++offset;
    }
    const bool closing = offset < length && tag[offset] == '/';
    if (closing) {
        ++offset;
    }
    const size_t name_start = offset;
    while (offset < length &&
           tag[offset] != ' ' && tag[offset] != '/' && tag[offset] != '>') {
        ++offset;
    }
    const size_t name_length = offset - name_start;
    const bool styled =
        (name_length == 4u && memcmp(tag + name_start, "span", 4u) == 0) ||
        (name_length == 1u && tag[name_start] == 'a');
    if (!styled) {
        return true;
    }
    if (closing) {
        if (*depth == 0u) {
            return false;
        }
        *style = stack[--*depth];
        return true;
    }
    if (*depth >= TT_STYLE_STACK_DEPTH) {
        return false;
    }
    stack[(*depth)++] = *style;
    for (size_t cursor = offset; cursor + 7u < length; ++cursor) {
        if (memcmp(tag + cursor, "class=\"", 7u) != 0) {
            continue;
        }
        cursor += 7u;
        const size_t class_start = cursor;
        while (cursor < length && tag[cursor] != '"') {
            ++cursor;
        }
        if (cursor >= length) {
            return false;
        }
        apply_classes(tag + class_start, cursor - class_start, style);
        break;
    }
    return true;
}

/**
 * @brief Decode a numeric or supported named HTML entity.
 *
 * @param entity Entity contents without the ampersand and semicolon.
 * @param length Number of bytes in @p entity.
 * @return Unicode code point, or UINT32_MAX when unsupported or malformed.
 */
static uint32_t entity_codepoint(const char *entity, size_t length) {
    if (length >= 2u && entity[0] == '#') {
        size_t offset = 1u;
        unsigned base = 10u;
        if (offset < length && (entity[offset] == 'x' || entity[offset] == 'X')) {
            base = 16u;
            ++offset;
        }
        if (offset == length) {
            return UINT32_MAX;
        }
        uint32_t value = 0u;
        for (; offset < length; ++offset) {
            unsigned digit;
            const uint8_t character = (uint8_t)entity[offset];
            if (character >= '0' && character <= '9') {
                digit = character - '0';
            } else if (base == 16u && character >= 'a' && character <= 'f') {
                digit = character - 'a' + 10u;
            } else if (base == 16u && character >= 'A' && character <= 'F') {
                digit = character - 'A' + 10u;
            } else {
                return UINT32_MAX;
            }
            if (digit >= base || value > (UINT32_MAX - digit) / base) {
                return UINT32_MAX;
            }
            value = value * base + digit;
        }
        return value;
    }
    static const struct {
        const char *name;
        uint32_t codepoint;
    } named[] = {
        {"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'},
        {"apos", '\''}, {"nbsp", ' '}, {"pound", 0x00a3u},
        {"aacute", 0x00e1u}, {"Aacute", 0x00c1u},
        {"agrave", 0x00e0u}, {"Agrave", 0x00c0u},
        {"auml", 0x00e4u}, {"Auml", 0x00c4u},
        {"eacute", 0x00e9u}, {"Eacute", 0x00c9u},
        {"egrave", 0x00e8u}, {"Egrave", 0x00c8u},
        {"euml", 0x00ebu}, {"Euml", 0x00cbu},
        {"iuml", 0x00efu}, {"Iuml", 0x00cfu},
        {"ouml", 0x00f6u}, {"Ouml", 0x00d6u},
        {"uuml", 0x00fcu}, {"Uuml", 0x00dcu},
        {"ccedil", 0x00e7u}, {"Ccedil", 0x00c7u},
    };
    for (size_t index = 0u; index < sizeof(named) / sizeof(named[0]); ++index) {
        if (strlen(named[index].name) == length &&
            memcmp(named[index].name, entity, length) == 0) {
            return named[index].codepoint;
        }
    }
    return UINT32_MAX;
}

/**
 * @brief Map a Unicode code point to the closest P2000T Viewdata glyph.
 *
 * @param codepoint Unicode character to convert.
 * @return Seven-bit display glyph, using `?` when no closer glyph exists.
 */
static uint8_t transliterate(uint32_t codepoint) {
    // The P2000T Viewdata set is not ASCII in these positions. In particular,
    // 23h is pound and the actual hash glyph is 5Fh. Preserve exact P2000T
    // glyphs where available and use visually close fallbacks otherwise.
    switch (codepoint) {
        case '#': return 0x5fu;
        case '|': return 0x7cu;
        case '[': return '(';
        case '\\': return '/';
        case ']': return ')';
        case '^': return 0x5eu;  // upward arrow
        case '_': return 0x60u;  // horizontal bar
        case '`': return '\'';
        case '{': return '(';
        case '}': return ')';
        case '~': return 0x60u;
        case 0x7fu: return '?';
        case 0x00a3u: return 0x23u;
        case 0x00bcu: return 0x7bu;
        case 0x00bdu: return 0x5cu;
        case 0x00beu: return 0x7du;
        case 0x00f7u: return 0x7eu;
        case 0x2014u: return 0x60u;
        case 0x2190u: return 0x5bu;
        case 0x2191u: return 0x5eu;
        case 0x2192u: return 0x5du;
        case 0x2588u: case 0x25a0u: return 0x7fu;
        default: break;
    }
    if (codepoint >= 0x20u && codepoint <= 0x7au) {
        return (uint8_t)codepoint;
    }
    switch (codepoint) {
        case 0x00c0u: case 0x00c1u: case 0x00c2u: case 0x00c3u: case 0x00c4u:
        case 0x00c5u:
            return 'A';
        case 0x00e0u: case 0x00e1u: case 0x00e2u: case 0x00e3u: case 0x00e4u:
        case 0x00e5u:
            return 'a';
        case 0x00c7u:
            return 'C';
        case 0x00e7u:
            return 'c';
        case 0x00c8u: case 0x00c9u: case 0x00cau: case 0x00cbu:
            return 'E';
        case 0x00e8u: case 0x00e9u: case 0x00eau: case 0x00ebu:
            return 'e';
        case 0x00ccu: case 0x00cdu: case 0x00ceu: case 0x00cfu:
            return 'I';
        case 0x00ecu: case 0x00edu: case 0x00eeu: case 0x00efu:
            return 'i';
        case 0x00d1u:
            return 'N';
        case 0x00f1u:
            return 'n';
        case 0x00d2u: case 0x00d3u: case 0x00d4u: case 0x00d5u: case 0x00d6u:
            return 'O';
        case 0x00f2u: case 0x00f3u: case 0x00f4u: case 0x00f5u: case 0x00f6u:
            return 'o';
        case 0x00d9u: case 0x00dau: case 0x00dbu: case 0x00dcu:
            return 'U';
        case 0x00f9u: case 0x00fau: case 0x00fbu: case 0x00fcu:
            return 'u';
        default:
            return '?';
    }
}

/**
 * @brief Append one decoded visual cell while enforcing the 25-by-40 layout.
 *
 * @param[in,out] cells Complete source-page cell matrix.
 * @param[in,out] row Current zero-based row.
 * @param[in,out] column Current zero-based column.
 * @param style Visual style attached to the code point.
 * @param codepoint Character, mosaic code point, or line ending to append.
 * @return false when a row has the wrong width or the page exceeds its bounds.
 */
static bool append_cell(
    visual_cell_t cells[TELETEKST_SOURCE_ROWS][TELETEKST_COLUMNS],
    size_t *row,
    size_t *column,
    display_state_t style,
    uint32_t codepoint
) {
    if (codepoint == '\r') {
        return true;
    }
    if (codepoint == '\n') {
        if (*column != TELETEKST_COLUMNS || *row >= TELETEKST_SOURCE_ROWS) {
            return false;
        }
        ++*row;
        *column = 0u;
        return true;
    }
    if (*row >= TELETEKST_SOURCE_ROWS || *column >= TELETEKST_COLUMNS) {
        return false;
    }
    visual_cell_t *cell = &cells[*row][(*column)++];
    cell->foreground = style.foreground;
    cell->background = style.background;
    if (codepoint >= 0xf020u && codepoint <= 0xf07fu &&
        (codepoint & 0x20u) != 0u) {
        cell->glyph = (uint8_t)(codepoint - 0xf000u);
        cell->mode = TT_GRAPHICS;
    } else {
        cell->glyph = transliterate(codepoint);
        cell->mode = TT_ALPHA;
    }
    return true;
}

/**
 * @brief Parse the API's HTML-like `content` string into visual cells.
 *
 * @param json First response byte.
 * @param end One-past-the-last response byte.
 * @param[out] cells Parsed 25-row visual page.
 * @return true only for balanced markup with exactly 25 rows of 40 cells.
 */
static bool parse_content(
    const char *json,
    const char *end,
    visual_cell_t cells[TELETEKST_SOURCE_ROWS][TELETEKST_COLUMNS]
) {
    const char *content = find_json_string(json, end, "content");
    if (content == NULL) {
        return false;
    }
    json_string_reader_t reader = {.position = content, .end = end};
    display_state_t style = {
        .foreground = TT_WHITE,
        .background = TT_BLACK,
        .mode = TT_ALPHA,
    };
    display_state_t stack[TT_STYLE_STACK_DEPTH];
    size_t depth = 0u;
    size_t row = 0u;
    size_t column = 0u;
    while (true) {
        int32_t character = json_string_next(&reader);
        if (character == -1) {
            break;
        }
        if (character < 0) {
            return false;
        }
        if (character == '<') {
            char tag[TT_TAG_MAX + 1u];
            size_t tag_length = 0u;
            do {
                character = json_string_next(&reader);
                if (character < 0 || tag_length >= TT_TAG_MAX) {
                    return false;
                }
                if (character != '>') {
                    tag[tag_length++] = character <= 0x7f ? (char)character : '?';
                }
            } while (character != '>');
            tag[tag_length] = '\0';
            if (!parse_tag(tag, tag_length, &style, stack, &depth)) {
                return false;
            }
            continue;
        }
        uint32_t codepoint = (uint32_t)character;
        if (character == '&') {
            const json_string_reader_t after_ampersand = reader;
            char entity[TT_ENTITY_MAX + 1u];
            size_t entity_length = 0u;
            bool terminated = false;
            while (true) {
                character = json_string_next(&reader);
                if (character < 0 || entity_length >= TT_ENTITY_MAX) {
                    break;
                }
                if (character == ';') {
                    terminated = true;
                    break;
                }
                if (character > 0x7f) {
                    break;
                }
                entity[entity_length++] = (char)character;
            }
            entity[entity_length] = '\0';
            const uint32_t entity_value = terminated
                ? entity_codepoint(entity, entity_length)
                : UINT32_MAX;
            if (entity_value == UINT32_MAX) {
                // NOS content is HTML-like and occasionally contains a bare
                // ampersand. Browsers render it literally; do the same and
                // leave the following input untouched.
                reader = after_ampersand;
                codepoint = '&';
            } else {
                codepoint = entity_value;
            }
        }
        if (!append_cell(cells, &row, &column, style, codepoint)) {
            return false;
        }
    }
    return depth == 0u && row == TELETEKST_SOURCE_ROWS && column == 0u;
}

/**
 * @brief Record a cheaper transition in the SAA5050 row compiler.
 *
 * @param[in,out] next_cost Candidate costs for the next column.
 * @param[in,out] predecessors Backtracking records for the next column.
 * @param previous State index before emitting @p output.
 * @param next State index after emitting @p output.
 * @param output Display byte emitted for the current cell.
 * @param cost Total path cost through this transition.
 */
static void relax(
    uint16_t next_cost[TT_STATE_COUNT],
    predecessor_t predecessors[TT_STATE_COUNT],
    uint8_t previous,
    uint8_t next,
    uint8_t output,
    uint16_t cost
) {
    if (cost < next_cost[next]) {
        next_cost[next] = cost;
        predecessors[next].previous_state = previous;
        predecessors[next].output = output;
        predecessors[next].valid = true;
    }
}

/**
 * @brief Compile one visual row into stateful SAA5050 bytes.
 *
 * Dynamic programming chooses control-code positions with minimum visual loss:
 * blank cells are preferred, then graphics, and alphabetic glyphs last.
 *
 * @param cells Forty desired visual cells.
 * @param[out] output Forty SAA5050 display bytes.
 * @return true when a valid control-code sequence can represent the row.
 */
static bool compile_row(
    const visual_cell_t cells[TELETEKST_COLUMNS],
    uint8_t output[TELETEKST_COLUMNS]
) {
    uint16_t costs[TT_STATE_COUNT];
    uint16_t next_costs[TT_STATE_COUNT];
    for (size_t state = 0u; state < TT_STATE_COUNT; ++state) {
        costs[state] = TT_INFINITE_COST;
    }
    const display_state_t initial = {
        .foreground = TT_WHITE,
        .background = TT_BLACK,
        .mode = TT_ALPHA,
    };
    costs[state_index(initial)] = 0u;

    for (size_t column = 0u; column < TELETEKST_COLUMNS; ++column) {
        for (size_t state = 0u; state < TT_STATE_COUNT; ++state) {
            next_costs[state] = TT_INFINITE_COST;
            compile_history[column][state].valid = false;
        }
        const visual_cell_t *cell = &cells[column];
        const bool blank = cell->glyph == 0x20u;
        const uint16_t control_cost = blank
            ? 1u
            : (cell->mode == TT_GRAPHICS ? 100u : 200u);
        for (uint8_t index = 0u; index < TT_STATE_COUNT; ++index) {
            if (costs[index] == TT_INFINITE_COST) {
                continue;
            }
            const display_state_t state = state_from_index(index);
            if (blank && state.background == cell->background) {
                relax(
                    next_costs,
                    compile_history[column],
                    index,
                    index,
                    0x20u,
                    costs[index]
                );
            }
            if (!blank && state.mode == cell->mode &&
                state.foreground == cell->foreground &&
                state.background == cell->background) {
                relax(
                    next_costs,
                    compile_history[column],
                    index,
                    index,
                    cell->glyph,
                    costs[index]
                );
            }
            if (!blank && state.mode == cell->mode &&
                state.foreground == cell->background &&
                state.background == cell->foreground) {
                relax(
                    next_costs,
                    compile_history[column],
                    index,
                    index,
                    (uint8_t)(cell->glyph | 0x80u),
                    costs[index]
                );
            }
            // Foreground/mode controls are set-after. Their own cell is a
            // space rendered with the old background. Prefer source spaces,
            // but permit sacrificing a glyph when the source changes mode
            // without leaving a cell for an SAA5050 control code.
            if (cell->background == state.background) {
                for (uint8_t colour = TT_RED; colour <= TT_WHITE; ++colour) {
                    display_state_t changed = state;
                    changed.foreground = colour;
                    changed.mode = TT_ALPHA;
                    relax(
                        next_costs,
                        compile_history[column],
                        index,
                        state_index(changed),
                        colour,
                        (uint16_t)(costs[index] + control_cost)
                    );
                    changed.mode = TT_GRAPHICS;
                    relax(
                        next_costs,
                        compile_history[column],
                        index,
                        state_index(changed),
                        (uint8_t)(0x10u + colour),
                        (uint16_t)(costs[index] + control_cost)
                    );
                }
            }
            // Background controls are set-at, so their own cell is shown in
            // the new background colour rather than the previous one.
            if (cell->background == TT_BLACK) {
                display_state_t changed = state;
                changed.background = TT_BLACK;
                relax(
                    next_costs,
                    compile_history[column],
                    index,
                    state_index(changed),
                    0x1cu,
                    (uint16_t)(costs[index] + control_cost)
                );
            }
            if (cell->background == state.foreground) {
                display_state_t changed = state;
                changed.background = state.foreground;
                relax(
                    next_costs,
                    compile_history[column],
                    index,
                    state_index(changed),
                    0x1du,
                    (uint16_t)(costs[index] + control_cost)
                );
            }
        }
        memcpy(costs, next_costs, sizeof(costs));
    }

    uint8_t final_state = 0u;
    uint16_t final_cost = TT_INFINITE_COST;
    for (uint8_t state = 0u; state < TT_STATE_COUNT; ++state) {
        if (costs[state] < final_cost) {
            final_cost = costs[state];
            final_state = state;
        }
    }
    if (final_cost == TT_INFINITE_COST) {
        return false;
    }
    for (size_t column = TELETEKST_COLUMNS; column-- > 0u;) {
        const predecessor_t predecessor = compile_history[column][final_state];
        if (!predecessor.valid) {
            return false;
        }
        output[column] = predecessor.output;
        final_state = predecessor.previous_state;
    }
    return true;
}

/** @copydoc teletekst_decode_nos_json_diagnostic */
teletekst_decode_result_t teletekst_decode_nos_json_diagnostic(
    const char *json,
    size_t json_length,
    uint16_t requested_page,
    uint8_t screen[TELETEKST_SCREEN_SIZE],
    uint8_t *next_subpage,
    uint8_t *failed_row
) {
    if (failed_row != NULL) {
        *failed_row = 0u;
    }
    if (json == NULL || json_length == 0u || screen == NULL ||
        next_subpage == NULL || requested_page < 100u || requested_page > 899u) {
        return TELETEKST_DECODE_INVALID_ARGUMENT;
    }
    const char *end = json + json_length;
    uint8_t parsed_next_subpage;
    if (!parse_next_subpage(
            json,
            end,
            requested_page,
            &parsed_next_subpage
        )) {
        return TELETEKST_DECODE_INVALID_NEXT_SUBPAGE;
    }
    const binary_display_result_t binary_display = parse_binary_display(
        json,
        end,
        screen
    );
    if (binary_display == BINARY_DISPLAY_INVALID) {
        return TELETEKST_DECODE_INVALID_BINARY_DISPLAY;
    }
    if (binary_display == BINARY_DISPLAY_VALID) {
        *next_subpage = parsed_next_subpage;
        return TELETEKST_DECODE_OK;
    }
    if (!parse_content(json, end, decode_cells)) {
        return TELETEKST_DECODE_INVALID_CONTENT;
    }
    for (size_t row = 0u; row < TELETEKST_DISPLAY_ROWS; ++row) {
        if (!compile_row(
                decode_cells[row],
                screen + row * TELETEKST_COLUMNS
            )) {
            if (failed_row != NULL) {
                *failed_row = (uint8_t)(row + 1u);
            }
            return TELETEKST_DECODE_UNREPRESENTABLE_ROW;
        }
    }
    *next_subpage = parsed_next_subpage;
    return TELETEKST_DECODE_OK;
}

/** @copydoc teletekst_decode_nos_json */
bool teletekst_decode_nos_json(
    const char *json,
    size_t json_length,
    uint16_t requested_page,
    uint8_t screen[TELETEKST_SCREEN_SIZE],
    uint8_t *next_subpage
) {
    return teletekst_decode_nos_json_diagnostic(
        json,
        json_length,
        requested_page,
        screen,
        next_subpage,
        NULL
    ) == TELETEKST_DECODE_OK;
}

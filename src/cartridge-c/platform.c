#include "platform.h"

#define VIDEO_RAM ((volatile uint8_t *)0x5000)

static const uint8_t keymap[144] = {
    0,   '6', 0,   'q', '3', '5', '7', '4', 0,   'h', 'z', 's', 'd', 'g', 'j',
    'f', 0,   ' ', '0', '0', '#', 0,   ',', 0,   0,   'n', '<', 'x', 'c', 'b',
    'm', 'v', 0,   'y', 'a', 'w', 'e', 't', 'u', 'r', 0,   '9', '+', '-', 8,
    '0', '1', '-', '9', 'o', '8', '7', 13,  'p', '8', '@', '3', '.', '2', '1',
    0,   '/', 'k', '2', '6', 'l', '5', '4', 0,   ';', 'i', ':', 0,   '&', 0,
    'Q', 0,   '%', 0,   '$', 0,   'H', 'Z', 'S', 'D', 'G', 'J', 'F', 0,   ' ',
    0,   0,   0,   0,   0,   0,   0,   'N', '>', 'X', 'C', 'B', 'M', 'V', 0,
    'Y', 'A', 'W', 'E', 'T', 'U', 'R', 0,   ')', 0,   0,   8,   '=', '!', '_',
    0,   'O', 0,   0,   13,  'P', '(', 0,   0,   0,   0,   0,   0,   '?', 'K',
    '"', 0,   'L', 0,   0,   0,   '+', 'I', '*',
};

uint8_t platform_read_ascii(void) {
  uint8_t key;
  do {
    key = platform_read_key();
  } while (key >= sizeof(keymap) || keymap[key] == 0u);
  return keymap[key];
}

void platform_clear_screen(void) {
  uint8_t row;
  uint8_t column;

  for (row = 0; row != P2000T_SCREEN_ROWS; ++row) {
    volatile uint8_t *line = VIDEO_RAM + ((uint16_t)row * P2000T_VIDEO_STRIDE);
    for (column = 0; column != P2000T_SCREEN_COLUMNS; ++column) {
      line[column] = ' ';
    }
  }
}

void platform_clear_line(uint8_t row) {
  uint8_t column;
  volatile uint8_t *line = VIDEO_RAM + ((uint16_t)row * P2000T_VIDEO_STRIDE);
  for (column = 0u; column != P2000T_SCREEN_COLUMNS; ++column)
    line[column] = ' ';
}

void platform_write_text(uint8_t row, uint8_t column, const char *text) {
  volatile uint8_t *destination =
      VIDEO_RAM + ((uint16_t)row * P2000T_VIDEO_STRIDE) + column;

  while (*text != '\0' && column != P2000T_SCREEN_COLUMNS) {
    *destination++ = (uint8_t)*text++;
    ++column;
  }
}

void platform_write_u8(uint8_t row, uint8_t column, uint8_t value) {
  char digits[4];
  uint8_t position = 0;

  if (value >= 100u) {
    digits[position++] = (char)('0' + value / 100u);
    value %= 100u;
  }
  if (position != 0u || value >= 10u) {
    digits[position++] = (char)('0' + value / 10u);
  }
  digits[position++] = (char)('0' + value % 10u);
  digits[position] = '\0';
  platform_write_text(row, column, digits);
}

void platform_write_bytes(uint8_t row, uint8_t column, const uint8_t *data,
                          uint8_t length) {
  volatile uint8_t *destination =
      VIDEO_RAM + ((uint16_t)row * P2000T_VIDEO_STRIDE) + column;
  while (length-- != 0u && column++ != P2000T_SCREEN_COLUMNS)
    *destination++ = *data++;
}

void platform_present_screen(const uint8_t *screen) {
  uint8_t row;
  uint8_t column;

  for (row = 0u; row != P2000T_SCREEN_ROWS; ++row) {
    volatile uint8_t *destination =
        VIDEO_RAM + ((uint16_t)row * P2000T_VIDEO_STRIDE);
    for (column = 0u; column != P2000T_SCREEN_COLUMNS; ++column)
      *destination++ = *screen++;
  }
}

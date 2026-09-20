#ifndef P2000T_PLATFORM_H
#define P2000T_PLATFORM_H

#include <stdint.h>

enum {
  P2000T_SCREEN_COLUMNS = 40,
  P2000T_SCREEN_ROWS = 24,
  P2000T_VIDEO_STRIDE = 80,
};

uint8_t platform_read_key(void);
uint8_t platform_read_ascii(void);
void platform_halt(void);
uint16_t platform_clock(void);
uint8_t platform_link_status(void);
uint8_t platform_link_receive(void);
void platform_link_send(uint8_t value) __z88dk_fastcall;
void platform_clear_screen(void);
void platform_clear_line(uint8_t row);
void platform_write_text(uint8_t row, uint8_t column, const char *text);
void platform_write_bytes(uint8_t row, uint8_t column, const uint8_t *data,
                          uint8_t length);
void platform_write_u8(uint8_t row, uint8_t column, uint8_t value);
void platform_present_screen(const uint8_t *screen);

#endif

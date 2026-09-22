/**
 * @file platform.h
 * @brief P2000T monitor, keyboard, video, clock, and cartridge-port interface.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the P2000T Teletekst cartridge and is licensed under
 * version 3 of the GNU General Public License. See the repository LICENSE.
 */

#ifndef P2000T_PLATFORM_H_
#define P2000T_PLATFORM_H_

#include <stdint.h>

enum {
  P2000T_SCREEN_COLUMNS = 40,
  P2000T_SCREEN_ROWS = 24,
  P2000T_VIDEO_STRIDE = 80,
  P2000T_KEY_RIGHT = 0xfb,
  P2000T_KEY_LEFT = 0xfc,
  P2000T_KEY_START = 0xfd,
  P2000T_KEY_STOP = 0xfe,
};

/**
 * @brief Reads one key through the P2000T monitor.
 * @return Monitor key code.
 */
uint8_t platform_read_key(void);

/**
 * @brief Queries the monitor keyboard FIFO without blocking.
 * @return Zero if empty, one for a regular key, or two for STOP.
 */
uint8_t platform_key_status(void);

/**
 * @brief Reads the next usable translated character.
 * @return ASCII or cartridge control key, excluding START and STOP.
 */
uint8_t platform_read_ascii(void);

/**
 * @brief Converts a P2000T matrix/monitor key code into ASCII or a control key.
 * @param key Raw key code.
 * @return Translated byte, or zero when unmapped.
 */
uint8_t platform_translate_key(uint8_t key);

/**
 * @brief Converts an uppercase ASCII letter to lowercase.
 * @param key Input byte.
 * @return Lowercase letter or the unchanged byte.
 */
uint8_t platform_lower_ascii(uint8_t key);

/**
 * @brief Enters the cartridge's terminal halt loop.
 */
void platform_halt(void);

/**
 * @brief Reads the monitor's 20 ms tick counter.
 * @return Current tick.
 */
uint16_t platform_clock(void);

/**
 * @brief Busy-waits for a number of 20 ms monitor ticks.
 * @param ticks Number of ticks to wait.
 */
void platform_wait_ticks(uint8_t ticks);

/**
 * @brief Reads Pico link status.
 * @return Raw status-port bits.
 */
uint8_t platform_link_status(void);

/**
 * @brief Reads one Pico link byte.
 * @return Raw receive-port byte.
 */
uint8_t platform_link_receive(void);

/**
 * @brief Writes one byte to the Pico link transmit port.
 * @param value Byte to send.
 */
void platform_link_send(uint8_t value) __z88dk_fastcall;

/**
 * @brief Clears all visible P2000T screen columns.
 */
void platform_clear_screen(void);

/**
 * @brief Clears one visible row.
 * @param row Screen row.
 */
void platform_clear_line(uint8_t row);

/**
 * @brief Writes a clipped null-terminated string to video RAM.
 * @param row Screen row.
 * @param column Starting column.
 * @param text Text to write.
 */
void platform_write_text(uint8_t row, uint8_t column, const char *text);

/**
 * @brief Writes clipped display bytes to video RAM.
 * @param row Screen row.
 * @param column Starting column.
 * @param data Bytes to write.
 * @param length Number of bytes.
 */
void platform_write_bytes(uint8_t row, uint8_t column, const uint8_t *data,
                          uint8_t length);

/**
 * @brief Reads clipped display bytes from video RAM.
 * @param row Screen row.
 * @param column Starting column.
 * @param[out] data Destination buffer.
 * @param length Number of bytes.
 */
void platform_read_bytes(uint8_t row, uint8_t column, uint8_t *data,
                         uint8_t length);

/**
 * @brief Writes an unsigned byte in compact decimal notation.
 * @param row Screen row.
 * @param column Starting column.
 * @param value Value to format.
 */
void platform_write_u8(uint8_t row, uint8_t column, uint8_t value);

/**
 * @brief Writes one byte as two uppercase hexadecimal digits.
 * @param row Screen row.
 * @param column Starting column.
 * @param value Value to format.
 */
void platform_write_hex(uint8_t row, uint8_t column, uint8_t value);

/**
 * @brief Writes a page/status number as three fixed-width decimal digits.
 * @param row Screen row.
 * @param column Starting column.
 * @param value Value from zero through 999.
 */
void platform_write_page(uint8_t row, uint8_t column, uint16_t value);

/**
 * @brief Atomically copies a packed 40-by-24 buffer to video RAM.
 * @param screen Packed display buffer.
 */
void platform_present_screen(const uint8_t *screen);

/**
 * @brief Formats date/time fields from the viewer-state layout.
 * @param state Byte view of the viewer state.
 * @param[out] out Destination display-byte buffer.
 * @return Number of formatted bytes.
 */
uint8_t platform_format_clock(const uint8_t *state, uint8_t *out);

/**
 * @brief Advances viewer clock fields when their deadline expires.
 * @param state Mutable byte view of the viewer state.
 * @return One when the visible clock should be redrawn.
 */
uint8_t platform_advance_clock(uint8_t *state) __z88dk_fastcall;

/**
 * @brief Updates conceal controls directly in an already visible normal page.
 * @param screen Raw 40-by-24 page buffer.
 * @param reveal Whether concealed text should be revealed.
 */
void platform_commit_reveal(const uint8_t *screen, uint8_t reveal);

/**
 * @brief Renders normal or zoomed SAA5050 page bytes into a packed buffer.
 * @param raw Raw 40-by-24 page bytes.
 * @param[out] display Packed output display.
 * @param zoom Zero for normal, one for top, or two for bottom half.
 * @param reveal Whether conceal controls should be replaced by current colour.
 */
void platform_render_page(const uint8_t *raw, uint8_t *display, uint8_t zoom,
                          uint8_t reveal);

#endif  // P2000T_PLATFORM_H_

/**
 * @file platform.c
 * @brief Portable keyboard translation and monitor timing helpers.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the P2000T Teletekst cartridge and is licensed under
 * version 3 of the GNU General Public License. See the repository LICENSE.
 */

#include "platform.h"

/** P2000T keyboard matrix codes mapped to cartridge input bytes. */
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

/**
 * @brief Translates a P2000T matrix/monitor key code into cartridge input.
 */
uint8_t platform_translate_key(uint8_t key) {
  if (key == 0u) return P2000T_KEY_LEFT;
  if (key == 23u) return P2000T_KEY_RIGHT;
  if (key == 128u) return P2000T_KEY_START;
  return key < sizeof(keymap) ? keymap[key] : 0u;
}

/**
 * @brief Converts an uppercase ASCII letter to lowercase.
 */
uint8_t platform_lower_ascii(uint8_t key) {
  return key >= 'A' && key <= 'Z' ? (uint8_t)(key + ('a' - 'A')) : key;
}

/**
 * @brief Busy-waits for a number of 20 ms monitor ticks.
 */
void platform_wait_ticks(uint8_t ticks) {
  uint16_t end = platform_clock() + ticks;
  while ((int16_t)(platform_clock() - end) < 0) {
  }
}

/**
 * @brief Reads the next translated non-START/non-STOP character.
 */
uint8_t platform_read_ascii(void) {
  uint8_t key;
  do {
    key = platform_translate_key(platform_read_key());
  } while (key == 0u || key == P2000T_KEY_START || key == P2000T_KEY_STOP);
  return key;
}

/**
 * @file main.c
 * @brief Cartridge startup and top-level P2WP/Wi-Fi dispatch.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the P2000T Teletekst cartridge and is licensed under
 * version 3 of the GNU General Public License. See the repository LICENSE.
 */

#include "p2wp.h"
#include "platform.h"
#include "teletekst.h"
#include "ui.h"
#include "wifi.h"

/**
 * @brief Starts the cartridge UI, protocol session, Wi-Fi, and page viewer.
 *
 * @return Zero in the unreachable event that the cartridge halt loop returns.
 */
int main(void) {
  p2wp_session_t session;
  enum p2wp_result result;
  uint8_t opening_timed_out;

  ui_opening_screen();

  opening_timed_out = ui_wait_opening();
  wifi_show_scanning();
  result = p2wp_hello(&session);
  if (result == P2WP_OK) {
    if (wifi_startup(&session)) teletekst_start(&session, opening_timed_out);
  } else if (result == P2WP_INCOMPATIBLE) {
    platform_write_text(10, 0, "GEEN GEDEELDE P2WP-VERSIE");
  } else if (result == P2WP_TIMEOUT) {
    platform_write_text(10, 0, "PICO W NIET GEVONDEN");
  } else {
    platform_write_text(10, 0, "ONGELDIG P2WP-ANTWOORD");
  }
  platform_halt();
  return 0;
}

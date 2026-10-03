/**
 * @file ui.h
 * @brief Shared SAA5050 screen-layout primitives for cartridge menus.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the P2000T Teletekst cartridge and is licensed under
 * version 3 of the GNU General Public License. See the repository LICENSE.
 */

#ifndef P2000T_UI_H_
#define P2000T_UI_H_

#include <stdint.h>

/**
 * @brief Draws the 14-row cartridge mosaic at the requested screen row.
 * @param[in] row First destination row.
 */
void ui_draw_logo(uint8_t row);

/**
 * @brief Draws the complete cartridge opening screen.
 */
void ui_opening_screen(void);

/**
 * @brief Waits for a key while running the 60-second opening countdown.
 * @return One if the countdown elapsed, or zero if a key was pressed.
 */
uint8_t ui_wait_opening(void);

/**
 * @brief Writes a white-on-blue full-width title row.
 * @param[in] row Screen row.
 * @param[in] text Null-terminated text after the control-byte prefix.
 */
void ui_title(uint8_t row, const char *text);

/**
 * @brief Writes a blue-on-white full-width content row.
 * @param[in] row Screen row.
 * @param[in] text Null-terminated text after the control-byte prefix.
 */
void ui_panel(uint8_t row, const char *text);

/** Action rows use the same white-on-blue style as title rows. */
#define ui_action ui_title

/**
 * @brief Draws a blue mosaic separator across one row.
 * @param[in] row Screen row.
 */
void ui_rule(uint8_t row);

/**
 * @brief Draws the standard cartridge version footer on row 23.
 */
void ui_footer(void);

/**
 * @brief Shows help page two with build information and the project URL.
 */
void ui_build_info(void);

/**
 * @brief Shows the styled Wi-Fi profile-save confirmation screen.
 */
void ui_save_prompt(void);

/**
 * @brief Shows the static custom-server editor panels.
 */
void ui_custom_setup(void);

/**
 * @brief Shows the static Wi-Fi scanning panels.
 */
void ui_wifi_scan(void);

#endif  // P2000T_UI_H_

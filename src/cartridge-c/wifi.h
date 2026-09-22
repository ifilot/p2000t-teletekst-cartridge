/**
 * @file wifi.h
 * @brief Wi-Fi setup and reconfiguration interface for the cartridge UI.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the P2000T Teletekst cartridge and is licensed under
 * version 3 of the GNU General Public License. See the repository LICENSE.
 */

#ifndef P2000T_WIFI_H_
#define P2000T_WIFI_H_

#include "p2wp.h"

/**
 * @brief Draws the initial Wi-Fi scan progress screen.
 */
void wifi_show_scanning(void);

/**
 * @brief Connects a saved profile or performs interactive Wi-Fi onboarding.
 * @param session Active negotiated protocol session.
 * @return One after connecting, otherwise zero.
 */
uint8_t wifi_startup(p2wp_session_t *session);

/**
 * @brief Discards password state and interactively chooses another network.
 * @param session Active negotiated protocol session.
 * @return One after connecting, otherwise zero.
 */
uint8_t wifi_reconfigure(p2wp_session_t *session);

#endif  // P2000T_WIFI_H_

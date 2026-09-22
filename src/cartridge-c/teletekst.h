/**
 * @file teletekst.h
 * @brief Entry point for the interactive Teletekst source and page viewer.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the P2000T Teletekst cartridge and is licensed under
 * version 3 of the GNU General Public License. See the repository LICENSE.
 */

#ifndef P2000T_TELETEKST_H_
#define P2000T_TELETEKST_H_

#include "p2wp.h"

/**
 * @brief Runs source selection and the interactive page viewer indefinitely.
 * @param session Active negotiated and network-connected protocol session.
 * @param opening_timed_out Whether the opening countdown selected autostart.
 */
void teletekst_start(p2wp_session_t *session, uint8_t opening_timed_out);

#endif  // P2000T_TELETEKST_H_

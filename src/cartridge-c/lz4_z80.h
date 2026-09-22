/**
 * @file lz4_z80.h
 * @brief Interface to the cartridge's compact raw-LZ4 decompressor.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the P2000T Teletekst cartridge and is licensed under
 * version 3 of the GNU General Public License. See the repository LICENSE.
 */

#ifndef P2000T_LZ4_Z80_H_
#define P2000T_LZ4_Z80_H_

#include <stdint.h>

/**
 * @brief Decompresses one size-delimited raw LZ4 block.
 *
 * The caller must provide a destination large enough for the decoded block.
 * The decoder intentionally omits framing and output-size checks to remain
 * small enough for the cartridge ROM.
 *
 * @param source Address of the raw LZ4 block in ROM.
 * @param destination Address of the output buffer in RAM.
 * @param compressed_size Number of compressed input bytes.
 */
void lz4_decompress(const uint8_t *source, uint8_t *destination,
                    uint16_t compressed_size);

#endif  // P2000T_LZ4_Z80_H_

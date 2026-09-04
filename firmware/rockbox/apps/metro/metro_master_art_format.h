/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 Ricardo Gomez
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
/* M-097 (contract v16): the shared MASTER image cache format, pure
 * part -- no Rockbox dependency, so it compiles on the host for
 * test/test_master_art.c. The three firmware families (Aura, Metro,
 * moonlit) write and read the same files under /.aura/art/, so a JPEG
 * is decoded ONCE, by whichever firmware is active, and never again.
 *
 * File = 16-byte little-endian header + width*height RGB565 LE pixels,
 * row-contiguous, square, "fill and center crop", no corners/theme/
 * reflection (each family derives its own look from this at load).
 * Name = stable key (independent of tagcache seeks):
 *   albums  a-<crc32 hex8 of the representative track path>.<mtime>.art
 *   artists r-<crc32 of the image file path>.<mtime>.art
 *   photos  p-<crc32 of the photo file path>.<mtime>.art
 * A zero-byte "<key>.none" is the shared negative marker (no art, or
 * undecodable) -- nobody retries a JPEG while it exists. The crc is
 * Rockbox's crc_32() (CRC-32/MPEG-2: poly 0x04C11DB7, init 0xffffffff,
 * no reflection, no final xor); metro_master_art_crc32() is the same
 * algorithm so the host tests can pin the vectors. */
#ifndef METRO_MASTER_ART_FORMAT_H
#define METRO_MASTER_ART_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define METRO_MASTER_ART_MAGIC       0x5453414Du /* 'MAST' as LE uint32 */
#define METRO_MASTER_ART_HEADER_SIZE 16

/* Contract sizes (v16): albums and artists 130x130, photos 80x80. */
#define METRO_MASTER_ART_ALBUM_PX  130
#define METRO_MASTER_ART_ARTIST_PX 130
#define METRO_MASTER_ART_PHOTO_PX  80
#define METRO_MASTER_ART_MAX_PX    130

#define METRO_MASTER_ART_EXT       ".art"
#define METRO_MASTER_ART_NONE_EXT  ".none"

/* Longest key this module ever formats: "x-" + 8 hex + "." + a long
 * (up to 20 digits) + NUL -- 32 is comfortably enough. */
#define METRO_MASTER_ART_KEY_LEN 32

/* Pixel size for a subdirectory name ("albums"/"artists"/"photos");
 * 0 for anything else. */
int metro_master_art_px_for_subdir(const char *subdir);

/* CRC-32/MPEG-2, same output as Rockbox's crc_32(buf, len, 0xffffffff). */
uint32_t metro_master_art_crc32(const void *buf, size_t len);

/* Formats "<prefix>-<crc hex8>.<mtime>" into out. prefix is 'a', 'r'
 * or 'p'. */
void metro_master_art_format_key(char prefix, uint32_t crc, long mtime,
                                 char *out, size_t outsz);

/* M-102 (contract v18): the <mtime> of an ALBUM key is
 * max(mtime of the representative track, mtime of the sibling
 * cover.jpg if there is one).
 *
 * Why it is not just the track's: Studio can rewrite an album's
 * cover.jpg without touching a single music file -- a better scan, a
 * corrected aspect ratio, a re-fetch. With the track's mtime alone the
 * key never moves, so the old master and the old tile stay valid
 * forever and the new cover is never seen. That is hypothesis (a) of
 * D-338/M-096/D-055, and this is what closes it.
 *
 * A missing cover.jpg is passed as 0 (or any value <= 0), which leaves
 * the key exactly as it was before v18 -- so a library with embedded
 * art only does not churn its whole cache on the upgrade. */
long metro_master_art_album_mtime(long track_mtime, long cover_mtime);

/* Header <-> fields. pack() always succeeds; parse() returns false on
 * a bad magic or a zero dimension. flags/reserved are written as 0 and
 * ignored on read (forward-compatible). */
void metro_master_art_header_pack(uint8_t out[METRO_MASTER_ART_HEADER_SIZE],
                                  uint16_t width, uint16_t height);
bool metro_master_art_header_parse(const uint8_t in[METRO_MASTER_ART_HEADER_SIZE],
                                   uint16_t *width, uint16_t *height);

/* Integer box filter: src (spx x spx) -> dst (dpx x dpx), dpx <= spx.
 * Each destination pixel is the per-channel average of its source
 * box ([x*spx/dpx, (x+1)*spx/dpx) on both axes), so 130 -> 80 mixes
 * 1x1..2x2 source pixels -- the L2 80x80 tile Metro keeps under
 * /.aura/thumbs is derived from the master this way, never from a
 * second JPEG decode. */
void metro_master_art_box_down(const uint16_t *src, int spx, uint16_t *dst, int dpx);

/* Nearest-neighbour "fill and center crop": conceptually scales src
 * (sw x sh) until BOTH axes cover dw x dh, then samples the centered
 * dw x dh window -- the master's own geometry (square, no letterbox).
 * Same algorithm metro_thumbs.c's cover_crop() had for 80px, now
 * generic so it also produces the 130px masters. */
void metro_master_art_cover(const uint16_t *src, int sw, int sh,
                            uint16_t *dst, int dw, int dh);

/* Bilinear "fill and center crop" -- for the one place that scales a
 * master UP a lot (the 130px cover behind Now Playing, 320x240 at 30%
 * opacity), where nearest-neighbour blocks would show through. */
void metro_master_art_cover_bilinear(const uint16_t *src, int sw, int sh,
                                     uint16_t *dst, int dw, int dh);

#endif /* METRO_MASTER_ART_FORMAT_H */

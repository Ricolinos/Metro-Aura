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
#include <stdio.h>
#include <string.h>

#include "metro_master_art_format.h"

int metro_master_art_px_for_subdir(const char *subdir)
{
    if (!strcmp(subdir, "albums"))  return METRO_MASTER_ART_ALBUM_PX;
    if (!strcmp(subdir, "artists")) return METRO_MASTER_ART_ARTIST_PX;
    if (!strcmp(subdir, "photos"))  return METRO_MASTER_ART_PHOTO_PX;
    return 0;
}

/* Same 4-bit table as firmware/common/crc32.c's space-optimized
 * crc_32() (CCITT polynomial 0x04C11DB7, MSB first). Kept here rather
 * than #including Rockbox's so the host tests build without config.h;
 * test_master_art.c pins both against an independently computed
 * CRC-32/MPEG-2 vector. */
uint32_t metro_master_art_crc32(const void *buf, size_t len)
{
    static const uint32_t table[16] = {
        0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9,
        0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
        0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61,
        0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD
    };
    const unsigned char *p = buf;
    uint32_t crc = 0xffffffffu;

    while (len--)
    {
        crc = table[(crc >> 28) ^ (*p >> 4)] ^ (crc << 4);
        crc = table[(crc >> 28) ^ (*p & 0x0f)] ^ (crc << 4);
        p++;
    }
    return crc;
}

void metro_master_art_format_key(char prefix, uint32_t crc, long mtime,
                                 char *out, size_t outsz)
{
    snprintf(out, outsz, "%c-%08lx.%ld", prefix, (unsigned long)crc, mtime);
}

static void put_le16(uint8_t *p, uint16_t v) { p[0] = v & 0xff; p[1] = v >> 8; }
static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; p[2] = (v >> 16) & 0xff; p[3] = v >> 24;
}
static uint16_t get_le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void metro_master_art_header_pack(uint8_t out[METRO_MASTER_ART_HEADER_SIZE],
                                  uint16_t width, uint16_t height)
{
    put_le32(out + 0, METRO_MASTER_ART_MAGIC);
    put_le16(out + 4, width);
    put_le16(out + 6, height);
    put_le32(out + 8, 0);  /* flags */
    put_le32(out + 12, 0); /* reserved */
}

bool metro_master_art_header_parse(const uint8_t in[METRO_MASTER_ART_HEADER_SIZE],
                                   uint16_t *width, uint16_t *height)
{
    if (get_le32(in) != METRO_MASTER_ART_MAGIC)
        return false;
    *width = get_le16(in + 4);
    *height = get_le16(in + 6);
    return *width > 0 && *height > 0;
}

void metro_master_art_box_down(const uint16_t *src, int spx, uint16_t *dst, int dpx)
{
    int ox, oy;

    for (oy = 0; oy < dpx; oy++)
    {
        int y0 = oy * spx / dpx, y1 = (oy + 1) * spx / dpx;
        if (y1 <= y0) y1 = y0 + 1;
        if (y1 > spx) y1 = spx;
        for (ox = 0; ox < dpx; ox++)
        {
            int x0 = ox * spx / dpx, x1 = (ox + 1) * spx / dpx;
            unsigned r = 0, g = 0, b = 0, n = 0;
            int x, y;
            if (x1 <= x0) x1 = x0 + 1;
            if (x1 > spx) x1 = spx;
            for (y = y0; y < y1; y++)
                for (x = x0; x < x1; x++)
                {
                    uint16_t p = src[y * spx + x];
                    r += (p >> 11) & 0x1f;
                    g += (p >> 5) & 0x3f;
                    b += p & 0x1f;
                    n++;
                }
            r = (r + n / 2) / n;
            g = (g + n / 2) / n;
            b = (b + n / 2) / n;
            dst[oy * dpx + ox] = (uint16_t)((r << 11) | (g << 5) | b);
        }
    }
}

/* Shared geometry for both cover variants: the source scale factor
 * (16.16 fixed point) and the centered crop offset in SOURCE pixels.
 * "Fill": the axis that would fall short of the destination decides
 * the scale, the other one gets cropped. */
static void cover_geometry(int sw, int sh, int dw, int dh,
                           long *step_x, long *step_y, long *off_x, long *off_y)
{
    /* step = source pixels per destination pixel, same on both axes */
    long step;
    if ((long)sw * dh >= (long)sh * dw)
        step = ((long)sh << 16) / dh;  /* height limits: fit height, crop width */
    else
        step = ((long)sw << 16) / dw;  /* width limits: fit width, crop height */
    *step_x = *step_y = step;
    *off_x = (((long)sw << 16) - step * dw) / 2;
    *off_y = (((long)sh << 16) - step * dh) / 2;
    if (*off_x < 0) *off_x = 0;
    if (*off_y < 0) *off_y = 0;
}

void metro_master_art_cover(const uint16_t *src, int sw, int sh,
                            uint16_t *dst, int dw, int dh)
{
    long step_x, step_y, off_x, off_y;
    int ox, oy;

    cover_geometry(sw, sh, dw, dh, &step_x, &step_y, &off_x, &off_y);
    for (oy = 0; oy < dh; oy++)
    {
        int sy = (int)((off_y + step_y * oy) >> 16);
        if (sy >= sh) sy = sh - 1;
        for (ox = 0; ox < dw; ox++)
        {
            int sx = (int)((off_x + step_x * ox) >> 16);
            if (sx >= sw) sx = sw - 1;
            dst[oy * dw + ox] = src[sy * sw + sx];
        }
    }
}

void metro_master_art_cover_bilinear(const uint16_t *src, int sw, int sh,
                                     uint16_t *dst, int dw, int dh)
{
    long step_x, step_y, off_x, off_y;
    int ox, oy;

    cover_geometry(sw, sh, dw, dh, &step_x, &step_y, &off_x, &off_y);
    for (oy = 0; oy < dh; oy++)
    {
        long fy = off_y + step_y * oy;
        int sy = (int)(fy >> 16), wy = (int)((fy >> 8) & 0xff);
        int sy1;
        if (sy >= sh - 1) { sy = sh - 1; wy = 0; }
        sy1 = sy + (wy ? 1 : 0);
        for (ox = 0; ox < dw; ox++)
        {
            long fx = off_x + step_x * ox;
            int sx = (int)(fx >> 16), wx = (int)((fx >> 8) & 0xff);
            int sx1;
            uint16_t p00, p01, p10, p11;
            unsigned r, g, b;
            int w00, w01, w10, w11;
            if (sx >= sw - 1) { sx = sw - 1; wx = 0; }
            sx1 = sx + (wx ? 1 : 0);
            p00 = src[sy * sw + sx];  p01 = src[sy * sw + sx1];
            p10 = src[sy1 * sw + sx]; p11 = src[sy1 * sw + sx1];
            w11 = wx * wy; w10 = (256 - wx) * wy; w01 = wx * (256 - wy);
            w00 = 65536 - w11 - w10 - w01;
            r = (((p00 >> 11) & 31) * w00 + ((p01 >> 11) & 31) * w01 +
                 ((p10 >> 11) & 31) * w10 + ((p11 >> 11) & 31) * w11 + 32768) >> 16;
            g = (((p00 >> 5) & 63) * w00 + ((p01 >> 5) & 63) * w01 +
                 ((p10 >> 5) & 63) * w10 + ((p11 >> 5) & 63) * w11 + 32768) >> 16;
            b = ((p00 & 31) * w00 + (p01 & 31) * w01 +
                 (p10 & 31) * w10 + (p11 & 31) * w11 + 32768) >> 16;
            dst[oy * dw + ox] = (uint16_t)((r << 11) | (g << 5) | b);
        }
    }
}

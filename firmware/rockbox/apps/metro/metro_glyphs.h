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
/* R5 (M-089): big anti-aliased glyphs as 8-bit coverage masks, generated
 * by firmware/tools/gen_icons.py (GLYPHS list) into metro_glyphs_table.c
 * and COMMITTED, same pattern as metro_icons_table.c. For the screens
 * that cannot touch the disk (USB) and need more than a 16px cell:
 * scaling the monochrome 16px mask up gives blocks; this is the SVG
 * rasterised at its final size with real anti-aliasing. Drawn with
 * metro_widgets_draw_glyph(): colour of the caller's choice blended over
 * whatever is behind, pixel by pixel (metro_fb_plot_alpha). */
#ifndef METRO_GLYPHS_H
#define METRO_GLYPHS_H

struct metro_glyph {
    int width;
    int height;
    const unsigned char *alpha; /* width*height bytes, row-major, 0..255 */
};

extern const struct metro_glyph metro_glyph_sync_large;
/* M-104: pantalla de bloqueo en reposo (interruptor Hold puesto). */
extern const struct metro_glyph metro_glyph_lock_large;

#endif /* METRO_GLYPHS_H */

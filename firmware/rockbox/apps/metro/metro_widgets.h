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
#ifndef METRO_WIDGETS_H
#define METRO_WIDGETS_H

#include "metro_icons.h"
#include "metro_glyphs.h"

#include <stdbool.h>

/* The system's single modal dialog (PLAN_MAESTRO.md S2.3, MCTX_DIALOG):
 * blocks with its own small input loop (MCTX_DIALOG context) until the
 * user confirms or cancels. Defaults the selection to "no" -- the safe
 * answer -- never to "yes". Redraws the whole screen; the caller must
 * redraw whatever was behind it afterwards. */
bool metro_widgets_confirm(const char *title, const char *question);

/* M-100: igual, con una linea de detalle en caption bajo la pregunta,
 * para una advertencia que no cabe en las dos lineas de la pregunta. */
bool metro_widgets_confirm_detail(const char *title, const char *question,
                                   const char *detail);

/* R5-F3 (M-083): the F5 volume bar ("320x6 at y=232 + 'volume NN%'")
 * is gone -- Now Playing now shows the volume as a two-digit level
 * ("00".."15") drawn by metro_screen_nowplaying.c itself; see
 * metro_volume.h for the scale. Nothing else used the bar. */

/* R5-F3 (M-083) / corregido en M-086: 1px circle outline, centre
 * (cx, cy), radius r -- ANTI-ALIASED. M-083 drew it with the plain
 * midpoint algorithm and on the real panel it read as a staircase
 * (owner: "se notan muy pixelizados"); the spec's "que cuides el
 * antialiasing" meant smooth it, not avoid it. Coverage per pixel =
 * 1 - |distance - r| over the ring's bounding box, blended over what
 * is already there (metro_fb_plot_alpha), so it works on the flat
 * background and over the dimmed album art alike. (2r+1)^2 integer
 * sqrts per ring -- ~730 for r=13, three rings, once a second. */
void metro_widgets_draw_circle(int cx, int cy, int r, unsigned color);

/* R5-F3 (M-083): a metro_icons.h glyph centred inside a
 * metro_widgets_draw_circle() ring. (x, y) is the top-left of the
 * ring's bounding box, which is (2r+1) square; the 16px glyph is
 * centred in it. Ring and glyph take separate colours because the
 * player uses them for state (e.g. pause glyph in accent inside an fg
 * ring). */
void metro_widgets_draw_icon_in_circle(enum metro_icon_id id, int x, int y,
                                        int r, unsigned ring_color,
                                        unsigned glyph_color);

/* F10: floating index letter, shown ~600ms after a fast scroll (steps
 * >= 3) stops moving -- PLAN_MAESTRO.md S1.4. `c` is the first
 * character of the row the selection landed on, uppercased by the
 * caller. Draws over whatever the list already shows (a solid
 * MFONT_DISPLAY-sized accent square, centered) -- the caller decides
 * when to stop calling it and let the next real redraw clear it. */
void metro_widgets_draw_index_letter(const char *letter);

/* Empty-list state (PLAN_MAESTRO.md S1.4): a centered 96x96 accent
 * tile (blank -- metro_draw_tile() with no letter, this isn't naming
 * anything) plus a caption message below it. Callers: any pivot whose
 * count() is 0. */
void metro_widgets_draw_empty_state(const char *message);

/* F10: Now Playing's shuffle/repeat status icons (M-018's icon set),
 * replacing F5-1's deferred "shuffle"/"repeat todo" text badges --
 * simple line-art, top-left corner at (x, y), METRO_WIDGETS_ICON_SIZE
 * square. Repeat draws a small "1" centered over the loop when `one`
 * is true (REPEAT_ONE vs REPEAT_ALL). */
/* Lado de todos los glifos de estado. Coincide con METRO_ICON_SIZE
 * (metro_icons.h): los de Fluent que se usan son las variantes
 * dibujadas para 16px. */
#define METRO_WIDGETS_ICON_SIZE METRO_ICON_SIZE

/* R4/FA-1 (M-077): dibuja un glifo de metro_icons.h en (x, y) del color
 * dado, METRO_ICON_SIZE de lado. Sustituye a los iconos trazados a mano
 * uno por uno; los que quedan geométricos son los que ningún glifo del
 * set resuelve mejor (la batería, que es un indicador con relleno
 * proporcional, no un símbolo fijo). */
void metro_widgets_draw_icon(enum metro_icon_id id, int x, int y, unsigned color);

/* R5 (M-089): draws a big anti-aliased glyph (metro_glyphs.h) with its
 * top-left at (x, y), blending `color` over what is already there. */
void metro_widgets_draw_glyph(const struct metro_glyph *g, int x, int y, unsigned color);

#endif /* METRO_WIDGETS_H */

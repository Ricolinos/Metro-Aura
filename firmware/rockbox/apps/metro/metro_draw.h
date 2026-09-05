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
#ifndef METRO_DRAW_H
#define METRO_DRAW_H

#include "metro_fonts.h"
#include "metro_page.h"

/* How many rows metro_draw_rows() shows fully before the next one
 * starts peeking off the bottom -- callers need this to compute
 * metro_nav_move_sel()'s visible_rows argument consistently with what
 * actually gets drawn. */
#define METRO_DRAW_ROWS_VISIBLE 5

/* R3-F8/DD-9 (M-069): margen izquierdo compartido por la ceja del
 * encabezado, los nombres de pivot y las filas -- las tres cosas se
 * alinean en la misma vertical, que es lo que permite que el volador
 * de CONTINUUM viaje SOLO en y. Era un define privado de metro_draw.c
 * (METRO_ROWS_LEFT_X) hasta que metro_transitions.c necesitó la misma
 * x para no re-declarar el 12 por su cuenta. */
#define METRO_DRAW_LEFT_X 12

/* R3-F8/DD-9 (M-069): geometría de la lista de filas, expuesta por el
 * mismo motivo que METRO_DRAW_LEFT_X -- metro_screen_list.c necesita
 * calcular en qué y está dibujada la fila seleccionada para decirle a
 * CONTINUUM desde dónde arranca el vuelo. Eran privados de
 * metro_draw.c (METRO_ROWS_FIRST_Y / METRO_ROW_PITCH). */
#define METRO_DRAW_ROWS_FIRST_Y 84
#define METRO_DRAW_ROW_PITCH    28

/* High-level drawing shell over lcd_* -- see PLAN_MAESTRO.md S1.2.
 * metro_draw_tile()/metro_draw_progress() are still deferred -- F3
 * doesn't have tiles or a progress bar yet, those land with Now
 * Playing/Music Flow. metro_draw_rows()/metro_draw_pivots() were
 * deferred from F2 to F3 (docs/DESVIACIONES.md F2-1, because
 * metro_page.h didn't exist yet) and are implemented here now. */

/* Fills the whole screen with metro_color_bg(). */
void metro_draw_clear(void);

/* Draws str at (x,y) in the given role's font and color, on the full
 * screen viewport -- naturally clipped at the screen edges by the LCD
 * driver (INVESTIGACION.md A.6), no explicit width needed. */
void metro_draw_text(enum metro_font_role role, int x, int y,
                      const char *str, unsigned color);

/* M-106: ancho en pixeles de `str` en la fuente de `role` -- la misma
 * medida que usa metro_draw_text(). Existe para que la marquesina
 * (metro_marquee.h) decida si un texto desborda su banda sin que cada
 * llamador repita el par lcd_setfont()/lcd_getstringsize() y sin
 * arriesgarse a medir con un rol distinto del que dibuja. */
int metro_draw_text_width(enum metro_font_role role, const char *str);

/* M-117 (addendum de moonlit a su D-081): ancho Y ALTO de `str` en el
 * rol dado, midiendo tramo por tramo igual que metro_draw_text().
 *
 * Es la única forma correcta de medir texto desde fuera de este
 * módulo. El par `lcd_setfont(metro_font_id(role))` +
 * `lcd_getstringsize()` que usaban los llamadores mide un codepoint
 * cirílico con la fuente PRIMARIA, que no lo tiene: devuelve el ancho
 * del `defaultchar`, no el del glifo que metro_draw_text() acaba
 * dibujando con la fuente cirílica. El glifo salía bien y la medida
 * mal -- centrados corridos, marquesinas con el ciclo corto y las dos
 * copias encimadas, textos recortados donde no tocaba.
 *
 * `w` y/o `h` pueden ser NULL. Deja la fuente del viewport en la del
 * último tramo medido: si un llamador medía una vez y luego hacía
 * varias `lcd_getstringsize()` a pelo confiando en el
 * `lcd_setfont()` inicial, tiene que pasar TODAS por aquí. */
void metro_draw_text_size(enum metro_font_role role, const char *str,
                          int *w, int *h);

/* Same, but clipped to a viewport of exactly clip_w pixels starting
 * at x -- for content that must never run into a fixed-width
 * neighbour (e.g. a row that must stop before a right-aligned icon).
 * Metro-language "recorte en el borde derecho": pass a clip_w
 * narrower than the string's rendered width and the tail is cut, not
 * wrapped or refused. */
void metro_draw_text_cut_right(enum metro_font_role role, int x, int y,
                                const char *str, unsigned color, int clip_w);

/* R5-F5 (M-085): the general form -- the string is positioned at
 * absolute (x, y) but only the part inside [clip_x, clip_x + clip_w)
 * is drawn; x may be LESS than clip_x (text sliding out to the left
 * under a fixed margin -- the hub's marquee). metro_draw_text_cut_right
 * is this with clip_x == x. */
void metro_draw_text_clipped(enum metro_font_role role, int clip_x, int clip_w,
                              int x, int y, const char *str, unsigned color);

/* Top line: page_title in caption/secondary at the left, current time
 * (if the RTC has one) and battery percentage at the right. */
void metro_draw_header(const char *page_title);

/* Battery icon (rect body + nub, proportionally filled) ending at
 * (x_right, y) -- M-018's icon set, F10. 20px wide total. */
void metro_draw_battery(int x_right, int y);

/* Pivot header at y=28..76 (PLAN_MAESTRO.md S1.4): draws pivots from
 * active_pivot onwards only -- pivots to the left of the active one
 * are never drawn (WP7 Pivot behaviour, no "peek back"). The active
 * pivot is in metro_color_fg(), the rest in metro_color_tertiary();
 * natural screen-edge clipping (A.6) cuts the last one that doesn't
 * fully fit. x_offset shifts everything horizontally, for the slide
 * transition that lands in F11 -- always 0 for now. */
void metro_draw_pivots(const struct metro_page *page, int active_pivot,
                        int x_offset);

/* Row list at y=84.. (PLAN_MAESTRO.md S1.4): `first` is the first
 * visible row (metro_nav_first_visible()), `sel` the selected one
 * (metro_nav_sel()) drawn in MFONT_LIST_SEL/metro_color_fg(), the
 * rest in MFONT_LIST/metro_color_secondary(). Draws one row past the
 * visible window on purpose -- it peeks, cut by the bottom of the
 * screen, exactly like the pivot header. x_offset: see
 * metro_draw_pivots(). */
void metro_draw_rows(const struct metro_pivot *pivot, int first, int sel,
                      int x_offset);

/* Same as metro_draw_rows(), plus y_offsets: NULL for the identical
 * behaviour, or an array of METRO_DRAW_ROWS_VISIBLE+1 pixel offsets
 * (one per visible row slot, added to that row's y) -- F12's FEATHER
 * cascade (metro_screen_list.c) redraws with this every frame instead
 * of duplicating metro_draw_rows()'s own loop. */
void metro_draw_rows_ex(const struct metro_pivot *pivot, int first, int sel,
                         int x_offset, const int *y_offsets);

/* Clears just the row area (y >= METRO_ROWS_FIRST_Y) with
 * metro_color_bg() -- FEATHER's per-frame redraw needs this between
 * frames without paying for metro_draw_clear()'s full-screen fill
 * (header/pivots above METRO_ROWS_FIRST_Y never change during the
 * cascade). */
void metro_draw_clear_rows_area(void);

/* Flat bar: metro_color_tertiary() background, metro_color_accent()
 * fill for the first `pct` percent (0..100, clamped). Now Playing's
 * progress bar (PLAN_MAESTRO.md S1.4: 320x4 at y=214), reusable
 * anywhere else a determinate progress needs showing. */
void metro_draw_progress(int x, int y, int width, int height, int pct);

/* metro_color_accent() square with a single big uppercase initial
 * (first byte of `label`, ASCII only) centered in MFONT_DISPLAY/bg --
 * the no-album-art fallback (PLAN_MAESTRO.md S1.4 "tile acento con
 * inicial del album"). */
void metro_draw_tile(int x, int y, int size, const char *label);

/* R2-F2/DD-8: grid geometry -- 4 flush 80x80 tiles per row (Metro/Zune
 * HD: no gap, no margin, unlike Aura's 55px-with-margin photo grid),
 * 2 full rows visible before the natural screen-edge clip at y=240
 * peeks the start of a 3rd (row 2 sits at y=164, 164+80=244 -- the
 * bottom 76px show, same "asoma cortado" rule metro_draw_rows_ex()
 * already uses, no extra peek row needed here since the geometry does
 * it on its own). */
#define METRO_TILE_SIZE         80
#define METRO_TILE_COLS         4
#define METRO_TILE_ROWS_VISIBLE 2

/* Grid counterpart of metro_draw_rows() -- `first` MUST already be
 * aligned to a multiple of METRO_TILE_COLS (metro_nav_move_sel_grid()
 * guarantees this), `sel` the selected linear index. Draws up to
 * METRO_TILE_COLS*METRO_TILE_ROWS_VISIBLE tiles starting at `first`:
 * pivot->get_tile() supplies each tile's bitmap (already decoded/cached
 * RAM, DD-9), or metro_draw_tile() with that row's title is the
 * fallback when it returns NULL (thumbnail not ready yet, or the
 * pivot has none). The selected tile gets a 3px metro_color_accent()
 * border drawn INSIDE its bounds -- the tile stays 80x80, never grows. */
void metro_draw_tiles(const struct metro_pivot *pivot, int first, int sel,
                       int x_offset);

#endif /* METRO_DRAW_H */

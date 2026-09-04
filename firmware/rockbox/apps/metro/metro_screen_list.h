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
#ifndef METRO_SCREEN_LIST_H
#define METRO_SCREEN_LIST_H

#include "metro_nav.h"
#include "metro_page.h"
#include "metro_keymap.h"

/* Owns the app's single metro_nav_t instance (PLAN_MAESTRO.md S1.1:
 * "la pantalla genérica dibuja cualquier proveedor") and the stack of
 * pages pushed on top of it -- depth 1 is always the hub, which does
 * NOT go through this screen (metro_screen_hub.c draws it directly)
 * but DOES share this same nav_t for its own selection/windowing,
 * via metro_screen_nav(). Depths 2+ are pages pushed with
 * metro_screen_list_push(); each pushed depth corresponds 1:1 to a
 * metro_nav_t frame, both stacks always move together. */

void metro_screen_list_init(void);

/* Shared nav instance -- metro_screen_hub.c reads/writes depth 1
 * through this, nothing else needs it directly. */
metro_nav_t *metro_screen_nav(void);

/* Pushes page and a matching metro_nav_t frame together. Returns
 * false (no-op) if the stack is already at METRO_NAV_MAX_DEPTH. */
bool metro_screen_list_push(const struct metro_page *page);

/* M-106: fija la seleccion del pivot ACTUAL, con el ventaneo que
 * corresponda (rejilla o lista, segun tile_cols). Existe para que el
 * visor de fotos, al volver, deje la cuadricula sobre la foto que se
 * estaba viendo y no sobre la que se abrio -- si el usuario recorrio
 * veinte fotos, volver al principio es perder su lugar. Fuera de rango
 * o sin pagina, no hace nada. */
void metro_screen_list_set_sel(int index);
bool metro_screen_list_pop(void);
void metro_screen_list_pop_to_root(void);

/* Draws the page currently on top of the stack (pivots header + rows
 * + page header). No-op if the stack is empty (depth 1, i.e. the hub
 * is showing -- metro_main.c decides which screen to draw). */
void metro_screen_list_show(void);

/* NULL at the hub (depth 1); otherwise the page passed to the most
 * recent metro_screen_list_push() still on the stack. F5:
 * metro_screen_nowplaying.c compares this against its own sentinel
 * page pointer so metro_main.c can tell "a page is on top" (draw with
 * this module) from "specifically Now Playing is on top" (draw with
 * metro_screen_nowplaying instead) without this module knowing
 * anything about Now Playing itself. */
const struct metro_page *metro_screen_list_current_page(void);

/* F10: true while the floating index letter (metro_widgets.h) is
 * still on screen -- metro_main.c polls this to redraw again once it
 * expires, since nothing else about the list changed in the meantime. */
bool metro_screen_list_has_pending_redraw(void);

/* Reacts to one MACT_* action against the top-of-stack page: PIVOT_*
 * switches pivots, PREV/NEXT moves the selection, SELECT calls the
 * current row's on_select(), BACK pops, HOME pops to the root. Does
 * NOT redraw -- the caller (metro_main.c) decides when to. */
void metro_screen_list_handle(int action, int steps);

/* F12: FEATHER (PLAN_MAESTRO.md S3.3) -- if the top-of-stack page was
 * just pushed (metro_screen_list_push() sets a pending flag), runs
 * the staggered row-entrance cascade over the CURRENT screen contents
 * and clears the flag; a plain no-op otherwise (nothing was pushed,
 * animations isn't effectively `all`, or the LCD is off). Blocking,
 * like every other transition in F11/F12 -- metro_main.c calls this
 * right after metro_transitions_push() settles, only when the result
 * is a list page (never the hub, never Now Playing). */
void metro_screen_list_run_feather_if_pending(void);

#endif /* METRO_SCREEN_LIST_H */

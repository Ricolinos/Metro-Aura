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
#ifndef METRO_PAGE_H
#define METRO_PAGE_H

#include "lcd.h" /* fb_data, R2-F2/DD-7 -- struct metro_pivot's get_tile() */
#include "metro_lang.h"

/* Declarative model for any twist page (PLAN_MAESTRO.md S1.1 point 3):
 * a page is a title + a small array of pivots; each pivot is a
 * provider (count/get_row/on_select), not a data copy. The generic
 * list screen (metro_screen_list.c) can render and drive ANY page
 * built from this without knowing what it actually contains --
 * artists, videos, settings rows, whatever.
 *
 * Page titles and pivot names are enum metro_lang_id, NOT a resolved
 * const char* -- these tables are static const arrays (compile-time
 * initializers), and metro_lang_str() is a function call, not a
 * constant expression, so it can't appear in one. Whoever draws a
 * page/pivot resolves the id at that moment
 * (metro_draw_header()/metro_draw_pivots() do this internally), which
 * is also what makes metro_lang_set() take effect immediately on the
 * next redraw. Row title/subtitle have no such restriction -- get_row()
 * runs at draw time, so it can call metro_lang_str() directly. */

enum metro_row_kind {
    METRO_ROW_NAV,     /* selecting it pushes a new page */
    METRO_ROW_ACTION,  /* selecting it performs an action, no push */
    METRO_ROW_SETTING, /* title = setting name, subtitle = current value */
};

struct metro_row {
    const char *title;
    const char *subtitle; /* NULL if none */
    enum metro_row_kind kind;
};

struct metro_pivot {
    enum metro_lang_id name;
    int  (*count)(void *ctx);
    void (*get_row)(void *ctx, int index, struct metro_row *out);
    /* Called on SELECT over row `index`. Free to do nothing, mutate
     * ctx (METRO_ROW_SETTING: cycle a value), or push a child page
     * via metro_screen_list_push() (METRO_ROW_NAV). */
    void (*on_select)(void *ctx, int index);
    void *ctx;

    /* R2-F2/DD-7: appended at the end on purpose -- every existing
     * positional initializer of struct metro_pivot across the codebase
     * stays valid (these two fields default to 0/NULL, meaning "plain
     * row list", the behaviour every pivot had before this phase).
     * tile_cols > 0 switches this pivot to a grid of square tiles
     * (metro_screen_list.c calls metro_draw_tiles()/
     * metro_nav_move_sel_grid() instead of the row-list path) --
     * get_tile() then supplies each tile's already-decoded bitmap
     * (RAM-resident, DD-9's thumbnail engine owns decoding/caching),
     * or NULL to fall back to an accent tile with the row's initial
     * letter (metro_draw_tile(), same placeholder plain rows never
     * needed because they don't have a "picture"). */
    int tile_cols;
    const fb_data *(*get_tile)(void *ctx, int index);

    /* R3-F4/DD-5 (M-065): appended at the end, same reasoning as
     * tile_cols/get_tile above -- every existing positional
     * initializer keeps working (defaults to 0). 0 means "no custom
     * message, use the generic LANG_EMPTY_LIST" (metro_screen_list.c).
     * A pivot whose emptiness means something more specific than "not
     * synced yet" -- Quickplay's "sin historial todavía" would be
     * actively wrong advice if it said "sincroniza con Aura Studio" --
     * sets this to its own lang id instead. */
    enum metro_lang_id empty_message;

    /* M-101: appended at the end for the same reason as every field
     * above -- positional initializers stay valid and default to NULL,
     * which means "SELECT held does nothing here", the behaviour every
     * pivot had before. Only "acerca de" sets it today (reveals the
     * main-stack watermark row). */
    void (*on_select_hold)(void *ctx, int index);
};

struct metro_page {
    enum metro_lang_id title;
    const struct metro_pivot *pivots;
    int npivots;
    /* F4: NULL for every static page (the common case -- title resolved
     * from `title` above, same as a pivot name). Non-NULL overrides it
     * verbatim: a page whose header names a piece of the user's own
     * library (an artist, an album, a genre) can't use `title`, because
     * that string isn't UI chrome and was never going to fit in a
     * compile-time enum metro_lang_id -- it's data, pulled from
     * tagcache at the moment the page is pushed. Owned by whoever
     * pushes the page (a static buffer alive for as long as the page
     * stays on the nav stack is enough, see metro_screen_hub.c). */
    const char *title_dynamic;
};

#endif /* METRO_PAGE_H */

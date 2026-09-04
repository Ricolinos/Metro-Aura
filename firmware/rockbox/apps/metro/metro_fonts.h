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
#ifndef METRO_FONTS_H
#define METRO_FONTS_H

#include <stdbool.h>

/* Five type roles, all Selawik (SIL OFL) -- see DECISIONS.md M-010.
 * Loaded once, fully (font_load_ex(path, 0, N), never the glyph-cache
 * path -- there's no need on this target, see INVESTIGACION.md A.7
 * errata: MEMORYSIZE=64 is injected by tools/configure, so there is
 * no 10KB MAX_FONT_SIZE limit here). Falls back to FONT_SYSFIXED for
 * any role whose .fnt is missing or fails to load, so the UI never
 * ends up with nothing to draw text with. */
enum metro_font_role {
    MFONT_DISPLAY = 0, /* Selawik Light 48px -- hub rows, giant titles */
    MFONT_TITLE,       /* Selawik Light 28px -- Now Playing title, pivot headers */
    MFONT_LIST,        /* Selawik Regular 20px -- unselected list rows */
    MFONT_LIST_SEL,    /* Selawik Semibold 20px -- selected list row */
    MFONT_CAPTION,     /* Selawik Regular 14px -- header, subtitles, values */
    MFONT_COUNT
};

/* Loads all 5 roles from FONT_DIR ("/.rockbox/fonts/metro-*.fnt").
 * Safe to call more than once (e.g. after a language/theme change
 * that might swap fonts in the future) -- reloading just replaces the
 * cached ids. Call once from metro_main() before the first screen
 * draws. */
void metro_fonts_init(void);

/* Rockbox font id for a role, ready to pass to lcd_setfont()/
 * font_getstringsize(). Always valid -- returns FONT_SYSFIXED if the
 * role's .fnt never loaded. */
int metro_font_id(enum metro_font_role role);

/* M-114 (ronda "ajustes 2", Fase 5): true if `role` has its own
 * Cyrillic font (Inter, M-113) -- every role except MFONT_DISPLAY,
 * which measured over half its Cyrillic-range budget in one font and
 * falls back to MFONT_TITLE instead (see metro_font_cyrillic_id()
 * below, and DECISIONS.md M-113/M-114 for the measurement). */
bool metro_font_has_cyrillic(enum metro_font_role role);

/* M-114: the Cyrillic font id for `role` -- only valid (and only
 * meaningful to call) when metro_font_has_cyrillic() is true for that
 * exact role; metro_textseg_build()'s has_cyrillic_font parameter is
 * what actually gates whether a caller reaches this at all. For
 * MFONT_DISPLAY this returns metro_font_cyrillic_id(MFONT_TITLE)
 * unconditionally (the documented fallback, not a load-failure path).
 * Falls back further to metro_font_id(role) if the Cyrillic .fnt never
 * loaded, same "never leave the caller with an invalid font" rule as
 * metro_font_id() itself. */
int metro_font_cyrillic_id(enum metro_font_role role);

#endif /* METRO_FONTS_H */

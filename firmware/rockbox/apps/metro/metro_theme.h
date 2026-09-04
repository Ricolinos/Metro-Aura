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
#ifndef METRO_THEME_H
#define METRO_THEME_H

/* Active theme (dark/light) + accent state, and the role->color
 * resolver that reads metro_palette.h -- see DECISIONS.md M-012. F2:
 * RAM-only, always starts at the defaults (dark, magenta). Persisting
 * the user's choice to aura.cfg is metro_settings.c's job (F6/F8);
 * when that lands it will call metro_theme_set()/metro_accent_set()
 * once at boot after reading the saved values, nothing here changes. */

enum metro_theme_kind {
    METRO_THEME_DARK = 0,
    METRO_THEME_LIGHT
};

enum metro_accent {
    METRO_ACCENT_BLUE = 0,
    METRO_ACCENT_BROWN,
    METRO_ACCENT_GREEN,
    METRO_ACCENT_LIME,
    METRO_ACCENT_MAGENTA,
    METRO_ACCENT_MANGO,
    METRO_ACCENT_PINK,
    METRO_ACCENT_PURPLE,
    METRO_ACCENT_RED,
    METRO_ACCENT_TEAL,
    METRO_ACCENT_COUNT
};

#define METRO_ACCENT_DEFAULT METRO_ACCENT_MAGENTA
#define METRO_THEME_DEFAULT  METRO_THEME_DARK

void metro_theme_init(void);

enum metro_theme_kind metro_theme_get(void);
void metro_theme_set(enum metro_theme_kind theme);

enum metro_accent metro_accent_get(void);
void metro_accent_set(enum metro_accent accent);

/* Resolved to the active theme/accent -- pass straight to
 * lcd_set_foreground()/lcd_set_background() or LCD_RGBPACK-style
 * drawing calls. */
unsigned metro_color_bg(void);
unsigned metro_color_fg(void);
unsigned metro_color_secondary(void);
unsigned metro_color_tertiary(void);
unsigned metro_color_accent(void);

/* M-105 (plan maestro SS F): el acento MEZCLADO hacia el fondo al 55 %.
 *
 * El problema que resuelve: el tile de respaldo (el que se dibuja
 * cuando un album o un artista no tiene imagen) y el marco de seleccion
 * usaban el MISMO acento puro, asi que sobre una cuadricula sin
 * caratulas la seleccion desaparecia -- un cuadro de acento rodeado de
 * acento. La regla comun a las tres familias es: **el acento puro es
 * solo para el estado activo; los rellenos de respaldo usan una
 * variante derivada**.
 *
 * Derivada, no un color nuevo: se calcula con metro_fb_blend_color()
 * desde el acento vigente y el fondo del tema vigente, asi que sigue
 * habiendo CERO literales RGB fuera de metro_palette.h (regla del
 * CLAUDE.md) y funciona igual en tema claro y oscuro sin una segunda
 * tabla. */
unsigned metro_color_accent_dim(void);

/* Any of the 10 accents by index, regardless of which one is active
 * -- e.g. for a settings screen listing all of them, or (F2) the type
 * specimen's accent strip. Returns METRO_ACCENT_COLOR_MAGENTA for an
 * out-of-range index. */
unsigned metro_accent_color(enum metro_accent accent);

#endif /* METRO_THEME_H */

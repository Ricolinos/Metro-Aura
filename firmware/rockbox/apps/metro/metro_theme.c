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
#include "metro_theme.h"
#include "metro_palette.h"
#include "metro_fb.h" /* M-105: metro_fb_blend_color() */

static const unsigned accent_colors[METRO_ACCENT_COUNT] = {
    METRO_ACCENT_COLOR_BLUE,
    METRO_ACCENT_COLOR_BROWN,
    METRO_ACCENT_COLOR_GREEN,
    METRO_ACCENT_COLOR_LIME,
    METRO_ACCENT_COLOR_MAGENTA,
    METRO_ACCENT_COLOR_MANGO,
    METRO_ACCENT_COLOR_PINK,
    METRO_ACCENT_COLOR_PURPLE,
    METRO_ACCENT_COLOR_RED,
    METRO_ACCENT_COLOR_TEAL,
};

static enum metro_theme_kind current_theme = METRO_THEME_DEFAULT;
static enum metro_accent current_accent = METRO_ACCENT_DEFAULT;

void metro_theme_init(void)
{
    current_theme = METRO_THEME_DEFAULT;
    current_accent = METRO_ACCENT_DEFAULT;
}

enum metro_theme_kind metro_theme_get(void)
{
    return current_theme;
}

void metro_theme_set(enum metro_theme_kind theme)
{
    if (theme == METRO_THEME_DARK || theme == METRO_THEME_LIGHT)
        current_theme = theme;
}

enum metro_accent metro_accent_get(void)
{
    return current_accent;
}

void metro_accent_set(enum metro_accent accent)
{
    if ((unsigned)accent < METRO_ACCENT_COUNT)
        current_accent = accent;
}

unsigned metro_color_bg(void)
{
    return current_theme == METRO_THEME_LIGHT ? METRO_LIGHT_BG : METRO_DARK_BG;
}

unsigned metro_color_fg(void)
{
    return current_theme == METRO_THEME_LIGHT ? METRO_LIGHT_FG : METRO_DARK_FG;
}

unsigned metro_color_secondary(void)
{
    return current_theme == METRO_THEME_LIGHT
               ? METRO_LIGHT_SECONDARY : METRO_DARK_SECONDARY;
}

unsigned metro_color_tertiary(void)
{
    return current_theme == METRO_THEME_LIGHT
               ? METRO_LIGHT_TERTIARY : METRO_DARK_TERTIARY;
}

unsigned metro_color_accent(void)
{
    return accent_colors[current_accent];
}

unsigned metro_color_accent_dim(void)
{
    /* 55 % hacia el fondo: suficiente para que el marco de acento puro
     * se despegue del relleno, y no tanto como para que el tile deje de
     * leerse como "del color del tema". El 45 % restante es lo que
     * mantiene legible la inicial en blanco/negro encima. */
    return metro_fb_blend_color(metro_color_accent(), metro_color_bg(),
                                 55 * 256 / 100);
}

unsigned metro_accent_color(enum metro_accent accent)
{
    if ((unsigned)accent >= METRO_ACCENT_COUNT)
        return accent_colors[METRO_ACCENT_DEFAULT];
    return accent_colors[accent];
}

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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "metro_shared_settings.h"

void metro_shared_settings_defaults(metro_shared_settings_t *s)
{
    memset(s, 0, sizeof(*s));
    s->rev = 0;
    s->updated_by = METRO_SHARED_BY_METRO;
    s->screen_lock_enabled = false;
    s->screen_lock_pin[0] = '\0';
    s->screen_lock_require = METRO_LOCK_REQUIRE_HOLD;
    s->keyclick = false;
    s->replaygain = METRO_SHARED_RG_OFF;
    strcpy(s->language, "es"); /* buffer de 3, "es" cabe siempre */
    s->appearance = METRO_THEME_DARK;
}

bool metro_shared_settings_is_header(const char *line)
{
    return strcmp(line, METRO_SHARED_SETTINGS_HEADER) == 0;
}

static bool is_4_digit_pin(const char *v)
{
    int i;

    if (v[0] == '\0')
        return true; /* vacio = sin clave, valido */
    for (i = 0; i < 4; i++)
        if (v[i] < '0' || v[i] > '9')
            return false;
    return v[4] == '\0';
}

bool metro_shared_settings_parse_field(metro_shared_settings_t *s,
                                        const char *name, const char *value)
{
    if (!strcmp(name, "rev"))
    {
        int v = atoi(value);
        if (v >= 1)
            s->rev = v;
        return true;
    }
    if (!strcmp(name, "updated_by"))
    {
        if (!strcmp(value, "aura"))         s->updated_by = METRO_SHARED_BY_AURA;
        else if (!strcmp(value, "metro"))   s->updated_by = METRO_SHARED_BY_METRO;
        else if (!strcmp(value, "moonlit")) s->updated_by = METRO_SHARED_BY_MOONLIT;
        /* un cuarto nombre no rompe nada mas: se ignora, s->updated_by
         * se queda como estaba (A.2.2). */
        return true;
    }
    if (!strcmp(name, "screen_lock_enabled"))
    {
        s->screen_lock_enabled = (atoi(value) != 0);
        return true;
    }
    if (!strcmp(name, "screen_lock_pin"))
    {
        if (is_4_digit_pin(value))
            strcpy(s->screen_lock_pin, value); /* is_4_digit_pin ya limito a <=4 */
        return true;
    }
    if (!strcmp(name, "screen_lock_require"))
    {
        if (!strcmp(value, "hold"))       s->screen_lock_require = METRO_LOCK_REQUIRE_HOLD;
        else if (!strcmp(value, "1min"))  s->screen_lock_require = METRO_LOCK_REQUIRE_1MIN;
        else if (!strcmp(value, "5min"))  s->screen_lock_require = METRO_LOCK_REQUIRE_5MIN;
        else if (!strcmp(value, "boot"))  s->screen_lock_require = METRO_LOCK_REQUIRE_BOOT;
        return true;
    }
    if (!strcmp(name, "brightness"))
    {
        int v = atoi(value);
        if (v > 0 && v <= METRO_SHARED_BRIGHTNESS_SANE_MAX)
            s->brightness = v;
        return true;
    }
    if (!strcmp(name, "backlight_timeout"))
    {
        int v = atoi(value);
        if (v >= -1 && v <= METRO_SHARED_BACKLIGHT_SANE_MAX)
            s->backlight_timeout = v;
        return true;
    }
    if (!strcmp(name, "idle_poweroff"))
    {
        int v = atoi(value);
        if (v >= 0 && v <= METRO_SHARED_POWEROFF_SANE_MAX)
            s->idle_poweroff = v;
        return true;
    }
    if (!strcmp(name, "keyclick"))
    {
        s->keyclick = (atoi(value) != 0);
        return true;
    }
    if (!strcmp(name, "volume_limit"))
    {
        int v = atoi(value);
        if (v >= METRO_SHARED_VOLUME_LIMIT_SANE_MIN && v <= METRO_SHARED_VOLUME_LIMIT_SANE_MAX)
            s->volume_limit = v;
        return true;
    }
    if (!strcmp(name, "replaygain"))
    {
        if (!strcmp(value, "off"))        s->replaygain = METRO_SHARED_RG_OFF;
        else if (!strcmp(value, "track")) s->replaygain = METRO_SHARED_RG_TRACK;
        else if (!strcmp(value, "album")) s->replaygain = METRO_SHARED_RG_ALBUM;
        return true;
    }
    if (!strcmp(name, "language"))
    {
        /* Los seis codigos de dos letras del contrato -- solo "es"/"en"
         * tienen tabla propia hoy (metro_lang.h); los otros cuatro se
         * guardan igual (round-trip correcto) pero metro_settings.c
         * los ignora al aplicar hasta la Fase 3 de esta ronda
         * (metro_lang_from_code() devuelve false para ellos). */
        static const char *const codes[] = { "es", "en", "fr", "de", "ru", "it" };
        size_t i;
        for (i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
            if (!strcmp(value, codes[i]))
            {
                strcpy(s->language, codes[i]);
                break;
            }
        return true;
    }
    if (!strcmp(name, "appearance"))
    {
        if (!strcmp(value, "dark"))       s->appearance = METRO_THEME_DARK;
        else if (!strcmp(value, "light")) s->appearance = METRO_THEME_LIGHT;
        return true;
    }
    return false;
}

size_t metro_shared_settings_format_field(const metro_shared_settings_t *s,
                                           int index, char *buf, size_t bufsz)
{
    int n;

    switch (index)
    {
        case 0: n = snprintf(buf, bufsz, "rev: %d\n", s->rev); break;
        case 1:
        {
            const char *by = s->updated_by == METRO_SHARED_BY_AURA ? "aura"
                            : s->updated_by == METRO_SHARED_BY_MOONLIT ? "moonlit"
                            : "metro";
            n = snprintf(buf, bufsz, "updated_by: %s\n", by);
            break;
        }
        case 2:
            n = snprintf(buf, bufsz, "screen_lock_enabled: %d\n",
                         s->screen_lock_enabled ? 1 : 0);
            break;
        case 3:
            /* A.1: el PIN y "cuando pedirlo" solo tienen sentido -- y
             * solo se escriben -- con el candado puesto, mismo criterio
             * que metro_settings_save() ya aplica a aura.cfg (M-068). */
            if (!s->screen_lock_enabled)
                return 0;
            n = snprintf(buf, bufsz, "screen_lock_pin: %s\n", s->screen_lock_pin);
            break;
        case 4:
        {
            static const char *const req[] = { "hold", "1min", "5min", "boot" };
            if (!s->screen_lock_enabled)
                return 0;
            n = snprintf(buf, bufsz, "screen_lock_require: %s\n",
                         req[s->screen_lock_require]);
            break;
        }
        case 5: n = snprintf(buf, bufsz, "brightness: %d\n", s->brightness); break;
        case 6: n = snprintf(buf, bufsz, "backlight_timeout: %d\n", s->backlight_timeout); break;
        case 7: n = snprintf(buf, bufsz, "idle_poweroff: %d\n", s->idle_poweroff); break;
        case 8: n = snprintf(buf, bufsz, "keyclick: %d\n", s->keyclick ? 1 : 0); break;
        case 9: n = snprintf(buf, bufsz, "volume_limit: %d\n", s->volume_limit); break;
        case 10:
        {
            const char *rg = s->replaygain == METRO_SHARED_RG_TRACK ? "track"
                            : s->replaygain == METRO_SHARED_RG_ALBUM ? "album"
                            : "off";
            n = snprintf(buf, bufsz, "replaygain: %s\n", rg);
            break;
        }
        case 11: n = snprintf(buf, bufsz, "language: %s\n", s->language); break;
        case 12:
            n = snprintf(buf, bufsz, "appearance: %s\n",
                         s->appearance == METRO_THEME_LIGHT ? "light" : "dark");
            break;
        default:
            return 0;
    }
    return (n > 0 && (size_t)n < bufsz) ? (size_t)n : 0;
}

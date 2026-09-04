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
/* M-106: ver metro_marquee.h. El calculo del ciclo vive aparte, en
 * metro_marquee_cycle.c, para que el arnes de host lo pueda enlazar
 * solo -- mismo patron que metro_master_art_format.c respecto de la
 * cache maestra (M-097). Portado de moonlit.aura (D-067, solo
 * lectura). */
#include <string.h>

#include "lcd.h"
#include "kernel.h"        /* current_tick, HZ */
#include "backlight.h"     /* lcd_active() */
#include "string-extra.h"  /* strlcpy() */

#include "metro_marquee.h"
#include "metro_draw.h"
#include "metro_settings.h" /* metro_settings.animations */

/* --- estado por ranura -------------------------------------------------- */

/* Cuanto texto se guarda para detectar que la ranura cambio de
 * contenido. No hace falta la cadena entera: dos titulos distintos que
 * coincidan en los primeros 48 bytes Y midan lo mismo son
 * indistinguibles para el ojo a efectos del reinicio del ciclo. */
#define MARQUEE_KEY_LEN 48

struct marquee_state {
    char key[MARQUEE_KEY_LEN];
    long since;      /* current_tick en que arranco el ciclo */
    bool scrolling;  /* quedo desplazando en el ultimo dibujo */
};

static struct marquee_state s_slots[METRO_MARQUEE_COUNT];

void metro_marquee_reset(void)
{
    memset(s_slots, 0, sizeof(s_slots));
}

bool metro_marquee_wants_ticks(void)
{
    int i;

    for (i = 0; i < METRO_MARQUEE_COUNT; i++)
        if (s_slots[i].scrolling)
            return true;
    return false;
}

bool metro_marquee_draw(enum metro_marquee_slot slot,
                          enum metro_font_role role, int clip_x, int clip_w,
                          int y, const char *text, unsigned color)
{
    struct marquee_state *st;
    int text_w, span, offset;
    long elapsed_ms;

    /* Un solo comparado sin signo cubre los dos extremos: gcc compila
     * el enum como unsigned en ARM y `slot < 0` seria siempre falso
     * (-Wtype-limits). */
    if ((unsigned)slot >= (unsigned)METRO_MARQUEE_COUNT || !text || clip_w <= 0)
        return false;

    st = &s_slots[slot];

    /* Texto nuevo en esta ranura: el ciclo empieza otra vez por el
     * tramo quieto. Es lo que se espera al mover la seleccion -- que la
     * fila nueva se pueda leer antes de empezar a moverse. */
    if (strncmp(st->key, text, MARQUEE_KEY_LEN - 1) != 0)
    {
        strlcpy(st->key, text, sizeof(st->key));
        st->since = current_tick;
    }

    /* Se mide con la MISMA fuente con la que se dibuja: si la medida y
     * el dibujo usaran roles distintos, la marquesina arrancaria (o no)
     * por unos pixeles de diferencia. */
    text_w = metro_draw_text_width(role, text);

    if (text_w <= clip_w || !lcd_active() ||
        metro_settings.animations == METRO_ANIM_OFF)
    {
        st->scrolling = false;
        metro_draw_text_clipped(role, clip_x, clip_w, clip_x, y, text, color);
        return false;
    }

    span = text_w + METRO_MARQUEE_LOOP_GAP_PX;
    elapsed_ms = (current_tick - st->since) * 1000L / HZ;
    offset = metro_marquee_offset_px(elapsed_ms, span,
                                        METRO_MARQUEE_STATIC_MS,
                                        METRO_MARQUEE_SCROLL_MS);

    /* Dos copias separadas por el hueco: la segunda entra por la
     * derecha justo cuando la primera termina de salir por la
     * izquierda, asi que el bucle no tiene costura. Ambas van por
     * metro_draw_text_clipped(), que recorta con su propio viewport --
     * nada se dibuja fuera de la banda. */
    metro_draw_text_clipped(role, clip_x, clip_w, clip_x - offset, y, text, color);
    metro_draw_text_clipped(role, clip_x, clip_w, clip_x - offset + span, y,
                             text, color);

    st->scrolling = true;
    return true;
}

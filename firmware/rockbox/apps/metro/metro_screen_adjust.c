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
#include <stddef.h>

#include "lcd.h"
#include "kernel.h"
#include "queue.h"
#include "misc.h" /* default_event_handler() */

#include "metro_screen_adjust.h"
#include "metro_draw.h"
#include "metro_fonts.h"
#include "metro_theme.h"
#include "metro_lang.h"
#include "metro_input.h"

/* Anatomia de pagina de Metro (PLAN_MAESTRO.md S1.4): ceja arriba,
 * titulo grande a la altura del encabezado de pivots, y el contenido
 * desde y=84 como cualquier lista. La barra ocupa el ancho util
 * (LEFT_X a ambos lados) para que un paso de 1/10 sea visible. */
#define ADJUST_TITLE_Y  28
#define ADJUST_VALUE_Y  100
#define ADJUST_BAR_Y    148
#define ADJUST_BAR_H    10

static void draw(const struct metro_adjust_spec *spec, int step)
{
    int pct;

    /* Un control de N pasos: el primero NO es 0 % (una barra vacia
     * sugiere "apagado", y el paso 1 de brillo no apaga nada) y el
     * ultimo es 100 %. */
    pct = (step + 1) * 100 / spec->steps;

    metro_draw_clear();
    metro_draw_header(metro_lang_str(LANG_HUB_SETTINGS));
    metro_draw_text(MFONT_DISPLAY, METRO_DRAW_LEFT_X, ADJUST_TITLE_Y,
                     metro_lang_str(spec->title), metro_color_fg());
    metro_draw_text(MFONT_TITLE, METRO_DRAW_LEFT_X, ADJUST_VALUE_Y,
                     spec->label(spec->ctx, step), metro_color_accent());
    metro_draw_progress(METRO_DRAW_LEFT_X, ADJUST_BAR_Y,
                         LCD_WIDTH - 2 * METRO_DRAW_LEFT_X, ADJUST_BAR_H, pct);
    lcd_update();
}

int metro_screen_adjust_run(const struct metro_adjust_spec *spec, int start)
{
    int step = start;

    if (spec == NULL || spec->steps < 2)
        return start;
    if (step < 0)
        step = 0;
    if (step >= spec->steps)
        step = spec->steps - 1;

    /* Se aplica ya, antes del primer dibujo: si `start` venia de un
     * valor guardado fuera de la rejilla (un aura.cfg viejo, o el
     * default de Rockbox), lo que se ve y lo que esta activo tienen que
     * coincidir desde el primer cuadro. */
    spec->apply(spec->ctx, step);

    while (1)
    {
        int action, steps = 1;

        draw(spec, step);

        /* MCTX_LIST: la rueda mueve, MENU vuelve. El mismo contexto que
         * cualquier lista, para que el gesto no cambie de significado al
         * entrar aqui. LEFT/RIGHT (torsion de pivots) no existen en esta
         * pantalla y caen a `default`, sin efecto. */
        action = metro_input_next(MCTX_LIST, HZ / 10, &steps);

        if (action & SYS_EVENT)
        {
            default_event_handler(action);
            continue;
        }

        switch (action)
        {
            case MACT_PREV:
            case MACT_NEXT:
            {
                /* UN paso por evento, sin aceleracion: el control tiene
                 * 6 o 10 posiciones, y la aceleracion de la rueda
                 * (pensada para listas de cientos de filas) haria
                 * imposible pararse en una. `steps` se ignora a
                 * proposito. */
                int next = step + (action == MACT_PREV ? -1 : 1);

                if (next < 0)
                    next = 0;
                if (next >= spec->steps)
                    next = spec->steps - 1;
                if (next != step)
                {
                    step = next;
                    /* En vivo: el punto de la pantalla es VER el
                     * cambio, no confirmarlo al salir. */
                    spec->apply(spec->ctx, step);
                }
                break;
            }
            case MACT_BACK:
            case MACT_HOME:
                return step;
            default:
                break;
        }
    }
}

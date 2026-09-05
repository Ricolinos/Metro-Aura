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
#include <stddef.h>

#include "lcd.h"
#include "kernel.h"
#include "queue.h"
#include "misc.h" /* default_event_handler() */

#include "metro_screen_text.h"
#include "metro_draw.h"
#include "metro_fonts.h"
#include "metro_theme.h"
#include "metro_lang.h"
#include "metro_input.h"

#define TEXT_TITLE_Y   28
#define TEXT_FIRST_Y   84
#define TEXT_LINE_GAP  4

/* Tope de lineas ajustadas. El aviso legal actual son ~20; 96 deja
 * espacio de sobra para una traduccion mas larga sin que el corte sea
 * silencioso (si se llenara, el texto simplemente termina ahi -- pero
 * el aviso legal cabe con el triple de margen). Estatico, no en la
 * pila: 96 * 64 = 6 KB, muy por encima del tope de marco que vigila
 * firmware/tools/stack_report.py (M-101). */
#define TEXT_MAX_LINES 96
#define TEXT_LINE_LEN  64

static char s_lines[TEXT_MAX_LINES][TEXT_LINE_LEN];

/* Corta `body` en lineas que quepan en `max_w`, respetando los '\n'
 * duros y cortando el resto en espacios. Devuelve cuantas escribio.
 *
 * Una palabra mas larga que el ancho util (una URL, por ejemplo) se
 * deja entera y el LCD la recorta al borde: partirla a la mitad haria
 * ilegible justo lo que hay que poder leer y copiar. */
static int wrap(const char *body, int max_w)
{
    int n = 0;
    const char *p = body;

    /* M-117: ya no hay lcd_setfont() aquí -- metro_draw_text_size()
     * elige la fuente por tramo en cada medida, que es justo lo que un
     * `lcd_setfont()` único al entrar no podía hacer. */
    while (*p && n < TEXT_MAX_LINES)
    {
        const char *nl = strchr(p, '\n');
        size_t para_len = nl ? (size_t)(nl - p) : strlen(p);
        size_t off = 0;

        if (para_len == 0) /* linea en blanco entre parrafos */
        {
            s_lines[n][0] = '\0';
            n++;
        }

        while (off < para_len && n < TEXT_MAX_LINES)
        {
            size_t rest = para_len - off;
            size_t take = rest < TEXT_LINE_LEN - 1 ? rest : TEXT_LINE_LEN - 1;
            size_t cut = 0;
            size_t i;
            int w;

            /* La mayor cantidad de palabras completas que entra. */
            for (i = 0; i <= take; i++)
            {
                if (i == take || p[off + i] == ' ')
                {
                    memcpy(s_lines[n], p + off, i);
                    s_lines[n][i] = '\0';
                    metro_draw_text_size(MFONT_LIST, s_lines[n], &w, NULL); /* M-117 */
                    if (w > max_w)
                        break;
                    cut = i;
                }
            }
            if (cut == 0) /* ni la primera palabra entra: va entera */
                cut = take;

            memcpy(s_lines[n], p + off, cut);
            s_lines[n][cut] = '\0';
            n++;

            off += cut;
            while (off < para_len && p[off] == ' ')
                off++;
        }

        if (!nl)
            break;
        p = nl + 1;
    }
    return n;
}

void metro_screen_text_show(const char *title, const char *body)
{
    int total, line_h, visible, first = 0;
    int max_w = LCD_WIDTH - 2 * METRO_DRAW_LEFT_X;

    if (!body || !body[0])
        return;

    total = wrap(body, max_w);
    lcd_setfont(metro_font_id(MFONT_LIST));
    lcd_getstringsize((const unsigned char *)"Ag", NULL, &line_h);
    line_h += TEXT_LINE_GAP;
    visible = (LCD_HEIGHT - TEXT_FIRST_Y) / line_h;
    if (visible < 1)
        visible = 1;

    while (1)
    {
        int action, steps = 1, i;

        metro_draw_clear();
        metro_draw_header(metro_lang_str(LANG_HUB_SETTINGS));
        metro_draw_text(MFONT_DISPLAY, METRO_DRAW_LEFT_X, TEXT_TITLE_Y, title,
                         metro_color_fg());
        for (i = 0; i < visible && first + i < total; i++)
            metro_draw_text(MFONT_LIST, METRO_DRAW_LEFT_X,
                             TEXT_FIRST_Y + i * line_h,
                             s_lines[first + i], metro_color_secondary());
        lcd_update();

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
                /* Aqui SI se respeta la aceleracion de la rueda: es un
                 * texto largo que se recorre, no un control de pocas
                 * posiciones (al reves que metro_screen_adjust.c). */
                int next = first + (action == MACT_PREV ? -steps : steps);
                int last = total - visible;

                if (last < 0)
                    last = 0;
                if (next < 0)
                    next = 0;
                if (next > last)
                    next = last;
                first = next;
                break;
            }
            case MACT_BACK:
            case MACT_HOME:
                return;
            default:
                break;
        }
    }
}

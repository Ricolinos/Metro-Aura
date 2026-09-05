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
#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "misc.h"
#include "lcd.h"

#include "metro_widgets.h"
#include "metro_icons.h"
#include "metro_draw.h"
#include "metro_theme.h"
#include "metro_input.h"
#include "metro_lang.h"
#include "metro_fb.h"
#include "metro_fonts.h"

#define CONFIRM_QUESTION_X 12
#define CONFIRM_QUESTION_Y 90
#define CONFIRM_YES_Y      150
/* M-120: separacion entre "si" y "no" -- era el 178 literal del
 * llamador, que dejo de servir cuando "si" puede bajar. */
#define CONFIRM_ANSWER_PITCH 28

/* M-093: the question used to be one line at MFONT_TITLE, which held
 * "¿cambiar a Aura y reiniciar?" but not "¿cambiar a moonlit.aura y
 * reiniciar?". When it does not fit, break it at the last space that
 * does and draw two lines, centred on the single-line baseline so the
 * block still ends above "sí"/"no". Never more than two lines: every
 * question in the catalogue fits in two at 320 px, and a third would
 * run into the answers. */
/* M-120: devuelve la Y donde TERMINA el bloque de la pregunta. El
 * detalle arrancaba en una CONFIRM_DETAIL_Y fija calculada para una
 * pregunta de UNA linea; en ruso "обновить библиотеку сейчас?" ocupa
 * dos y el detalle se le encimaba (visible desde antes de M-118, no es
 * efecto del dibujo por tramos). */
static int draw_question(const char *question)
{
    static char head[96];
    int w, h, max_w = LCD_WIDTH - 2 * CONFIRM_QUESTION_X;
    const char *tail;
    size_t cut;

    metro_draw_text_size(MFONT_TITLE, question, &w, &h); /* M-117 */
    if (w <= max_w)
    {
        metro_draw_text(MFONT_TITLE, CONFIRM_QUESTION_X, CONFIRM_QUESTION_Y,
                         question, metro_color_fg());
        return CONFIRM_QUESTION_Y + h;
    }

    /* Longest head ending at a space that fits. */
    cut = 0;
    for (tail = question; (tail = strchr(tail, ' ')) != NULL; tail++)
    {
        size_t n = (size_t)(tail - question);
        if (n >= sizeof(head))
            break;
        memcpy(head, question, n);
        head[n] = '\0';
        metro_draw_text_size(MFONT_TITLE, head, &w, NULL); /* M-117 */
        if (w > max_w)
            break;
        cut = n;
    }
    if (cut == 0)
    {
        /* No usable space: let the LCD clip it, as before. */
        metro_draw_text(MFONT_TITLE, CONFIRM_QUESTION_X, CONFIRM_QUESTION_Y,
                         question, metro_color_fg());
        return CONFIRM_QUESTION_Y + h;
    }
    memcpy(head, question, cut);
    head[cut] = '\0';
    metro_draw_text(MFONT_TITLE, CONFIRM_QUESTION_X, CONFIRM_QUESTION_Y - h / 2,
                     head, metro_color_fg());
    metro_draw_text(MFONT_TITLE, CONFIRM_QUESTION_X, CONFIRM_QUESTION_Y + h / 2,
                     question + cut + 1, metro_color_fg());
    return CONFIRM_QUESTION_Y + h / 2 + h;
}

/* M-100: linea de detalle bajo la pregunta, en caption. draw_question()
 * esta topada en DOS lineas a proposito (una tercera choca con "sí"/
 * "no"), asi que una advertencia larga -- "puede tardar varios minutos,
 * segun cuantos archivos tengas y como este el disco" -- no cabe ahi sin
 * mutilarla. Va debajo, en tipografia menor: la jerarquia habitual de
 * Metro (titulo grande + caption), no una pregunta gigante. Se envuelve
 * a lo ancho por palabras y se topa en dos lineas propias, que es lo
 * que entra entre la pregunta y las respuestas. */
/* M-120: separacion vertical entre bloques. Con MFONT_TITLE a 28 px una
 * pregunta de una sola linea termina en 90+28 = 118, asi que 6 px dejan
 * el detalle en la misma Y=124 de antes -- la relacion pregunta/detalle
 * no se mueve en ningun idioma.
 *
 * Lo que SI se mueve, y hacia abajo, son las respuestas cuando hay
 * detalle de dos lineas: el encimado no era solo ruso. En espanol,
 * "como este el disco." terminaba justo encima de "si" (comparar
 * m120-dialogo-es-before/after.png). Un dialogo SIN detalle -- la
 * mayoria -- se ve exactamente igual que en v0.7.2. */
#define CONFIRM_BLOCK_GAP     6
#define CONFIRM_DETAIL_LINES  2

static int draw_detail(const char *detail, int y)
{
    static char line[128];
    int max_w = LCD_WIDTH - 2 * CONFIRM_QUESTION_X;
    const char *p = detail;
    int drawn = 0;
    int lh;

    lcd_setfont(metro_font_id(MFONT_CAPTION));
    lcd_getstringsize((const unsigned char *)"Ag", NULL, &lh);

    while (*p && drawn < CONFIRM_DETAIL_LINES)
    {
        const char *tail;
        size_t cut = 0, len;
        int w;

        /* Si lo que queda entra entero, va entero. Sin esto, la ULTIMA
         * linea se cortaba igual en su ultimo espacio y la palabra final
         * se perdia ("...y como este el" en vez de "...el disco."). */
        len = strlen(p);
        if (len < sizeof(line))
        {
            memcpy(line, p, len);
            line[len] = '\0';
            metro_draw_text_size(MFONT_CAPTION, line, &w, NULL); /* M-117 */
            if (w <= max_w)
            {
                metro_draw_text(MFONT_CAPTION, CONFIRM_QUESTION_X, y, line,
                                 metro_color_secondary());
                return y + lh;
            }
        }

        /* La palabra mas larga que entra, cortando en espacios. */
        for (tail = p; (tail = strchr(tail, ' ')) != NULL; tail++)
        {
            size_t n = (size_t)(tail - p);

            if (n >= sizeof(line))
                break;
            memcpy(line, p, n);
            line[n] = '\0';
            metro_draw_text_size(MFONT_CAPTION, line, &w, NULL); /* M-117 */
            if (w > max_w)
                break;
            cut = n;
        }
        if (cut == 0)
        {
            /* Ni una palabra entera entra, o es la ultima linea: se
             * dibuja lo que queda y el LCD recorta, como hacia
             * draw_question() en su mismo caso. */
            len = strlen(p);
            if (len >= sizeof(line))
                len = sizeof(line) - 1;
            memcpy(line, p, len);
            line[len] = '\0';
            metro_draw_text(MFONT_CAPTION, CONFIRM_QUESTION_X, y, line,
                             metro_color_secondary());
            return y + lh;
        }
        memcpy(line, p, cut);
        line[cut] = '\0';
        metro_draw_text(MFONT_CAPTION, CONFIRM_QUESTION_X, y, line,
                         metro_color_secondary());
        p += cut + 1;
        y += lh;
        drawn++;
    }
    return y;
}

bool metro_widgets_confirm(const char *title, const char *question)
{
    return metro_widgets_confirm_detail(title, question, NULL);
}

bool metro_widgets_confirm_detail(const char *title, const char *question,
                                   const char *detail)
{
    bool sel_yes = false; /* default to "no" -- the safe answer */

    while (1)
    {
        int action;
        int bottom, yes_y;

        metro_draw_clear();
        metro_draw_header(title);
        /* M-120: los tres bloques se encadenan de arriba abajo en vez de
         * vivir en Y fijas. Mover solo el detalle no bastaba -- bajado
         * lo justo para no chocar con una pregunta de dos lineas, sus
         * dos lineas propias se comian el "si". Las respuestas ceden
         * hacia abajo lo necesario y NO se mueven cuando no hace falta
         * (max con CONFIRM_YES_Y), que es el caso de los cinco idiomas
         * latinos y de todo dialogo sin detalle. */
        bottom = draw_question(question);
        if (detail)
            bottom = draw_detail(detail, bottom + CONFIRM_BLOCK_GAP);
        yes_y = bottom + CONFIRM_BLOCK_GAP;
        if (yes_y < CONFIRM_YES_Y)
            yes_y = CONFIRM_YES_Y;
        metro_draw_text(sel_yes ? MFONT_LIST_SEL : MFONT_LIST, 12, yes_y,
                         metro_lang_str(LANG_DIALOG_YES),
                         sel_yes ? metro_color_fg() : metro_color_secondary());
        metro_draw_text(!sel_yes ? MFONT_LIST_SEL : MFONT_LIST, 12,
                         yes_y + CONFIRM_ANSWER_PITCH,
                         metro_lang_str(LANG_DIALOG_NO),
                         !sel_yes ? metro_color_fg() : metro_color_secondary());
        lcd_update();

        action = metro_input_next(MCTX_DIALOG, HZ / 10, NULL);

        if (action & SYS_EVENT)
        {
            default_event_handler(action);
            continue;
        }

        switch (action)
        {
            case MACT_PREV:
            case MACT_NEXT:
                sel_yes = !sel_yes;
                break;
            case MACT_SELECT:
                return sel_yes;
            case MACT_BACK:
                return false;
            default:
                break;
        }
    }
}

#define METRO_INDEX_LETTER_SIZE 80

void metro_widgets_draw_index_letter(const char *letter)
{
    int x = (LCD_WIDTH - METRO_INDEX_LETTER_SIZE) / 2;
    int y = (LCD_HEIGHT - METRO_INDEX_LETTER_SIZE) / 2;

    /* R4/FA-5a (M-076): recibe una CADENA, no un char -- la inicial
     * puede ser un carácter UTF-8 de varios bytes. metro_draw_tile()
     * ya toma el primer carácter completo de lo que se le pase. */
    metro_draw_tile(x, y, METRO_INDEX_LETTER_SIZE, letter);
}

#define METRO_EMPTY_TILE_SIZE 96

void metro_widgets_draw_empty_state(const char *message)
{
    int x = (LCD_WIDTH - METRO_EMPTY_TILE_SIZE) / 2;
    int y = 60;
    int w, h;

    metro_draw_tile(x, y, METRO_EMPTY_TILE_SIZE, " ");

    metro_draw_text_size(MFONT_CAPTION, message, &w, &h); /* M-117 */
    metro_draw_text(MFONT_CAPTION, (LCD_WIDTH - w) / 2, y + METRO_EMPTY_TILE_SIZE + 16,
                     message, metro_color_secondary());
}

void metro_widgets_draw_icon(enum metro_icon_id id, int x, int y, unsigned color)
{
    const struct metro_icon *icon;
    int row;

    if ((unsigned)id >= METRO_ICON_COUNT)
        return;

    icon = &metro_icons[id];
    lcd_set_foreground(color);

    for (row = 0; row < METRO_ICON_SIZE; row++)
    {
        unsigned mask = icon->rows[row];
        int col = 0;

        /* Por CORRIDAS horizontales, no pixel por pixel: un icono de
         * 16x16 son hasta 256 lcd_drawpixel() sueltos, y estos glifos
         * son siluetas rellenas donde una fila suele ser una o dos
         * corridas. Mismo criterio que el resto del dibujo de Metro,
         * que usa lcd_fillrect() para todo lo que sea un bloque. */
        while (col < METRO_ICON_SIZE)
        {
            int run;

            if (!(mask & (1u << (METRO_ICON_SIZE - 1 - col))))
            {
                col++;
                continue;
            }
            run = 0;
            while (col + run < METRO_ICON_SIZE &&
                   (mask & (1u << (METRO_ICON_SIZE - 1 - (col + run)))))
                run++;

            lcd_fillrect(x + col, y + row, run, 1);
            col += run;
        }
    }
}

/* Integer sqrt, rounded down. Inputs here are < 2^24 (distances in
 * 8.8 fixed point squared), so the 32-bit loop is enough. */
static unsigned isqrt32(unsigned v)
{
    unsigned res = 0, bit = 1u << 30;

    while (bit > v)
        bit >>= 2;
    while (bit)
    {
        if (v >= res + bit)
        {
            v -= res + bit;
            res = (res >> 1) + bit;
        }
        else
            res >>= 1;
        bit >>= 2;
    }
    return res;
}

void metro_widgets_draw_circle(int cx, int cy, int r, unsigned color)
{
    int dx, dy;
    int r256 = r * 256;

    if (r <= 0)
        return;

    for (dy = -r - 1; dy <= r + 1; dy++)
    {
        for (dx = -r - 1; dx <= r + 1; dx++)
        {
            /* distance in 8.8: sqrt((dx^2+dy^2) * 65536) = d * 256 */
            unsigned d256 = isqrt32((unsigned)(dx * dx + dy * dy) << 16);
            int diff = (int)d256 - r256;
            int alpha;

            if (diff < 0)
                diff = -diff;
            /* ~1.5px ring: full within 0.25px of r, gone at 1.25px. A
             * strict 1px ring spreads over two pixel rows at half
             * intensity and reads grey on the panel; this keeps it thin
             * but solid. */
            alpha = 320 - diff;
            if (alpha > 256)
                alpha = 256;
            if (alpha > 0)
                metro_fb_plot_alpha(cx + dx, cy + dy, color, alpha);
        }
    }
}

void metro_widgets_draw_icon_in_circle(enum metro_icon_id id, int x, int y,
                                        int r, unsigned ring_color,
                                        unsigned glyph_color)
{
    int cx = x + r, cy = y + r;

    metro_widgets_draw_circle(cx, cy, r, ring_color);
    /* The 16px glyph cell centred on the ring's centre; with r=13 that
     * leaves 5px of air between cell and ring, and Fluent's own ~2px
     * internal padding makes the visible ink ~12px. */
    metro_widgets_draw_icon(id, cx - METRO_ICON_SIZE / 2, cy - METRO_ICON_SIZE / 2,
                            glyph_color);
}

void metro_widgets_draw_glyph(const struct metro_glyph *g, int x, int y, unsigned color)
{
    int row, col;

    for (row = 0; row < g->height; row++)
        for (col = 0; col < g->width; col++)
        {
            int a = g->alpha[row * g->width + col];
            if (a)
                metro_fb_plot_alpha(x + col, y + row, color, a * 256 / 255);
        }
}

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
#include <stdbool.h>
#include "lcd.h"
#include "viewport.h"
#include "powermgmt.h"
#include "timefuncs.h"
#include "button.h" /* M-104: button_hold() */

#include "metro_draw.h"
#include "metro_widgets.h"
#include "audio.h"
#include "metro_theme.h"
#include "metro_lang.h"
#include "metro_marquee.h" /* M-106 */

#define METRO_HEADER_HEIGHT 24

void metro_draw_clear(void)
{
    lcd_set_background(metro_color_bg());
    lcd_clear_display();
}

/* R2-F1/DD-1: every apps/metro/ glyph draws under DRMODE_FG (transparent
 * against whatever is already on screen) instead of Rockbox's default
 * DRMODE_SOLID (opaque per-glyph background box) -- SOLID was painting
 * black plates over Now Playing's cover art behind every text line.
 * Left set to FG afterward; nothing in apps/metro/ needs a SOLID
 * rectangle on purpose, so there is no restore step. See DECISIONS.md
 * M-051. */
int metro_draw_text_width(enum metro_font_role role, const char *str)
{
    int w = 0, h;

    if (!str)
        return 0;
    lcd_setfont(metro_font_id(role));
    lcd_getstringsize((const unsigned char *)str, &w, &h);
    return w;
}

void metro_draw_text(enum metro_font_role role, int x, int y,
                      const char *str, unsigned color)
{
    lcd_setfont(metro_font_id(role));
    lcd_set_foreground(color);
    lcd_set_drawmode(DRMODE_FG);
    lcd_putsxy(x, y, (const unsigned char *)str);
}

void metro_draw_text_cut_right(enum metro_font_role role, int x, int y,
                                const char *str, unsigned color, int clip_w)
{
    metro_draw_text_clipped(role, x, clip_w, x, y, str, color);
}

void metro_draw_text_clipped(enum metro_font_role role, int clip_x, int clip_w,
                              int x, int y, const char *str, unsigned color)
{
    struct viewport vp;
    struct viewport *old_vp;

    if (clip_w <= 0)
        return;

    /* viewport_set_defaults() -- NOT viewport_set_fullscreen() directly.
     * Both end up in lcd_init_viewport(), which READS vp->buffer before
     * anything sets it: if it's non-NULL it is dereferenced as a
     * struct frame_buffer_t* and its stride/data/get_address_fn fields
     * are read and possibly written through. With a stack viewport that
     * is whatever garbage was on the stack -- undefined behaviour that
     * in practice corrupted the LCD state and made later
     * screen_dump() calls (FBADDR() -> buffer->get_address_fn) jump
     * into random code. viewport_set_defaults() zeroes vp->buffer first,
     * which is why every core caller uses it. See DECISIONS.md M-027.
     *
     * F11: vp.buffer is overwritten right after with whatever buffer
     * lcd_current_viewport is ACTUALLY drawing into -- NULL (real
     * screen) normally, but an offscreen metro_fb.c buffer while
     * metro_transitions.c is pre-rendering a destination frame. Left
     * at viewport_set_defaults()'s NULL, this function always drew
     * into the real LCD regardless -- row titles never made it into
     * an offscreen "to" frame, so a completed SLIDE composited a
     * blank row area (found visually: rows present with
     * animations=off, gone with animations=all). Still well-defined
     * either way -- never the M-027 stack-garbage case, just a
     * pointer copy of an already-valid buffer. */
    viewport_set_defaults(&vp, SCREEN_MAIN);
    vp.buffer = lcd_current_viewport->buffer;
    vp.x = clip_x;
    vp.width = clip_w;
    vp.font = metro_font_id(role);
    vp.fg_pattern = color;
    vp.bg_pattern = metro_color_bg();
    vp.drawmode = DRMODE_FG; /* M-051 -- see metro_draw_text() */

    old_vp = lcd_set_viewport(&vp);
    lcd_putsxy(x - clip_x, y - vp.y, (const unsigned char *)str);
    lcd_set_viewport(old_vp);
}

/* F10: real geometric icon (rect body + nub, proportional fill)
 * replacing the "N%" text (M-018's deferred placeholder). Body is
 * outlined regardless of charge level; a negative battery_level()
 * (charge unknown, e.g. running off USB power in the sim) just draws
 * the empty outline with no fill instead of Rockbox's own "--%". */
/* R5-F4 (M-084): eje de la barra de estado -- ver metro_draw_header(). */
#define METRO_HEADER_TEXT_Y     4
#define METRO_HEADER_ICON_Y     3
#define METRO_HEADER_BATTERY_Y  7

#define METRO_BATTERY_W     18
#define METRO_BATTERY_H     9
#define METRO_BATTERY_NUB_W 2
#define METRO_BATTERY_NUB_H 4

void metro_draw_battery(int x_right, int y)
{
    int level = battery_level();
    int body_x = x_right - METRO_BATTERY_NUB_W - METRO_BATTERY_W;
    int nub_x = x_right - METRO_BATTERY_NUB_W;
    int nub_y = y + (METRO_BATTERY_H - METRO_BATTERY_NUB_H) / 2;
    int fill_w;

    lcd_set_foreground(metro_color_secondary());
    lcd_drawrect(body_x, y, METRO_BATTERY_W, METRO_BATTERY_H);
    lcd_fillrect(nub_x, nub_y, METRO_BATTERY_NUB_W, METRO_BATTERY_NUB_H);

    if (level > 100)
        level = 100; /* battery_level() is documented as percent, but
                        its prototype can't tell the compiler that. */
    if (level > 0)
    {
        fill_w = (METRO_BATTERY_W - 4) * level / 100;
        if (fill_w > 0)
            lcd_fillrect(body_x + 2, y + 2, fill_w, METRO_BATTERY_H - 4);
    }
}

void metro_draw_header(const char *page_title)
{
    struct tm *now = get_time();
    char timebuf[8];
    int w, h;

    /* R5-F4 (M-084): todo lo de la barra comparte UN eje horizontal, el
     * centro vertical de los dígitos del reloj. La caption de 14px
     * dibujada en y=4 pone su caja de dígitos en y=7..15 (centro 11);
     * la batería (9px) va en y=7 para ocupar exactamente esas filas, y
     * el glifo de transporte (16px de celda, ~12px de tinta a partir de
     * la fila 2 en los Fluent) en y=3 para que su tinta (5..16) quede
     * centrada ahí mismo. Antes la batería iba en y=4 y flotaba ~2.5px
     * por encima del texto. */
    int clock_x = LCD_WIDTH - 40;
    int status = audio_status();
    int transport_x;

    metro_draw_text(MFONT_CAPTION, METRO_DRAW_LEFT_X, METRO_HEADER_TEXT_Y, page_title,
                     metro_color_secondary());

    if (now != NULL)
    {
        lcd_setfont(metro_font_id(MFONT_CAPTION));
        snprintf(timebuf, sizeof(timebuf), "%02d:%02d", now->tm_hour, now->tm_min);
        lcd_getstringsize((const unsigned char *)timebuf, &w, &h);
        clock_x = LCD_WIDTH - 40 - w;
        metro_draw_text(MFONT_CAPTION, clock_x, METRO_HEADER_TEXT_Y, timebuf,
                         metro_color_secondary());
    }

    transport_x = clock_x - 6 - METRO_ICON_SIZE;

    /* R5-F4 (M-084): hay música (sonando o en pausa) -> glifo a la
     * izquierda del reloj, misma asimetría de color de M-073: play en
     * secundario (lo normal no grita), pausa en acento (es lo que uno
     * busca con la mirada cuando no se oye nada). Sin audio, nada. */
    if (status & AUDIO_STATUS_PAUSE)
    {
        metro_widgets_draw_icon(METRO_ICON_PAUSE, transport_x,
                                METRO_HEADER_ICON_Y, metro_color_accent());
        transport_x -= 6 + METRO_ICON_SIZE;
    }
    else if (status & AUDIO_STATUS_PLAY)
    {
        metro_widgets_draw_icon(METRO_ICON_PLAY, transport_x,
                                METRO_HEADER_ICON_Y, metro_color_secondary());
        transport_x -= 6 + METRO_ICON_SIZE;
    }

    /* M-104 (plan maestro SS D/SS H): candado a la IZQUIERDA del glifo de
     * transporte mientras el interruptor Hold esta puesto. En
     * secundario, no en acento: es un estado del aparato que se
     * consulta, no algo que reclame atencion -- el acento en esta barra
     * esta reservado para "pausa", que es lo unico que uno busca con la
     * mirada.
     *
     * Se dibuja en TODA pantalla con barra porque todas pasan por aqui.
     * El refresco es por sondeo (metro_screen_lock_poll_hold(), desde el
     * bucle principal): el Hold del 6G no genera eventos de boton. Sin
     * sondeo el icono aparecia solo la proxima vez que algo mas
     * provocara un redibujo. */
    if (button_hold())
        metro_widgets_draw_icon(METRO_ICON_LOCK, transport_x,
                                METRO_HEADER_ICON_Y, metro_color_secondary());

    metro_draw_battery(LCD_WIDTH - 4, METRO_HEADER_BATTERY_Y);
}

#define METRO_PIVOT_Y      28
#define METRO_PIVOT_GAP    24
#define METRO_ROWS_FIRST_Y METRO_DRAW_ROWS_FIRST_Y
#define METRO_ROW_PITCH    METRO_DRAW_ROW_PITCH
#define METRO_ROWS_VISIBLE METRO_DRAW_ROWS_VISIBLE
#define METRO_ROWS_LEFT_X  METRO_DRAW_LEFT_X

void metro_draw_pivots(const struct metro_page *page, int active_pivot,
                        int x_offset)
{
    int i, x = METRO_ROWS_LEFT_X + x_offset;

    lcd_setfont(metro_font_id(MFONT_DISPLAY));

    for (i = active_pivot; i < page->npivots && x < LCD_WIDTH; i++)
    {
        int w, h;
        const char *name = metro_lang_str(page->pivots[i].name);

        lcd_getstringsize((const unsigned char *)name, &w, &h);
        metro_draw_text(MFONT_DISPLAY, x, METRO_PIVOT_Y, name,
                         i == active_pivot ? metro_color_fg()
                                            : metro_color_tertiary());
        x += w + METRO_PIVOT_GAP;
    }
}

void metro_draw_clear_rows_area(void)
{
    lcd_set_foreground(metro_color_bg());
    lcd_fillrect(0, METRO_ROWS_FIRST_Y, LCD_WIDTH, LCD_HEIGHT - METRO_ROWS_FIRST_Y);
}

/* F12: y_offsets[] (one entry per VISIBLE row slot, same indexing as
 * the loop below -- NULL for the plain, un-offset draw
 * metro_draw_rows() still does) is metro_screen_list.c's FEATHER
 * cascade (S3.3): each row's own y nudged down by a shrinking amount
 * as its own entrance animates in, independent of every other row's
 * offset. Never clears first -- metro_screen_list.c owns clearing the
 * row area between frames (metro_draw_clear_rows_area()) since the
 * feather loop redraws far more often than a single metro_draw_rows()
 * call would otherwise need to. */
void metro_draw_rows_ex(const struct metro_pivot *pivot, int first, int sel,
                         int x_offset, const int *y_offsets)
{
    int count = pivot->count(pivot->ctx);
    int i, y = METRO_ROWS_FIRST_Y;
    int x = METRO_ROWS_LEFT_X + x_offset;
    int visible_index = 0;

    /* Draw one row past METRO_ROWS_VISIBLE on purpose -- it peeks,
     * naturally cut by the bottom of the 240px screen (A.6), same
     * "next thing asoma cortado" effect as the pivot header. */
    for (i = first; i < count && i < first + METRO_ROWS_VISIBLE + 1; i++, visible_index++)
    {
        struct metro_row row;
        bool selected = (i == sel);
        int row_y = y + (y_offsets ? y_offsets[visible_index] : 0);

        pivot->get_row(pivot->ctx, i, &row);

        /* F10: clip the title so it can never run into the
         * right-aligned subtitle (long filenames especially --
         * videos/photos, up to METRO_FSUTIL_NAME_LEN bytes). Subtitle
         * width has to be measured first to know where the title's
         * clip boundary is. */
        int title_clip_w = LCD_WIDTH - x;

        if (row.subtitle)
        {
            int sub_w, sub_h;
            lcd_setfont(metro_font_id(MFONT_CAPTION));
            lcd_getstringsize((const unsigned char *)row.subtitle, &sub_w, &sub_h);
            metro_draw_text(MFONT_CAPTION, LCD_WIDTH - 12 - sub_w, row_y + 4,
                             row.subtitle, metro_color_tertiary());
            title_clip_w = LCD_WIDTH - 12 - sub_w - x - 8;
        }

        /* M-106 (plan maestro SS G): solo la fila CON FOCO desplaza. Las
         * demas se cortan a la derecha como siempre -- mover seis
         * textos a la vez seria ilegible, y ademas la fila que el
         * usuario no puede leer entera es la que tiene elegida. */
        if (selected)
            metro_marquee_draw(METRO_MARQUEE_ROW, MFONT_LIST_SEL, x,
                                title_clip_w, row_y, row.title,
                                metro_color_fg());
        else
            metro_draw_text_cut_right(MFONT_LIST, x, row_y, row.title,
                                       metro_color_secondary(), title_clip_w);

        y += METRO_ROW_PITCH;
    }
}

void metro_draw_rows(const struct metro_pivot *pivot, int first, int sel,
                      int x_offset)
{
    metro_draw_rows_ex(pivot, first, sel, x_offset, NULL);
}

void metro_draw_progress(int x, int y, int width, int height, int pct)
{
    int fill_w;

    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    fill_w = width * pct / 100;

    lcd_set_foreground(metro_color_tertiary());
    lcd_fillrect(x, y, width, height);

    if (fill_w > 0)
    {
        lcd_set_foreground(metro_color_accent());
        lcd_fillrect(x, y, fill_w, height);
    }
}

void metro_draw_tile(int x, int y, int size, const char *label)
{
    /* R4/FA-5a (M-076): 5 bytes, no 2 -- la inicial puede ser un
     * carácter UTF-8 de varios bytes ("Álbum", "Ñu"). Cortar por byte
     * entregaba una secuencia partida y un glifo basura. */
    char initial[5] = { ' ', '\0' };
    int w, h;

    metro_lang_initial(label, initial, sizeof(initial));
    if (!initial[0])
        initial[0] = ' ';

    /* M-105 (plan maestro SS F): acento ATENUADO, no puro. El puro queda
     * reservado para el marco de seleccion; con los dos iguales, una
     * cuadricula sin caratulas era un cuadro de acento seleccionado
     * dentro de cuadros de acento y la seleccion se perdia. */
    lcd_set_foreground(metro_color_accent_dim());
    lcd_fillrect(x, y, size, size);

    /* A blank label (metro_widgets_draw_empty_state()'s plain accent
     * square, no letter) must draw nothing here -- the custom
     * MFONT_DISPLAY bitmap font has no real space glyph and falls
     * back to garbage (observed rendering an unrelated glyph) instead
     * of blank pixels. */
    if (initial[0] != ' ')
    {
        lcd_setfont(metro_font_id(MFONT_DISPLAY));
        lcd_getstringsize((const unsigned char *)initial, &w, &h);
        lcd_set_foreground(metro_color_bg());
        lcd_set_drawmode(DRMODE_FG); /* M-051 -- see metro_draw_text() */
        lcd_putsxy(x + (size - w) / 2, y + (size - h) / 2, (const unsigned char *)initial);
    }
}

/* R2-F2/DD-7/DD-8 (M-057): grid counterpart of metro_draw_rows() --
 * see the geometry rationale in metro_draw.h. `first` is always a
 * multiple of METRO_TILE_COLS (metro_nav_move_sel_grid() guarantees
 * this), so slot->index->col/row is a straight linear mapping, no
 * wraparound bookkeeping needed. */
/* R4 (M-080): rótulo del tile seleccionado.
 *
 * Una cuadrícula no tenía NINGÚN texto: metro_draw_tiles() solo usaba
 * el título para la inicial dentro del tile de respaldo. Con carátulas
 * parecidas entre sí -- cuatro álbumes heredando el mismo cover.jpg del
 * directorio padre, que es exactamente lo que pasa con los fixtures --
 * los tiles dejaban de ser distinguibles. Se notó al convertir Álbumes
 * a cuadrícula (FA-5b).
 *
 * Un solo rótulo para lo seleccionado, no uno por tile: no hay espacio
 * vertical para etiquetas individuales (dos filas de 80px arrancando en
 * y=84 ya se salen de los 240 de alto) y además sería ruido -- lo que
 * hace falta saber es qué está elegido.
 *
 * Va en una franja al pie, sobre fondo sólido pintado explícitamente
 * con lcd_fillrect() para que se lea encima de cualquier carátula.
 * (M-051 prohíbe conseguir ese fondo vía DRMODE_SOLID; pintarlo aparte
 * es justo la salida que esa regla contempla.) Tapa los 22px de abajo
 * de la segunda fila, que de todos modos ya venía cortada por el borde
 * de la pantalla -- su función es asomar para decir "hay más", y con 54
 * px sigue haciéndolo.
 *
 * Título a la izquierda y subtítulo a la derecha en terciario: el mismo
 * reparto que metro_draw_rows_ex() ya usa para las filas de texto, de
 * modo que un álbum se lee igual ("Analog Dreams" / "Wheel & Click")
 * esté en lista o en cuadrícula. */
#define METRO_TILE_CAPTION_H 22

static void draw_tile_caption(const struct metro_pivot *pivot, int sel, int count)
{
    struct metro_row row;
    int y = LCD_HEIGHT - METRO_TILE_CAPTION_H;
    int title_clip_w = LCD_WIDTH - 2 * METRO_ROWS_LEFT_X;

    if (sel < 0 || sel >= count)
        return;

    pivot->get_row(pivot->ctx, sel, &row);
    if (!row.title || !row.title[0])
        return;

    lcd_set_foreground(metro_color_bg());
    lcd_fillrect(0, y, LCD_WIDTH, METRO_TILE_CAPTION_H);

    if (row.subtitle && row.subtitle[0])
    {
        int sub_w, sub_h;

        lcd_setfont(metro_font_id(MFONT_CAPTION));
        lcd_getstringsize((const unsigned char *)row.subtitle, &sub_w, &sub_h);
        metro_draw_text(MFONT_CAPTION, LCD_WIDTH - METRO_ROWS_LEFT_X - sub_w,
                         y + 4, row.subtitle, metro_color_tertiary());
        title_clip_w = LCD_WIDTH - METRO_ROWS_LEFT_X - sub_w
                       - METRO_ROWS_LEFT_X - 8;
    }

    /* M-106: el rotulo del tile seleccionado tambien desplaza -- es, por
     * construccion, el unico texto de una cuadricula (R4/M-080). */
    metro_marquee_draw(METRO_MARQUEE_TILE, MFONT_CAPTION, METRO_ROWS_LEFT_X,
                        title_clip_w, y + 4, row.title, metro_color_fg());
}

void metro_draw_tiles(const struct metro_pivot *pivot, int first, int sel,
                       int x_offset)
{
    int count = pivot->count(pivot->ctx);
    int slot;

    for (slot = 0; slot < METRO_TILE_COLS * METRO_TILE_ROWS_VISIBLE; slot++)
    {
        int index = first + slot;
        int col, row, x, y;
        const fb_data *bmp;

        if (index >= count)
            break;

        col = slot % METRO_TILE_COLS;
        row = slot / METRO_TILE_COLS;
        x = x_offset + col * METRO_TILE_SIZE;
        y = METRO_ROWS_FIRST_Y + row * METRO_TILE_SIZE;

        bmp = pivot->get_tile ? pivot->get_tile(pivot->ctx, index) : NULL;
        if (bmp)
            lcd_bitmap(bmp, x, y, METRO_TILE_SIZE, METRO_TILE_SIZE);
        else
        {
            struct metro_row row_info;
            pivot->get_row(pivot->ctx, index, &row_info);
            metro_draw_tile(x, y, METRO_TILE_SIZE, row_info.title);
        }

        if (index == sel)
        {
            /* M-105: marco de acento de 3 px MAS un anillo interior de
             * 1 px del color de fondo. El anillo es lo que separa el
             * marco de la imagen que hay debajo: sin el, una caratula
             * clara contra un acento claro (o una oscura contra el
             * acento en tema oscuro) dejaba el borde sin contraste
             * justo donde tiene que verse. Mismo criterio en las tres
             * familias. */
            int b;

            lcd_set_foreground(metro_color_accent());
            for (b = 0; b < 3; b++)
                lcd_drawrect(x + b, y + b, METRO_TILE_SIZE - 2 * b, METRO_TILE_SIZE - 2 * b);
            lcd_set_foreground(metro_color_bg());
            lcd_drawrect(x + 3, y + 3, METRO_TILE_SIZE - 6, METRO_TILE_SIZE - 6);
        }
    }

    draw_tile_caption(pivot, sel, count);
}

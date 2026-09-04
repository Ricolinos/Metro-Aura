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
#include <string.h>
#include "string-extra.h"
#include "lcd.h"
#include "kernel.h"
#include "button.h"

#include "metro_screen_list.h"
#include "metro_draw.h"
#include "metro_lang.h"
#include "metro_widgets.h"
#include "metro_motion.h"
#include "metro_transitions.h"
#include "metro_music.h" /* R4/FA-8: metro_music_playpause() */

static metro_nav_t s_nav;

/* F10: floating index letter (S1.4) -- shown for 600ms after a fast
 * scroll (steps >= 3) lands on a new row, using that row's own first
 * character. Deliberately without the plan's "count >= 40" gate
 * (DESVIACIONES.md F10-1): none of Metro's real lists have that many
 * items with the test fixtures this session can generate, and a big
 * jump on a shorter list still benefits from the same feedback. */
#define METRO_INDEX_LETTER_TICKS (HZ * 3 / 5)
#define METRO_INDEX_LETTER_MIN_STEPS 3

static long s_index_letter_until = 0;
static char s_index_letter[5] = "";

/* F12: FEATHER (S3.3) -- staggered row entrance after a PUSH, animations=all
 * only. Set on every metro_screen_list_push() and consumed (or just
 * discarded, both are fine -- see the module comment on
 * metro_screen_list_run_feather_if_pending()) by whatever
 * metro_main.c does next; the push into Now Playing's sentinel page
 * sets it too but that path always ends in FADE, never calling the
 * run function below, so it just sits there until the NEXT real push
 * resets it -- never a stale cascade on an unrelated screen. */
#define METRO_FEATHER_FRAMES 6
#define METRO_FEATHER_ROW_OFFSET_PX 8

static bool s_feather_pending = false;

/* page_stack[d-1] holds the page pushed at nav depth d, for d>=2 --
 * index 0 (depth 1, the hub) is never used, metro_screen_hub.c owns
 * its own pivot and only borrows s_nav's depth-1 frame for its
 * selection/windowing state. */
static const struct metro_page *page_stack[METRO_NAV_MAX_DEPTH];

void metro_screen_list_init(void)
{
    metro_nav_init(&s_nav, 1);
}

metro_nav_t *metro_screen_nav(void)
{
    return &s_nav;
}

bool metro_screen_list_push(const struct metro_page *page)
{
    if (!metro_nav_push(&s_nav, page->npivots))
        return false;

    page_stack[metro_nav_depth(&s_nav) - 1] = page;
    s_feather_pending = true;
    return true;
}

bool metro_screen_list_pop(void)
{
    return metro_nav_pop(&s_nav);
}

void metro_screen_list_pop_to_root(void)
{
    metro_nav_pop_to_root(&s_nav);
}

static const struct metro_page *current_page(void)
{
    int depth = metro_nav_depth(&s_nav);

    if (depth < 2)
        return NULL;

    return page_stack[depth - 1];
}

const struct metro_page *metro_screen_list_current_page(void)
{
    return current_page();
}

bool metro_screen_list_has_pending_redraw(void)
{
    return current_tick < s_index_letter_until;
}

void metro_screen_list_show(void)
{
    const struct metro_page *page = current_page();
    const struct metro_pivot *pivot;
    int active;

    if (!page)
        return;

    active = metro_nav_pivot(&s_nav);
    pivot = &page->pivots[active];

    metro_draw_clear();
    metro_draw_header(page->title_dynamic ? page->title_dynamic
                                           : metro_lang_str(page->title));
    metro_draw_pivots(page, active, 0);

    if (pivot->count(pivot->ctx) == 0)
        metro_widgets_draw_empty_state(metro_lang_str(
            pivot->empty_message ? pivot->empty_message : LANG_EMPTY_LIST));
    else if (pivot->tile_cols > 0)
        metro_draw_tiles(pivot, metro_nav_first_visible(&s_nav), metro_nav_sel(&s_nav), 0);
    else
        metro_draw_rows(pivot, metro_nav_first_visible(&s_nav), metro_nav_sel(&s_nav), 0);

    /* R2-F2/DD-7: the floating index letter doesn't apply to grids
     * (also never gets armed for one -- see metro_screen_list_handle()) */
    if (pivot->tile_cols == 0 && current_tick < s_index_letter_until)
        metro_widgets_draw_index_letter(s_index_letter);

    lcd_update();
}

void metro_screen_list_handle(int action, int steps)
{
    const struct metro_page *page = current_page();
    const struct metro_pivot *pivot;
    int count;

    if (!page)
        return;

    pivot = &page->pivots[metro_nav_pivot(&s_nav)];
    count = pivot->count(pivot->ctx);

    switch (action)
    {
        case MACT_PREV:
        case MACT_NEXT:
            /* R2-F2/DD-7: grid pivots move the same linear index one
             * tile at a time (reading order, not a column jump) but
             * window by whole rows of tile_cols -- the floating index
             * letter (F10) doesn't apply here, it's a row-list-only
             * affordance. */
            if (pivot->tile_cols > 0)
            {
                metro_nav_move_sel_grid(&s_nav, action == MACT_PREV ? -steps : steps,
                                         count, pivot->tile_cols, METRO_TILE_ROWS_VISIBLE);
                break;
            }
            metro_nav_move_sel(&s_nav, action == MACT_PREV ? -steps : steps,
                                count, METRO_DRAW_ROWS_VISIBLE);
            if (steps >= METRO_INDEX_LETTER_MIN_STEPS && count > 0)
            {
                struct metro_row row;
                pivot->get_row(pivot->ctx, metro_nav_sel(&s_nav), &row);
                /* R4/FA-5a (M-076): carácter completo, no primer byte. */
                metro_lang_initial(row.title, s_index_letter,
                                    sizeof(s_index_letter));
                if (!s_index_letter[0])
                {
                    s_index_letter[0] = '?';
                    s_index_letter[1] = '\0';
                }
                s_index_letter_until = current_tick + METRO_INDEX_LETTER_TICKS;
            }
            break;
        case MACT_PIVOT_PREV:
            metro_nav_pivot_prev(&s_nav);
            break;
        case MACT_PIVOT_NEXT:
            metro_nav_pivot_next(&s_nav);
            break;
        case MACT_SELECT:
            if (pivot->on_select)
            {
                /* R3-F8/DD-9 (M-069): CONTINUUM. El título de la fila
                 * elegida, y dónde está dibujada AHORA, hay que
                 * capturarlos antes de on_select() -- que puede empujar
                 * una página nueva y dejar `pivot`/la selección
                 * apuntando a otra cosa. */
                char from_title[METRO_CONTINUUM_TITLE_MAX];
                int from_y = 0;
                bool have_from = false;

                if (pivot->tile_cols == 0 && count > 0)
                {
                    struct metro_row row;

                    pivot->get_row(pivot->ctx, metro_nav_sel(&s_nav), &row);
                    if (row.title && row.title[0])
                    {
                        strlcpy(from_title, row.title, sizeof(from_title));
                        from_y = METRO_DRAW_ROWS_FIRST_Y +
                                 (metro_nav_sel(&s_nav) - metro_nav_first_visible(&s_nav)) *
                                     METRO_DRAW_ROW_PITCH;
                        have_from = true;
                    }
                }

                pivot->on_select(pivot->ctx, metro_nav_sel(&s_nav));

                /* La tercera puerta de DD-9: solo si hay continuidad
                 * REAL que mostrar, es decir si la página nueva se
                 * llama igual que la fila que la abrió (su ceja es
                 * title_dynamic). Un "canciones" genérico abierto desde
                 * la fila "Analog Dreams" no coincide con nada y hace
                 * PUSH normal, sin inventar una animación. */
                if (have_from)
                {
                    const struct metro_page *dest = current_page();

                    if (dest && dest != page && dest->title_dynamic &&
                        !strcmp(dest->title_dynamic, from_title))
                        metro_transitions_arm_continuum(from_title, from_y);
                }
            }
            break;
        case MACT_SELECT_HOLD:
            /* M-101: sin CONTINUUM ni push -- un SELECT sostenido no
             * navega, solo deja que el pivot mute su propio estado. */
            if (pivot->on_select_hold)
                pivot->on_select_hold(pivot->ctx, metro_nav_sel(&s_nav));
            break;
        case MACT_BACK:
            metro_screen_list_pop();
            break;
        case MACT_HOME:
            metro_screen_list_pop_to_root();
            break;
        case MACT_PLAYPAUSE:
            metro_music_playpause();
            break;
        default:
            break;
    }
}

/* F12: no-op (just clears the flag) unless a push actually happened
 * and metro_main.c's dispatch decided this is the moment to run it --
 * see the module comment on s_feather_pending. Redraws only the row
 * area (metro_draw_clear_rows_area()) each frame, never the header/
 * pivots above it -- those already settled during the PUSH/turnstile
 * that ran just before this. */
void metro_screen_list_run_feather_if_pending(void)
{
    const struct metro_page *page;
    const struct metro_pivot *pivot;
    int active, count, first, sel, frame;

    if (!s_feather_pending)
        return;
    s_feather_pending = false;

    /* PLAN_MAESTRO.md S3.3: FEATHER only runs under animations=all,
     * "off" under both minimal and off -- more restrictive than
     * PUSH/POP/twist, which still animate (as a plain slide) under
     * minimal. */
    if (!lcd_active() || !metro_transitions_effective_all())
        return;

    page = current_page();
    if (!page)
        return;

    active = metro_nav_pivot(&s_nav);
    pivot = &page->pivots[active];
    count = pivot->count(pivot->ctx);
    if (count == 0)
        return; /* empty-state tile drew instead -- nothing to cascade */
    if (pivot->tile_cols > 0)
        return; /* R2-F2/DD-7: FEATHER is a row-list affordance, not for grids */

    first = metro_nav_first_visible(&s_nav);
    sel = metro_nav_sel(&s_nav);

    for (frame = 0; frame < METRO_FEATHER_FRAMES; frame++)
    {
        int y_offsets[METRO_DRAW_ROWS_VISIBLE + 1];
        int row;

        for (row = 0; row <= METRO_DRAW_ROWS_VISIBLE; row++)
        {
            /* 1-frame/row stagger (S3.3): row N doesn't start falling
             * until frame N, then eases the remaining frames it has
             * left in the budget down to 0 -- always lands exactly on
             * 0 in its own last frame regardless of how few frames
             * that leaves it (metro_ease()'s i==frames endpoint). */
            int start_frame = row;
            int local_frame = frame - start_frame;
            int local_total = METRO_FEATHER_FRAMES - start_frame;

            if (local_frame < 0)
                y_offsets[row] = METRO_FEATHER_ROW_OFFSET_PX;
            else
            {
                int p = metro_ease(METRO_EASE_OUT_QUAD, local_frame + 1, local_total);
                y_offsets[row] = METRO_FEATHER_ROW_OFFSET_PX -
                                  (METRO_FEATHER_ROW_OFFSET_PX * p) / 256;
            }
        }

        metro_draw_clear_rows_area();
        metro_draw_rows_ex(pivot, first, sel, 0, y_offsets);
        lcd_update();

        if (button_queue_full())
            button_clear_queue();
        if (frame < METRO_FEATHER_FRAMES - 1)
            sleep(3);
    }
}

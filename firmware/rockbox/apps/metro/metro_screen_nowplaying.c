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

/* config.h before tagcache.h -- see DECISIONS.md M-030. */
#include "config.h"
#include "tagcache.h"
#include "playlist.h"
#include "audio.h"
#include "kernel.h"
#include "lcd.h"
#include "misc.h"
#include "sound.h"
#include "settings.h"
#include "string-extra.h"
#include "file.h"

#include "metro_screen_nowplaying.h"
#include "metro_screen_list.h"
#include "metro_draw.h"
#include "metro_theme.h"
#include "metro_lang.h"
#include "metro_widgets.h"
#include "metro_albumart.h"
#include "metro_music.h"
#include "metro_settings.h"
#include "metro_fb.h"
#include "metro_keymap.h"
#include "metro_fsutil.h"
#include "metro_lrc.h"
#include "metro_motion.h" /* R4/FA-9: metro_seek_step_ms() */
#include "metro_volume.h" /* R5-F3: 00..15 */

/* R4/FA-9 (M-072): sin un evento de búsqueda en este lapso, la racha
 * se considera terminada (el usuario soltó el botón) y el paso vuelve
 * a ser fino. Holgado respecto al intervalo de repetición del driver,
 * para no reiniciarse a mitad de un hold. La rampa en sí vive en
 * metro_motion.c (host-testeable). */
#define METRO_SEEK_IDLE_TICKS   (HZ / 2)
#define METRO_NP_QUEUE_MAX      60

/* R5-F3 (M-083): el nivel de volumen ("00".."15") aparece al ajustar,
 * se queda 3 s quieto desde el ÚLTIMO ajuste y luego se desvanece
 * despacio hacia el fondo durante 1 s. El desvanecimiento es un fundido
 * de color del texto (metro_fb_blend_color), no de un frame buffer --
 * barato y sin estado gráfico. Con animations=off se corta en seco a
 * los 3 s; respeta lcd_active() como toda animación. */
#define METRO_VOL_HOLD_TICKS  (HZ * 3)
#define METRO_VOL_FADE_TICKS  (HZ)
#define METRO_VOL_FADE_STEPS  8

/* Geometría del reproductor (maqueta del dueño, R5-F3). Píxeles fijos
 * como en el resto de la app, no derivados de métricas de fuente. */
#define NP_LEFT_X        12
#define NP_VOL_Y         30   /* nivel de volumen, MFONT_LIST, sobre la carátula */
#define NP_COVER_Y       56   /* 130px -> 186 (M-097: la maestra, antes 136) */
#define NP_COL_X        164   /* columna derecha */
#define NP_MODE_Y        60   /* estrella / aleatorio / repetir, 16px */
#define NP_MODE_PITCH    36
#define NP_RING_Y        86   /* anillos de transporte, r=13 -> 27px -> 113 */
#define NP_RING_R        13
#define NP_RING_PITCH    36
#define NP_ARTIST_Y     124   /* MFONT_LIST_SEL, mayúsculas */
#define NP_ALBUM_Y      148   /* MFONT_LIST */
#define NP_TITLE_Y      170   /* MFONT_LIST -> 190 */
#define NP_BAR_Y        206   /* barra de progreso, con márgenes */
#define NP_BAR_H          3
#define NP_TIMES_Y      213   /* caption 14 -> 227, DEBAJO de la barra */

/* --- sentinel page: never drawn/queried through the generic list path,
 * just a stack-bookkeeping placeholder metro_screen_nowplaying_is_current()
 * recognizes by pointer (see metro_screen_nowplaying.h). */

static int sentinel_count(void *ctx) { (void)ctx; return 0; }
static void sentinel_get_row(void *ctx, int index, struct metro_row *out)
{ (void)ctx; (void)index; (void)out; }
static void sentinel_on_select(void *ctx, int index) { (void)ctx; (void)index; }

static const struct metro_pivot sentinel_pivots[] = {
    { LANG_HUB_NOWPLAYING, sentinel_count, sentinel_get_row, sentinel_on_select, NULL },
};
static const struct metro_page sentinel_page = { LANG_HUB_NOWPLAYING, sentinel_pivots, 1, NULL };

bool metro_screen_nowplaying_push(void)
{
    return metro_screen_list_push(&sentinel_page);
}

bool metro_screen_nowplaying_is_current(void)
{
    return metro_screen_list_current_page() == &sentinel_page;
}

/* --- options page: up next (real queue via playlist_get_track_info(),
 * titles resolved once per push, not per row-draw) + shuffle/repeat
 * toggles. A normal metro_page pushed on top of the sentinel -- MENU
 * pops back to Now Playing through the ordinary metro_screen_list
 * path, nothing special needed here for that. F5 keeps this read-only
 * beyond the two toggle rows: selecting a queued track doesn't jump to
 * it (deferred, not part of this phase's "hecho" list). */

static char s_queue_labels[METRO_NP_QUEUE_MAX][METRO_MUSIC_ITEM_LEN];
static int s_queue_n;

static void queue_refresh(void)
{
    int cur = playlist_get_display_index() - 1;
    int total = playlist_amount();
    int i, n = 0;

    if (cur < 0)
        cur = 0;

    for (i = cur; i < total && n < METRO_NP_QUEUE_MAX; i++)
    {
        struct playlist_track_info info;
        bool got_title = false;

        /* playlist_get_track_info() returns 0 on success, -1 on
         * failure -- an errno-style status, not a bool. `!result`
         * would skip every successfully-resolved track instead of
         * the failed ones. */
        if (playlist_get_track_info(NULL, i, &info) != 0)
            continue;

        if (tagcache_is_usable())
        {
            struct tagcache_search tcs;
            if (tagcache_find_index(&tcs, info.filename))
            {
                if (tagcache_retrieve(&tcs, tcs.idx_id, tag_title,
                                       s_queue_labels[n], METRO_MUSIC_ITEM_LEN)
                    && strcmp(s_queue_labels[n], UNTAGGED) != 0)
                    got_title = true;
                tagcache_search_finish(&tcs);
            }
        }

        if (!got_title)
        {
            const char *base = strrchr(info.filename, '/');
            strlcpy(s_queue_labels[n], base ? base + 1 : info.filename,
                    METRO_MUSIC_ITEM_LEN);
        }
        n++;
    }

    s_queue_n = n;
}

/* R3-F2/DD-2/DD-3 (M-063): synced lyrics, full-screen mode. "cache of
 * 1" keyed by id3->path, same shape as metro_albumart.c's own
 * load_art() -- reload only when the track actually changed, never
 * per redraw. metro_lrc_parse() works in place on lrc.buf, so the file
 * is read directly into it (no second buffer). A track with no
 * sibling .lrc (or an unreadable/empty one) leaves s_lrc.count at 0 --
 * that's the single source of truth both the Options row and the
 * full-screen mode itself check before showing anything, so there's
 * never a blank lyrics screen (same fallback criterion Aura's own
 * parser follows, INVESTIGACION-metro-r3.md A.3). */
static struct metro_lrc s_lrc;
static char s_lrc_loaded_path[MAX_PATH];
static bool s_lrc_loaded;
static bool s_lyrics_mode;

static bool ensure_lyrics_loaded(struct mp3entry *id3)
{
    char lrc_path[MAX_PATH];
    int fd;
    ssize_t n;

    if (s_lrc_loaded && !strcmp(s_lrc_loaded_path, id3->path))
        return s_lrc.count > 0;

    s_lrc.count = 0;
    s_lrc_loaded = true;
    strlcpy(s_lrc_loaded_path, id3->path, sizeof(s_lrc_loaded_path));

    if (!metro_lrc_sibling_path(id3->path, lrc_path, sizeof(lrc_path)))
        return false;

    fd = open(lrc_path, O_RDONLY);
    if (fd < 0)
        return false;

    n = read(fd, s_lrc.buf, sizeof(s_lrc.buf) - 1);
    close(fd);
    if (n <= 0)
        return false;

    return metro_lrc_parse(&s_lrc, (size_t)n);
}

/* R4/FA-9 (M-072): racha de eventos consecutivos. Estado de sesión
 * puro; la política de cuánto saltar es metro_seek_step_ms(). */
static int  s_seek_run;
static long s_seek_last_tick;

static long seek_step_ms(void)
{
    if (current_tick - s_seek_last_tick > METRO_SEEK_IDLE_TICKS)
        s_seek_run = 0; /* se soltó el botón: vuelve al paso fino */

    s_seek_last_tick = current_tick;
    return metro_seek_step_ms(s_seek_run++);
}

static void toggle_shuffle(void)
{
    /* Rockbox has no clean "unshuffle" (real limitation, not a Metro
     * gap) -- turning shuffle off only stops treating the flag as on
     * from here on, it doesn't restore the original order. Turning it
     * on reorders the remaining queue right away, same as any other
     * Rockbox shuffle trigger. */
    global_settings.playlist_shuffle = !global_settings.playlist_shuffle;
    if (global_settings.playlist_shuffle)
        playlist_shuffle(current_tick, playlist_get_display_index());
}

static void cycle_repeat(void)
{
    switch (global_settings.repeat_mode)
    {
        case REPEAT_OFF: global_settings.repeat_mode = REPEAT_ALL; break;
        case REPEAT_ALL: global_settings.repeat_mode = REPEAT_ONE; break;
        default:         global_settings.repeat_mode = REPEAT_OFF; break;
    }
}

static enum metro_lang_id repeat_value_lang(void)
{
    switch (global_settings.repeat_mode)
    {
        case REPEAT_ALL: return LANG_REPEAT_ALL;
        case REPEAT_ONE: return LANG_REPEAT_ONE;
        default:         return LANG_VALUE_OFF;
    }
}

/* R3-F5/DD-7 (M-066): tag_rating is native Rockbox, stored 0-10; the
 * UI cycles/shows 5 stars, mapped x2 -- same even-number convention
 * Aura-Firmware's own commit_rating() uses (consultado read-only), so
 * a rating either side writes/reads means the same thing on disk. */
static char s_rating_subtitle[8];

static void cycle_rating(void)
{
    struct mp3entry *id3 = audio_current_track();
    int stars;

    if (!id3)
        return;

    stars = (id3->rating / 2 + 1) % 6; /* 0..5, wraps */
    id3->rating = stars * 2;

    /* R3-F5/DD-7: verified live (calificar -> cambiar de pista ->
     * volver) que el `tagtree_buffer_event()` de stock (apps/tagtree.c,
     * registrado sin condiciones desde el propio `tagtree_init()` de
     * apps/main.c, jamás tocado por Metro) ya restaura `tag_rating` en
     * `id3->rating` en cada buffer -- a diferencia de Aura, que nunca
     * llama `tagtree_init()` (árbol propio, `INVESTIGACION-metro-r3.md`
     * C.2) y por eso sí necesita su propio listener de
     * `PLAYBACK_EVENT_TRACK_BUFFER` para esto mismo. Metro lo hereda
     * gratis -- ningún callback nuevo registrado aquí. Verificación en
     * `docs/DESVIACIONES.md` R3-5. */
    if (id3->tagcache_idx && global_settings.runtimedb)
    {
        tagcache_update_numeric(id3->tagcache_idx - 1, tag_rating, id3->rating);

        /* R3-F5/DD-7 (M-066): mismo motivo que `import_ratings()`
         * (`metro_sync.c`) -- `tagcache_update_numeric()` solo ENCOLA
         * la escritura; sin forzar el volcado, calificar una pista
         * quedaría invisible para cualquier lectura hasta que la cola
         * se llenara sola (32 pendientes) o el dispositivo se apagara
         * de verdad. Una calificación puesta a mano es exactamente la
         * clase de escritura poco frecuente e intencional que vale la
         * pena hacer persistir de inmediato -- `tagcache_shutdown()`
         * es solo `run_command_queue(true)` en este target (ver
         * `import_ratings()`), seguro de llamar aquí mismo. */
        tagcache_shutdown();
    }
}

static const char *rating_subtitle(void)
{
    struct mp3entry *id3 = audio_current_track();
    int stars = id3 ? id3->rating / 2 : 0;

    if (stars < 0)
        stars = 0;
    else if (stars > 5)
        stars = 5; /* defensive: tag_rating is meant to stay 0-10 */

    snprintf(s_rating_subtitle, sizeof(s_rating_subtitle), "%d/5", stars);
    return s_rating_subtitle;
}

/* R3-F2/DD-2: whether the CURRENT track has usable lyrics -- the
 * single check both this row and metro_screen_nowplaying_show() rely
 * on so the mode is never reachable (nor stays shown) without them.
 * ensure_lyrics_loaded() is a cache-of-1 keyed by path (same shape as
 * metro_albumart.c's load_art()), so calling it every redraw of this
 * row is as cheap as calling it every redraw of Now Playing itself
 * already is -- a no-op strcmp() unless the track changed. */
static bool lyrics_available(void)
{
    struct mp3entry *id3 = audio_current_track();
    return id3 && ensure_lyrics_loaded(id3);
}

static int options_count(void *ctx) { (void)ctx; return 4 + s_queue_n; }

static void options_get_row(void *ctx, int index, struct metro_row *out)
{
    (void)ctx;

    if (index == 0)
    {
        out->title = metro_lang_str(LANG_NP_SHUFFLE);
        out->subtitle = metro_lang_str(global_settings.playlist_shuffle
                                            ? LANG_VALUE_ON : LANG_VALUE_OFF);
        out->kind = METRO_ROW_SETTING;
    }
    else if (index == 1)
    {
        out->title = metro_lang_str(LANG_NP_REPEAT);
        out->subtitle = metro_lang_str(repeat_value_lang());
        out->kind = METRO_ROW_SETTING;
    }
    else if (index == 2)
    {
        out->title = metro_lang_str(LANG_NP_LYRICS);
        out->subtitle = !lyrics_available() ? metro_lang_str(LANG_VALUE_UNAVAILABLE)
                         : s_lyrics_mode     ? metro_lang_str(LANG_VALUE_ON)
                                             : metro_lang_str(LANG_VALUE_OFF);
        out->kind = METRO_ROW_SETTING;
    }
    else if (index == 3)
    {
        out->title = metro_lang_str(LANG_NP_RATING);
        out->subtitle = rating_subtitle();
        out->kind = METRO_ROW_SETTING;
    }
    else
    {
        out->title = s_queue_labels[index - 4];
        out->subtitle = NULL;
        out->kind = METRO_ROW_ACTION;
    }
}

static void options_on_select(void *ctx, int index)
{
    (void)ctx;

    if (index == 0)
        toggle_shuffle();
    else if (index == 1)
        cycle_repeat();
    else if (index == 2 && lyrics_available())
        s_lyrics_mode = !s_lyrics_mode;
    else if (index == 3)
        cycle_rating();
}

static const struct metro_pivot options_pivots[] = {
    { LANG_NP_OPTIONS_TITLE, options_count, options_get_row, options_on_select, NULL },
};
static const struct metro_page options_page = { LANG_NP_OPTIONS_TITLE, options_pivots, 1, NULL };

static void push_options(void)
{
    queue_refresh();
    metro_screen_list_push(&options_page);
}

/* --- Now Playing itself: custom layout, PLAN_MAESTRO.md S1.4 ---------
 * R5-F3 (M-083): rediseñado sobre la maqueta del dueño. Carátula a la
 * izquierda; a la derecha, una fila de tres glifos de estado (estrella /
 * aleatorio / repetir, SIEMPRE visibles: terciario apagados, acento
 * encendidos), una fila de tres anillos de transporte (anterior /
 * play-pausa / siguiente, círculo de 1px sin antialias), y tres líneas
 * ARTISTA (versalitas, semibold) / álbum / título. Abajo, la barra de
 * progreso con márgenes y los tiempos DEBAJO de ella. El volumen ya no
 * es una barra: es el nivel "00".."15" sobre la carátula, que aparece
 * al ajustar y se desvanece. */

static long s_vol_last_adjust_tick = -1;

/* True mientras el nivel de volumen está en pantalla (quieto o
 * desvaneciéndose): metro_main.c redibuja más seguido en ese lapso. */
bool metro_screen_nowplaying_volume_visible(void)
{
    if (s_vol_last_adjust_tick < 0)
        return false;
    return current_tick - s_vol_last_adjust_tick <
           METRO_VOL_HOLD_TICKS + METRO_VOL_FADE_TICKS;
}

static void draw_volume_level(void)
{
    char buf[4];
    long age;
    unsigned color = metro_color_fg();

    if (!metro_screen_nowplaying_volume_visible())
        return;

    age = current_tick - s_vol_last_adjust_tick;
    if (age >= METRO_VOL_HOLD_TICKS)
    {
        /* Fase de desvanecimiento. En pasos discretos (no por tick)
         * para que cada cuadro sea un color distinto y visible, y
         * porque ocho tonos bastan a esta densidad. Sin animaciones (o
         * LCD apagado) no hay fundido: ya no se dibuja y punto. */
        long into = age - METRO_VOL_HOLD_TICKS;
        int step;

        if (metro_settings.animations == METRO_ANIM_OFF || !lcd_active())
            return;

        step = (int)(into * METRO_VOL_FADE_STEPS / METRO_VOL_FADE_TICKS);
        if (step >= METRO_VOL_FADE_STEPS)
            return;
        color = metro_fb_blend_color(metro_color_fg(), metro_color_bg(),
                                     step * 256 / METRO_VOL_FADE_STEPS);
    }

    snprintf(buf, sizeof(buf), "%02d", metro_music_volume_level());
    metro_draw_text(MFONT_LIST, NP_LEFT_X, NP_VOL_Y, buf, color);
}

/* Fila de estado: estrella (calificación > 0 -- lo que Studio exporta
 * como "favorito" en ratings.cfg es la calificación, no hay otra
 * bandera), aleatorio, repetir. Siempre los tres, el color dice el
 * estado: terciario = apagado, acento = encendido. REPEAT_ONE lleva el
 * "1" al lado del lazo (M-077). */
static void draw_mode_row(struct mp3entry *id3)
{
    int x = NP_COL_X;
    bool starred = id3 && id3->rating > 0;
    bool repeat = global_settings.repeat_mode == REPEAT_ALL ||
                  global_settings.repeat_mode == REPEAT_ONE;
    unsigned on = metro_color_accent(), off = metro_color_tertiary();

    metro_widgets_draw_icon(METRO_ICON_STAR, x, NP_MODE_Y, starred ? on : off);
    x += NP_MODE_PITCH;
    metro_widgets_draw_icon(METRO_ICON_SHUFFLE, x, NP_MODE_Y,
                            global_settings.playlist_shuffle ? on : off);
    x += NP_MODE_PITCH;
    metro_widgets_draw_icon(METRO_ICON_REPEAT_ALL, x, NP_MODE_Y, repeat ? on : off);
    if (global_settings.repeat_mode == REPEAT_ONE)
        metro_draw_text(MFONT_CAPTION, x + METRO_WIDGETS_ICON_SIZE + 2, NP_MODE_Y,
                        "1", on);
}

/* Fila de transporte: tres anillos. El del centro muestra el ESTADO,
 * con la asimetría de M-073 conservada: play en fg mientras suena (lo
 * normal no grita), pausa en acento (es lo que uno busca con la mirada
 * cuando no se oye nada). */
static void draw_transport_row(void)
{
    int x = NP_COL_X;
    int status = audio_status();
    unsigned ring = metro_color_fg();

    metro_widgets_draw_icon_in_circle(METRO_ICON_PREVIOUS, x, NP_RING_Y, NP_RING_R,
                                      ring, metro_color_fg());
    x += NP_RING_PITCH;
    if (status & AUDIO_STATUS_PAUSE)
        metro_widgets_draw_icon_in_circle(METRO_ICON_PAUSE, x, NP_RING_Y, NP_RING_R,
                                          ring, metro_color_accent());
    else
        metro_widgets_draw_icon_in_circle(METRO_ICON_PLAY, x, NP_RING_Y, NP_RING_R,
                                          ring, metro_color_fg());
    x += NP_RING_PITCH;
    metro_widgets_draw_icon_in_circle(METRO_ICON_NEXT, x, NP_RING_Y, NP_RING_R,
                                      ring, metro_color_fg());
}

/* R4/FA-7 (M-078): el fondo deja de ser la misma imagen que el tile.
 *
 * Tabla acordada con el dueño (Q7 de la Fase 1), leída por columnas:
 *
 *   artista | álbum | FONDO             | TILE
 *   --------+-------+-------------------+---------------------------
 *      sí   |  sí   | foto del artista  | carátula real
 *      sí   |  no   | foto del artista  | acento + inicial
 *      no   |  sí   | carátula          | carátula real
 *      no   |  no   | plano (tema)      | acento + inicial
 *
 * La columna del TILE ya se comportaba así antes de esa fase (el
 * respaldo de acento + inicial existe desde F5), así que ahí solo
 * cambió la del FONDO. Y esa columna se reduce a una cascada: foto de
 * artista si la hay, si no la carátula, si no nada.
 *
 * La política vive aquí y no en metro_albumart.c a propósito: ese
 * módulo decodifica, no decide (su propia cabecera ya lo dice para el
 * nivel de FX). */
static bool load_background(void)
{
    struct mp3entry *id3 = audio_current_track();
    char filename[METRO_FSUTIL_NAME_LEN];
    char dir[MAX_PATH], path[MAX_PATH];
    long mtime;

    if (id3 && id3->artist &&
        metro_music_artist_image(id3->artist, filename, sizeof(filename), &mtime))
    {
        /* metro_settings_artists_dir() es la única autorizada a armar
         * esta ruta -- regla de rutas de contrato del CLAUDE.md. */
        metro_settings_artists_dir(dir, sizeof(dir));
        snprintf(path, sizeof(path), "%s/%s", dir, filename);
        /* M-097: el mtime habilita la maestra de artista compartida. */
        if (metro_albumart_load_background_file(path, mtime))
            return true;
        /* Mapeada pero ilegible (archivo corrupto, borrado entre el
         * índice y aquí): se cae a la carátula, no a fondo plano. */
    }

    return metro_albumart_load_background();
}

/* F12: 30% of the track's own art, scaled to fill the screen, behind
 * everything else -- graphics=full only (PLAN_MAESTRO.md S3.3);
 * static, redrawn plainly every metro_screen_nowplaying_show() call
 * like the rest of this screen, never animated on its own.
 *
 * R2-F1/DD-2 (M-052): re-verified against a near-white cover fixture
 * (0xF2F2EC, "Wheel & Click/Analog Dreams" in gen_test_media.sh) --
 * kept at 77 rather than dropping to 51 (20%). Blending at only 30%
 * keeps metro_color_bg()'s dark base dominant in the composite
 * regardless of how bright the source art is (0.3*brightCover +
 * 0.7*darkBg still lands in a medium-dark gray, not near-white), so
 * the tertiary (album) text line stays legible in practice even
 * against the palest cover this fixture set can produce -- see
 * docs/screenshots/R2-F1-np-worstcase.png. */
#define METRO_NP_BG_ALPHA256 77 /* ~30% of 256 */

/* R3-F2/DD-2: full-screen lyrics -- active line in the title face at
 * screen center, up to 2 lines of context each side in the list face,
 * dimmed further with distance. Left-aligned at x=12 like every other
 * NP text (Metro doesn't center text), never the "vidrio" translucent
 * panel Aura-Firmware uses for the same problem (INVESTIGACION-metro-r3.md
 * A.4) -- Metro's flat language has no glass, and the 30% cover dim
 * already applied above (metro_fb_blend_over_color) is what keeps this
 * legible over a bright cover instead. Y positions are fixed pixels,
 * same convention as the rest of this screen (and the whole app) --
 * not computed from font metrics. */
#define METRO_LYRICS_TOP_Y   40
#define METRO_LYRICS_PITCH   32
#define METRO_LYRICS_CONTEXT 2

static void draw_lyrics_mode(struct mp3entry *id3)
{
    int active = metro_lrc_find_active(&s_lrc, (uint32_t)id3->elapsed);
    int center_y = METRO_LYRICS_TOP_Y + (LCD_HEIGHT - METRO_LYRICS_TOP_Y) / 2;
    int i;

    for (i = -METRO_LYRICS_CONTEXT; i <= METRO_LYRICS_CONTEXT; i++)
    {
        int idx = active + i;
        int dist = (i < 0) ? -i : i;
        int y = center_y + i * METRO_LYRICS_PITCH;
        const char *text;

        if (idx < 0 || idx >= s_lrc.count)
            continue;

        text = metro_lrc_text(&s_lrc, idx);
        if (!text || !*text)
            continue;

        if (dist == 0)
            metro_draw_text_cut_right(MFONT_TITLE, 12, y, text,
                                       metro_color_fg(), LCD_WIDTH - 24);
        else
            metro_draw_text_cut_right(MFONT_LIST, 12, y, text,
                                       dist == 1 ? metro_color_secondary()
                                                  : metro_color_tertiary(),
                                       LCD_WIDTH - 24);
    }
}

static void draw_progress_and_times(struct mp3entry *id3)
{
    char elapsed_buf[16], total_buf[16];
    int w, h, pct;
    int bar_w = LCD_WIDTH - 2 * NP_LEFT_X;

    pct = id3->length > 0 ? (int)((unsigned long long)id3->elapsed * 100 / id3->length) : 0;
    metro_draw_progress(NP_LEFT_X, NP_BAR_Y, bar_w, NP_BAR_H, pct);

    format_time(elapsed_buf, sizeof(elapsed_buf), (long)id3->elapsed);
    metro_draw_text(MFONT_CAPTION, NP_LEFT_X, NP_TIMES_Y, elapsed_buf,
                    metro_color_secondary());

    /* Derecha: duración total (la maqueta muestra dos cifras fijas a
     * los extremos; restante cambiaría cada segundo y "baila"). */
    format_time(total_buf, sizeof(total_buf), (long)id3->length);
    lcd_setfont(metro_font_id(MFONT_CAPTION));
    lcd_getstringsize((const unsigned char *)total_buf, &w, &h);
    metro_draw_text(MFONT_CAPTION, LCD_WIDTH - NP_LEFT_X - w, NP_TIMES_Y, total_buf,
                    metro_color_secondary());
}

void metro_screen_nowplaying_show(void)
{
    struct mp3entry *id3;
    bool has_bg = metro_settings.graphics == METRO_GFX_FULL && load_background();

    if (has_bg)
        metro_fb_blend_over_color(metro_albumart_background_bitmap(),
                                   metro_color_bg(), METRO_NP_BG_ALPHA256);
    else
        metro_draw_clear();

    metro_draw_header(metro_lang_str(LANG_HUB_NOWPLAYING));

    if (!metro_music_is_playing() || !(id3 = audio_current_track()))
    {
        metro_draw_tile(NP_LEFT_X, NP_COVER_Y, METRO_ALBUMART_SIZE, "?");
        lcd_update();
        return;
    }

    if (s_lyrics_mode && ensure_lyrics_loaded(id3) && s_lrc.count > 0)
    {
        /* Replaces the whole normal layout (art, text, times, progress
         * bar, mode icons) -- "pantalla completa" means the whole
         * content area, not an overlay on top of it. The volume level
         * below still applies on top regardless: it's transient
         * feedback for an action just taken, not part of the layout
         * this mode replaces. */
        draw_lyrics_mode(id3);
    }
    else
    {
        char upper[METRO_MUSIC_ITEM_LEN];
        int col_w = LCD_WIDTH - NP_COL_X - NP_LEFT_X;

        if (metro_albumart_load_current())
            lcd_bitmap(metro_albumart_bitmap(), NP_LEFT_X, NP_COVER_Y,
                       METRO_ALBUMART_SIZE, METRO_ALBUMART_SIZE);
        else
            metro_draw_tile(NP_LEFT_X, NP_COVER_Y, METRO_ALBUMART_SIZE,
                             id3->album ? id3->album : (id3->title ? id3->title : "?"));

        draw_mode_row(id3);
        draw_transport_row();

        /* ARTISTA / álbum / título -- el orden de la maqueta (y del Zune
         * original): la línea fuerte es quién, luego de dónde, luego qué. */
        metro_lang_upper(id3->artist ? id3->artist
                                     : metro_lang_str(LANG_UNKNOWN_ARTIST),
                         upper, sizeof(upper));
        metro_draw_text_cut_right(MFONT_LIST_SEL, NP_COL_X, NP_ARTIST_Y, upper,
                                   metro_color_fg(), col_w);
        metro_draw_text_cut_right(MFONT_LIST, NP_COL_X, NP_ALBUM_Y,
                                   id3->album ? id3->album
                                              : metro_lang_str(LANG_UNKNOWN_ALBUM),
                                   metro_color_secondary(), col_w);
        metro_draw_text_cut_right(MFONT_LIST, NP_COL_X, NP_TITLE_Y,
                                   id3->title ? id3->title : "?",
                                   metro_color_secondary(), col_w);

        draw_progress_and_times(id3);
    }

    draw_volume_level();

    lcd_update();
}

void metro_screen_nowplaying_handle(int action, int steps)
{
    struct mp3entry *id3;
    long pos;

    switch (action)
    {
        case MACT_VOL_UP:
            metro_music_volume_step(steps);
            s_vol_last_adjust_tick = current_tick;
            break;
        case MACT_VOL_DOWN:
            metro_music_volume_step(-steps);
            s_vol_last_adjust_tick = current_tick;
            break;
        case MACT_TRACK_PREV:
            audio_prev();
            break;
        case MACT_TRACK_NEXT:
            audio_next();
            break;
        case MACT_SEEK_BACK:
            id3 = audio_current_track();
            if (id3)
            {
                pos = (long)id3->elapsed - seek_step_ms();
                audio_ff_rewind(pos > 0 ? pos : 0);
            }
            break;
        case MACT_SEEK_FWD:
            id3 = audio_current_track();
            if (id3)
            {
                pos = (long)id3->elapsed + seek_step_ms();
                audio_ff_rewind(pos < (long)id3->length ? pos : (long)id3->length);
            }
            break;
        case MACT_OPTIONS:
            push_options();
            break;
        case MACT_TOGGLE_SHUFFLE:
            toggle_shuffle();
            break;
        case MACT_PLAYPAUSE:
            metro_music_playpause();
            break;
        case MACT_BACK:
            metro_screen_list_pop();
            break;
        default:
            break;
    }
}

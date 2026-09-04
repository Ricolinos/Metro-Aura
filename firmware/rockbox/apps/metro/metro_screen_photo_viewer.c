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

#include "audio.h"
#include "file.h"
#include "jpeg_load.h"
#include "bmp.h"
#include "kernel.h" /* M-109: current_tick, HZ -- the scrub-settle debounce */

#include "metro_screen_photo_viewer.h"
#include "metro_master_art.h" /* the decode lock -- M-097 */
#include "metro_screen_list.h"
#include "metro_music.h" /* R4/FA-8: metro_music_playpause() */
#include "metro_draw.h"
#include "metro_theme.h"
#include "metro_lang.h"
#include "metro_keymap.h"

#define PHOTOS_DIR "/Photos" /* same local-constant precedent metro_photos.c/metro_video.c already use */

/* R2-F3/DD-10 (M-058): oversized on purpose (x2 margin, DECISIONS.md
 * M-033) -- FORMAT_RESIZE needs real headroom over the final bitmap
 * size before it downscales, not just LCD_WIDTH*LCD_HEIGHT*sizeof(fb_data).
 * Own buffer, not shared with Now Playing's metro_albumart.c -- both
 * can be the largest thing on screen for their own page, but never at
 * the same time (this page's sentinel and Now Playing's are mutually
 * exclusive, see metro_screen_photo_viewer_is_current()). */
#define METRO_PHOTO_VIEW_SCRATCH_SIZE (LCD_WIDTH * LCD_HEIGHT * 2 * 2)
static unsigned char s_scratch[METRO_PHOTO_VIEW_SCRATCH_SIZE];

/* Only show a "loading" message before a decode that will actually be
 * noticeable -- same threshold Aura-Firmware's aura_photos.c uses
 * (read-only reference), which also happens to match this contract's
 * own photo size cap (COMPAT_STUDIO.md C13, <=640px): a contract-
 * compliant photo never shows this, it's a courtesy for anything that
 * slipped past that cap. */
#define METRO_PHOTO_LOADING_INDICATOR_SIDE 640

/* M-109 (ronda "ajustes 2", plan maestro SS C.2): la ventana de
 * "quietud" antes de decodificar de verdad. >= 150 ms sin eventos de
 * rueda nuevos -- ver metro_photos_viewer_wants_ticks()/take_slide()/
 * show() para como se usa. HZ = 100 en este target, asi que son
 * exactamente 15 ticks, sin redondeo. */
#define METRO_PHOTO_SETTLE_TICKS (HZ * 150 / 1000)

static const metro_photo_item_t *s_items;
static int s_count;
static int s_index;

/* M-109: current_tick del ultimo cambio de s_index (por rueda o por
 * LEFT/RIGHT). Mientras `current_tick - s_nav_tick` sea menor que
 * METRO_PHOTO_SETTLE_TICKS, el visor esta "en scrubbing": se muestra
 * la vista previa barata (draw_scrub_preview()) y NO se decodifica
 * nada. En reposo (recien empujado, o mucho despues del ultimo
 * evento) esta diferencia ya es enorme, asi que el primer dibujo
 * siempre entra directo al camino de decode real -- por eso
 * metro_screen_photo_viewer_push() lo deja deliberadamente "viejo". */
static long s_nav_tick;

static int s_loaded_index = -1;
static bool s_loaded_ok = false;
/* Not reset on push() on purpose -- persists across photos and across
 * viewer sessions during the same boot (matches Aura-Firmware's own
 * viewer, whose equivalent flag is likewise never reset per-photo).
 * Starts at false (ajustar) the first time ever, same default DD-10
 * implies by listing "ajustar" first. */
static bool s_cover_mode = false;
static int s_display_w, s_display_h;
static struct bitmap s_bm;

static int s_probed_index = -1;
static bool s_probe_ok = false;
static int s_probed_w, s_probed_h;

/* --- JPEG dimension probe (no pixel decode) -------------------------
 *
 * Ported from Aura-Firmware's aura_photos.c (probe_jpeg_dimensions(),
 * read as reference per PLAN-metro-r2-maestro.md DD-10) -- reads only
 * the JPEG marker stream up to SOFn, never decodes a single pixel.
 * Needed for "cubrir": computing the cover scale factor ahead of the
 * real decode requires knowing the source dimensions first (unlike
 * the photo grid's thumbnails, R2-F2/DD-9, which are small enough to
 * get away with a single KEEP_ASPECT decode + a cheap nearest-neighbour
 * re-crop instead -- see metro_thumbs.c's own comment on why
 * that shortcut isn't precise enough here at full screen size). */
typedef enum {
    METRO_JPEG_PROBE_UNKNOWN = 0, /* unrecognized header -- let the decoder try anyway */
    METRO_JPEG_PROBE_BASELINE,
    METRO_JPEG_PROBE_UNSUPPORTED, /* progressive/arithmetic SOF -- Rockbox can't decode it */
} metro_jpeg_probe_t;

static metro_jpeg_probe_t probe_jpeg_dimensions(const char *path, int *out_w, int *out_h)
{
    int fd;
    unsigned char marker[2];
    metro_jpeg_probe_t result = METRO_JPEG_PROBE_UNKNOWN;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return METRO_JPEG_PROBE_UNKNOWN;

    if (read(fd, marker, 2) != 2 || marker[0] != 0xFF || marker[1] != 0xD8)
    {
        close(fd);
        return METRO_JPEG_PROBE_UNKNOWN;
    }

    for (;;)
    {
        unsigned char lenbuf[2];
        unsigned char sofbuf[5];
        int len;

        if (read(fd, marker, 2) != 2 || marker[0] != 0xFF)
            break;
        while (marker[1] == 0xFF) /* padding bytes before the real marker code */
        {
            if (read(fd, &marker[1], 1) != 1)
                break;
        }

        if (marker[1] == 0x01 || (marker[1] >= 0xD0 && marker[1] <= 0xD7))
            continue; /* TEM/RSTn -- no length field */
        if (marker[1] == 0xD9 || marker[1] == 0xDA)
            break; /* EOI/SOS with no SOFn seen -- give up */

        if (read(fd, lenbuf, 2) != 2)
            break;
        len = (lenbuf[0] << 8) | lenbuf[1];

        if (marker[1] == 0xC0 || marker[1] == 0xC1) /* SOF0/SOF1 baseline */
        {
            if (read(fd, sofbuf, 5) == 5)
            {
                *out_h = (sofbuf[1] << 8) | sofbuf[2];
                *out_w = (sofbuf[3] << 8) | sofbuf[4];
                result = METRO_JPEG_PROBE_BASELINE;
            }
            break;
        }
        if (marker[1] >= 0xC2 && marker[1] <= 0xCF && marker[1] != 0xC4 && marker[1] != 0xC8)
        {
            result = METRO_JPEG_PROBE_UNSUPPORTED;
            break;
        }

        if (len < 2 || lseek(fd, len - 2, SEEK_CUR) < 0)
            break;
    }

    close(fd);
    return result;
}

/* --- cover-mode scale math (Q16.16, no FPU) --------------------------
 *
 * Ported from Aura-Firmware's compute_decode_and_display_size()
 * (aura_photos.c:943-1002, read as reference per DD-10) -- computes
 * the cover scale factor (max(LCD_WIDTH/w, LCD_HEIGHT/h), the opposite
 * of FORMAT_KEEP_ASPECT's fit-scale), then clamps the DECODE size down
 * (display size stays the true cover size) if that would overflow
 * METRO_PHOTO_VIEW_SCRATCH_SIZE -- an integer sqrt by bisection, same
 * technique metro_transitions.c's Q10.6 turnstile math already uses
 * (shift instead of float), just a different shift width. */
static void compute_decode_and_display_size(int src_w, int src_h,
                                             int *decode_w, int *decode_h,
                                             int *display_w, int *display_h)
{
    long scale_x = ((long)LCD_WIDTH << 16) / src_w;
    long scale_y = ((long)LCD_HEIGHT << 16) / src_h;
    long ideal_scale = (scale_x > scale_y) ? scale_x : scale_y; /* max: cover, not fit */
    long dw, dh;

    dw = (src_w * ideal_scale) >> 16;
    dh = (src_h * ideal_scale) >> 16;
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    *display_w = (int)dw;
    *display_h = (int)dh;

    if (ideal_scale <= (1L << 16))
    {
        /* Source already covers without enlarging -- decode straight
         * to the final size, cheapest path (1:1 blit, no resampling). */
        *decode_w = *display_w;
        *decode_h = *display_h;
    }
    else
    {
        *decode_w = src_w;
        *decode_h = src_h;
    }

    /* R2-F3 bug found verifying this against a real fixture (640x300,
     * cover mode): Aura-Firmware's own formula (ported above) only
     * checks decode_w*decode_h against the scratch size -- but
     * FORMAT_RESIZE without FORMAT_KEEP_ASPECT can't always reach an
     * arbitrary target through the JPEG decoder's own DCT power-of-two
     * downscale steps (1/1, 1/2, 1/4, 1/8) alone; when the requested
     * decode size falls strictly BETWEEN two of those steps (as it did
     * here: 512x240 sits between 640x300 at 1/1 and 320x150 at 1/2),
     * the decoder has to decode the FULL SOURCE resolution as an
     * intermediate buffer before its own resize pass can shrink that
     * down to decode_w x decode_h -- read_jpeg_file() failed outright
     * (ret <= 0) rather than silently corrupting anything, so this was
     * caught, not a repeat of M-033's crash. Fix: budget against
     * whichever of decode/source pixel counts is larger, and actually
     * reserve JPEG_DECODE_OVERHEAD (apps/recorder/jpeg_load.h) instead
     * of assuming it fits in whatever slack happened to be left over. */
    {
        long decode_px = (long)(*decode_w) * (*decode_h);
        long src_px = (long)src_w * src_h;
        long check_px = (decode_px > src_px) ? decode_px : src_px;
        long budget_bytes = (long)METRO_PHOTO_VIEW_SCRATCH_SIZE - (long)JPEG_DECODE_OVERHEAD;

        if (budget_bytes < 0)
            budget_bytes = 0;

        if (check_px * (long)sizeof(fb_data) > budget_bytes)
        {
            long budget_px = budget_bytes / (long)sizeof(fb_data);
            long num = budget_px << 16, den = check_px;
            long mem_scale2 = num / den; /* factor^2, Q16.16 */
            long lo = 0, hi = 1L << 16, mem_scale;
            while (lo < hi)
            {
                long mid = (lo + hi + 1) / 2;
                if ((mid * mid) >> 16 <= mem_scale2)
                    lo = mid;
                else
                    hi = mid - 1;
            }
            mem_scale = lo;
            *decode_w = (int)(((long)(*decode_w) * mem_scale) >> 16);
            *decode_h = (int)(((long)(*decode_h) * mem_scale) >> 16);
            if (*decode_w < 1) *decode_w = 1;
            if (*decode_h < 1) *decode_h = 1;
        }
    }
}

/* Nearest-neighbour blit, ported from Aura-Firmware's
 * draw_scaled_centered() (aura_photos.c, read as reference per DD-10):
 * `display_w`x`display_h` is the TRUE final on-screen size (cover mode
 * can exceed the decode buffer's own size, DD-9-class memory clamp
 * above) -- centers and crops against the screen when larger, centers
 * and leaves the surrounding metro_color_bg() showing when smaller
 * (fit mode's letterbox bars, already painted by metro_draw_clear()
 * before this runs). 1:1 fast path with lcd_bitmap_part() when no
 * resampling is actually needed. */
static void draw_scaled_centered(const fb_data *src, int src_w, int src_h,
                                  int display_w, int display_h)
{
    int display_x0 = (display_w > LCD_WIDTH) ? (display_w - LCD_WIDTH) / 2 : 0;
    int display_y0 = (display_h > LCD_HEIGHT) ? (display_h - LCD_HEIGHT) / 2 : 0;
    int screen_x0 = (display_w < LCD_WIDTH) ? (LCD_WIDTH - display_w) / 2 : 0;
    int screen_y0 = (display_h < LCD_HEIGHT) ? (LCD_HEIGHT - display_h) / 2 : 0;
    int draw_w = (display_w < LCD_WIDTH) ? display_w : LCD_WIDTH;
    int draw_h = (display_h < LCD_HEIGHT) ? display_h : LCD_HEIGHT;
    int x, y;

    if (src_w == display_w && src_h == display_h)
    {
        lcd_bitmap_part(src, display_x0, display_y0, src_w, screen_x0, screen_y0, draw_w, draw_h);
        return;
    }

    for (y = 0; y < draw_h; y++)
    {
        int dy = display_y0 + y;
        int sy = dy * src_h / display_h;
        const fb_data *srow;
        fb_data *drow = FBADDR(screen_x0, screen_y0 + y);

        if (sy >= src_h) sy = src_h - 1;
        srow = src + (long)sy * src_w;
        for (x = 0; x < draw_w; x++)
        {
            int dx = display_x0 + x;
            int sx = dx * src_w / display_w;
            if (sx >= src_w) sx = src_w - 1;
            drow[x] = srow[sx];
        }
    }
}

/* M-109 (plan maestro SS C.2): vista previa INSTANTANEA mientras la
 * rueda todavia se esta moviendo -- la maestra compartida de 80 px
 * (contrato v16/M-097, la MISMA que ya llena la cuadricula) ampliada
 * 3x a 240x240 (llena el alto de pantalla, centrada) con el nombre y
 * la posicion debajo. Nunca decodifica un JPEG: si la maestra todavia
 * no existe (biblioteca grande, el constructor en segundo plano no ha
 * llegado a esta foto todavia) se ve solo el texto sobre el fondo
 * limpio -- preferible a forzar un decode que reintroduciria el
 * bloqueo que este mecanismo existe para evitar.
 *
 * draw_scaled_centered() ya sabe centrar y ampliar por muestreo
 * vecino-mas-cercano (es la misma funcion que dibuja el modo
 * "cubrir" de la foto completa) -- se reusa tal cual, sin una segunda
 * primitiva de escalado. */
#define METRO_PHOTO_PREVIEW_SIDE (LCD_HEIGHT) /* 240: llena el alto, dejando aire a los lados */

/* M-109: franja de fondo SOLIDO al pie, pintada explicitamente con
 * lcd_fillrect() antes del texto -- MISMO patron y MISMO motivo que
 * la caption de un tile en metro_draw.c (draw_tile_caption(),
 * METRO_TILE_CAPTION_H): el CLAUDE.md prohibe conseguir un fondo
 * opaco detras de texto via DRMODE_SOLID (M-051), asi que cualquier
 * sitio que de verdad necesite texto legible sobre una imagen lo pinta
 * a mano. Aqui hace falta ademas por una razon propia: la vista previa
 * ya llena los 240 px de alto (METRO_PHOTO_PREVIEW_SIDE == LCD_HEIGHT),
 * asi que no queda aire debajo donde dibujar el nombre/indice -- tiene
 * que ir SOBRE la imagen, no despues de ella. */
#define METRO_PHOTO_PREVIEW_CAPTION_H 40

static void draw_preview_caption(void)
{
    char buf[METRO_FSUTIL_NAME_LEN + 16];
    int w, h, y;

    y = LCD_HEIGHT - METRO_PHOTO_PREVIEW_CAPTION_H;
    lcd_set_foreground(metro_color_bg());
    lcd_fillrect(0, y, LCD_WIDTH, METRO_PHOTO_PREVIEW_CAPTION_H);

    snprintf(buf, sizeof(buf), "%s", s_items[s_index].filename);
    lcd_setfont(metro_font_id(MFONT_CAPTION));
    lcd_getstringsize((const unsigned char *)buf, &w, &h);
    metro_draw_text_cut_right(MFONT_CAPTION, (LCD_WIDTH - w) / 2 > 0 ? (LCD_WIDTH - w) / 2 : 4,
                              y + 4, buf, metro_color_fg(), LCD_WIDTH - 8);

    snprintf(buf, sizeof(buf), "%d / %d", s_index + 1, s_count);
    lcd_getstringsize((const unsigned char *)buf, &w, &h);
    metro_draw_text(MFONT_CAPTION, (LCD_WIDTH - w) / 2, y + 4 + h + 2,
                     buf, metro_color_secondary());
}

static void draw_scrub_preview(void)
{
    char key[METRO_MASTER_ART_KEY_LEN];
    fb_data *master = metro_master_art_scratch();
    bool have_master;

    metro_photos_master_key(s_items[s_index].filename, s_items[s_index].mtime,
                            key, sizeof(key));

    /* Lee y DIBUJA bajo el mismo lock: metro_master_art_scratch() es un
     * unico buffer compartido con el hilo del constructor (M-097) --
     * valido solo mientras el lock esta tomado, igual que cualquier
     * otro consumidor de la maestra (ver metro_albumart.c). */
    metro_master_art_lock();
    have_master = (metro_master_art_probe("photos", key) == METRO_MASTER_ART_PRESENT) &&
                  metro_master_art_read("photos", key, master, METRO_MASTER_ART_PHOTO_PX);
    if (have_master)
        draw_scaled_centered(master, METRO_MASTER_ART_PHOTO_PX, METRO_MASTER_ART_PHOTO_PX,
                             METRO_PHOTO_PREVIEW_SIDE, METRO_PHOTO_PREVIEW_SIDE);
    metro_master_art_unlock();

    draw_preview_caption();
    lcd_update();
}

static void probe_current(void)
{
    char path[MAX_PATH];

    if (s_probed_index == s_index)
        return;
    s_probed_index = s_index;

    snprintf(path, sizeof(path), "%s/%s", PHOTOS_DIR, s_items[s_index].filename);
    s_probe_ok = (probe_jpeg_dimensions(path, &s_probed_w, &s_probed_h) == METRO_JPEG_PROBE_BASELINE);
}

static void load_current(void)
{
    char path[MAX_PATH];
    int format;
    int ret;

    if (s_loaded_index == s_index)
        return;
    s_loaded_index = s_index;
    s_loaded_ok = false;

    snprintf(path, sizeof(path), "%s/%s", PHOTOS_DIR, s_items[s_index].filename);

    s_bm.data = (char *)s_scratch;
#if (LCD_DEPTH > 1)
    s_bm.maskdata = NULL;
#endif

    /* Cover mode needs the probed source size to compute the exact
     * decode/display split above; without it (probe failed) fall back
     * to fit regardless of s_cover_mode -- no source dimensions to
     * cover-crop against. */
    if (s_cover_mode && s_probe_ok)
    {
        int decode_w, decode_h;
        compute_decode_and_display_size(s_probed_w, s_probed_h, &decode_w, &decode_h,
                                         &s_display_w, &s_display_h);
        s_bm.width = decode_w;
        s_bm.height = decode_h;
        format = FORMAT_NATIVE | FORMAT_RESIZE;
    }
    else
    {
        s_bm.width = LCD_WIDTH;
        s_bm.height = LCD_HEIGHT;
        format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
    }

    /* M-097: read_jpeg_file() keeps its decoder in one static; the
     * background master builder may be inside it on its own thread. */
    metro_master_art_lock();
    ret = read_jpeg_file(path, &s_bm, sizeof(s_scratch), format, NULL);
    metro_master_art_unlock();
    s_loaded_ok = (ret > 0);
    if (s_loaded_ok && !(s_cover_mode && s_probe_ok))
    {
        /* Ajustar: the decoder already landed on the exact final size
         * (KEEP_ASPECT) -- decode and display are the same thing. */
        s_display_w = s_bm.width;
        s_display_h = s_bm.height;
    }
}

static void draw_centered_message(enum metro_lang_id id)
{
    const char *text = metro_lang_str(id);
    int w, h;

    lcd_setfont(metro_font_id(MFONT_CAPTION));
    lcd_getstringsize((const unsigned char *)text, &w, &h);
    metro_draw_text(MFONT_CAPTION, (LCD_WIDTH - w) / 2, (LCD_HEIGHT - h) / 2,
                     text, metro_color_fg());
}

/* --- sentinel page: same pattern as metro_screen_nowplaying.c's --
 * never drawn/queried through the generic list path, just a
 * stack-bookkeeping placeholder metro_screen_photo_viewer_is_current()
 * recognizes by pointer. */

static int sentinel_count(void *ctx) { (void)ctx; return 0; }
static void sentinel_get_row(void *ctx, int index, struct metro_row *out)
{ (void)ctx; (void)index; (void)out; }
static void sentinel_on_select(void *ctx, int index) { (void)ctx; (void)index; }

static const struct metro_pivot sentinel_pivots[] = {
    { .name = LANG_HUB_PHOTOS, .count = sentinel_count,
      .get_row = sentinel_get_row, .on_select = sentinel_on_select },
};
static const struct metro_page sentinel_page = { LANG_HUB_PHOTOS, sentinel_pivots, 1, NULL };

bool metro_screen_photo_viewer_push(const metro_photo_item_t *items, int count, int start_index)
{
    if (count <= 0)
        return false;
    if (!metro_screen_list_push(&sentinel_page))
        return false;

    s_items = items;
    s_count = count;
    s_index = start_index;
    if (s_index < 0) s_index = 0;
    if (s_index > count - 1) s_index = count - 1;

    s_loaded_index = -1;
    s_probed_index = -1;
    /* M-109: "ya paso el tiempo de quietud" desde el primer dibujo --
     * abrir el visor debe mostrar la foto de una vez, no la vista
     * previa (que es para SCRUBBING dentro del visor, no para su
     * apertura, que ademas ya tiene su propio fundido, M-106). */
    s_nav_tick = current_tick - METRO_PHOTO_SETTLE_TICKS;
    return true;
}

bool metro_screen_photo_viewer_is_current(void)
{
    return metro_screen_list_current_page() == &sentinel_page;
}

/* M-109: true mientras haya que seguir sondeando -- durante el
 * debounce de 150 ms (metro_main.c baja su espera a HZ/20, igual que
 * ya hace por el hub/la marquesina), y una vuelta MAS despues de que
 * el debounce termina, para que ese ultimo redibujo dispare el decode
 * real. Se apaga sola en cuanto la foto asentada queda decodificada:
 * de ahi en mas no hace falta seguir despertando al bucle principal
 * hasta el proximo evento de rueda. */
bool metro_screen_photo_viewer_wants_ticks(void)
{
    if (s_count == 0)
        return false;
    return (current_tick - s_nav_tick < METRO_PHOTO_SETTLE_TICKS) ||
           (s_loaded_index != s_index);
}

void metro_screen_photo_viewer_show(void)
{
    if (s_count == 0)
        return;

    /* M-109 (plan maestro SS C.1/C.2): mientras la rueda sigue
     * moviendose (o LEFT/RIGHT se siguen apretando), NINGUN indice
     * intermedio se decodifica -- solo se dibuja la vista previa
     * barata del destino ACTUAL. El decode real de una foto ocurre
     * como mucho una vez por gesto, cuando el usuario se detiene. Esto
     * es lo que cierra M-109: antes, cada paso de la rueda disparaba
     * su propio decode sincrono aqui mismo, así que un giro continuo
     * -- incluso si TODOS sus eventos hubieran llegado -- habria
     * seguido sintiendose atascado. */
    if (current_tick - s_nav_tick < METRO_PHOTO_SETTLE_TICKS)
    {
        metro_draw_clear();
        draw_scrub_preview();
        return;
    }

    metro_draw_clear();

    probe_current();

    if (s_loaded_index != s_index)
    {
        bool big = s_probe_ok && (s_probed_w > METRO_PHOTO_LOADING_INDICATOR_SIDE ||
                                   s_probed_h > METRO_PHOTO_LOADING_INDICATOR_SIDE);
        if (big)
        {
            /* Only forced when it will actually be noticeable (see
             * METRO_PHOTO_LOADING_INDICATOR_SIDE's comment) -- an
             * explicit lcd_update() here because load_current() below
             * blocks; the normal draw cycle only flips to the real LCD
             * after this whole function returns. */
            draw_centered_message(LANG_PHOTO_LOADING);
            lcd_update();
        }
        load_current();
    }

    if (!s_loaded_ok)
    {
        draw_centered_message(LANG_PHOTO_UNSUPPORTED);
        lcd_update();
        return;
    }

    draw_scaled_centered((const fb_data *)s_scratch, s_bm.width, s_bm.height,
                          s_display_w, s_display_h);
    lcd_update();
}

int metro_screen_photo_viewer_take_slide(void)
{
    /* M-109: ya NO se arma en handle() -- se calcula aqui, en el
     * momento en que metro_main.c esta a punto de invocar el redibujo
     * que de verdad va a decodificar (mismo criterio que
     * metro_screen_photo_viewer_show() usa para decidir "ya asento").
     * Solo hay deslizamiento la PRIMERA vez que ese redibujo ocurre
     * para un s_index nuevo -- comparar contra s_loaded_index (la foto
     * TODAVIA en pantalla) da la direccion sin necesitar guardar nada
     * en handle(). Devuelve 0 mientras se siga scrubbeando, y 0 otra
     * vez en cualquier llamada posterior una vez que ya asento (
     * s_loaded_index habra alcanzado a s_index). */
    if (s_count == 0)
        return 0;
    if (current_tick - s_nav_tick < METRO_PHOTO_SETTLE_TICKS)
        return 0;
    if (s_loaded_index == s_index)
        return 0;
    return (s_index > s_loaded_index) ? 1 : -1;
}

void metro_screen_photo_viewer_handle(int action, int steps)
{
    switch (action)
    {
        case MACT_PREV:
        {
            int new_index = s_index - steps;
            if (new_index < 0) new_index = 0;
            /* M-109: navegacion ACUMULATIVA -- cada evento SOLO mueve
             * s_index (barato, sin decodificar nada) y reinicia el
             * reloj de quietud. `steps` (la aceleracion de la rueda,
             * ya calculada por metro_input_next()) se sigue aplicando
             * igual que antes; lo que cambia es que un giro rapido ya
             * no dispara un decode por cada paso -- ver
             * metro_screen_photo_viewer_show(). El deslizamiento (si
             * corresponde) se decide en take_slide(), no aqui: en el
             * momento en que este evento se procesa todavia no se sabe
             * si es el ultimo de este gesto.
             *
             * El reloj SOLO se reinicia si el indice de verdad cambio
             * -- igual que el guard que M-106 ya tenia para el
             * deslizamiento. Sin este guard, seguir apretando en el
             * primer o ultimo elemento (clamp sin cambio real)
             * mantendria wants_ticks() en true para siempre y la
             * pantalla se quedaria mostrando la vista previa barata en
             * vez de la foto ya asentada, aunque nada este cambiando. */
            if (new_index != s_index)
                s_nav_tick = current_tick;
            s_index = new_index;
            break;
        }
        case MACT_NEXT:
        {
            int new_index = s_index + steps;
            if (new_index > s_count - 1) new_index = s_count - 1;
            if (new_index != s_index)
                s_nav_tick = current_tick;
            s_index = new_index;
            break;
        }
        case MACT_TOGGLE_VIEW_MODE:
            s_cover_mode = !s_cover_mode;
            s_loaded_index = -1; /* DD-10: re-decode, never resample a stale buffer */
            break;
        case MACT_BACK:
            metro_screen_list_pop();
            /* M-106: la cuadricula queda sobre la foto que se estaba
             * viendo, no sobre la que se abrio. Va DESPUES del pop
             * porque hasta entonces el pivot "actual" es el centinela
             * del visor, no la cuadricula. Si el usuario recorrio
             * veinte fotos, volver al principio seria perder su
             * lugar. */
            metro_screen_list_set_sel(s_index);
            break;
        case MACT_HOME:
            metro_screen_list_pop_to_root();
            break;
        case MACT_PLAYPAUSE:
            /* La música sigue sonando con el visor abierto (DD-10); el
             * toggle es el compartido de metro_music.c (R4/FA-8). */
            metro_music_playpause();
            break;
        default:
            break;
    }
}

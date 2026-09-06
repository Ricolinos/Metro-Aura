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
 * Metro UI -- replacement UI layer for this Rockbox fork (see
 * MODIFICATIONS.md, DECISIONS.md M-006 in the repository root).
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
#include <stdbool.h>
/* config.h before tagcache.h -- see DECISIONS.md M-030. */
#include "config.h"
#include "tagcache.h"
#include "kernel.h"
#include "button.h"
#include "lcd.h"
#include "misc.h"
#include "settings.h"
#include "statusbar.h"
#include "backlight.h" /* lcd_active() -- M-123: puerta del carril animado */

#include "metro_main.h"
#include "metro_screen_splash.h"
#include "metro_screen_usb.h"
#include "metro_fonts.h"
#include "metro_theme.h"
#include "metro_lang.h"
#include "metro_draw.h"
#include "metro_screen_list.h"
#include "metro_screen_hub.h"
#include "metro_screen_nowplaying.h"
#include "metro_input.h"
#include "metro_keymap.h"
#include "metro_settings.h"
#include <stdio.h> /* M-100: snprintf */
#include "metro_sync.h"
#include "metro_device.h"
#include "metro_manifest.h"
#include "metro_transitions.h"
#include "metro_thumbs.h"
#include "metro_music.h" /* metro_music_db_ready() -- M-098 */
#include "metro_master_art.h"         /* M-097 */
#include "metro_master_art_builder.h" /* M-097 */
#include "metro_marquee.h"           /* M-106: puerta de cuadros */
#include "metro_screen_photo_viewer.h"
#include "metro_screen_lock.h"

/* See metro_main.h for why this must be called from apps/main.c's
 * init(), not from here. None of these settings are exposed anywhere
 * in the Metro UI (yet), so forcing them is the only way to guarantee
 * the behaviour regardless of what a previous install left in the
 * on-disk config file. */
void metro_apply_hygiene(void)
{
    global_settings.statusbar = STATUSBAR_OFF;
    /* R5 (M-088): Rockbox's USB "keypad mode" (HID) has no place in
     * Metro -- and with it on, gui_usb_screen_run() takes the HID
     * branch of its loop, which never reaches Metro's animation tick. */
#ifdef USB_ENABLE_HID /* not defined in the simulator build */
    global_settings.usb_hid = false;
#endif
    global_settings.backdrop_file[0] = '-';
    global_settings.backdrop_file[1] = '\0';
    global_settings.show_shutdown_message = false;
    global_settings.talk_menu = false;
    global_settings.clear_settings_on_hold = false;
    global_settings.tagcache_ram = true;
    /* R3-F4/DD-4 (M-065): Quickplay needs tag_lastplayed, and Rockbox
     * never writes it unless this is on -- default is false, and
     * Metro has no menu of its own to expose it (INVESTIGACION-metro-r3.md
     * D.1: the writers, tagtree_buffer_event()/tagtree_track_finish_event(),
     * are already registered unconditionally from apps/main.c's own
     * tagtree_init() -- only the flag gating them was ever missing).
     * Purely local (the device's own playback history, on its own
     * disk, never sent anywhere) -- same "decide it for the user"
     * class as every other setting forced here. */
    global_settings.runtimedb = true;
    /* M-103: `keyclick` ya NO se fuerza aqui. M-008 lo apagaba en cada
     * arranque porque no habia forma de encenderlo -- no existia la
     * fila. Ahora existe (Ajustes > general > clicker) y persiste en
     * config.cfg, asi que forzarlo seria pisar en cada arranque lo que
     * el usuario acaba de elegir. El default sigue siendo apagado: lo
     * pone `apps/settings_list.c` (CHOICE_SETTING ... default 0) la
     * primera vez que se escribe config.cfg, que es exactamente donde
     * corresponde. Lo mismo vale para `poweroff`, que esta fase expone
     * y que nunca se forzo aqui (su default de Rockbox es 10 min). */
#ifdef USB_ENABLE_HID
    global_settings.usb_hid = false;
#endif
    /* Contract v15 (M-095/M-096): this is the one point after
     * settings_load() (which would restore the on-disk "database path")
     * and before init_tagcache() (which copies it into tc_stat.db_path
     * for good) -- exactly where the shared database path must land.
     * The thumbnail cache has no such ordering constraint; it just
     * rides the same one-shot boot hook. */
    metro_force_shared_db_path();
    metro_settings_migrate_shared_thumbs();
}

static void redraw_current(void)
{
    if (metro_nav_is_root(metro_screen_nav()))
        metro_screen_hub_show();
    else if (metro_screen_nowplaying_is_current())
        metro_screen_nowplaying_show();
    else if (metro_screen_photo_viewer_is_current())
        metro_screen_photo_viewer_show();
    else
        metro_screen_list_show();
}

/* M-109: redibuja el visor de fotos consultando si ESTE es el
 * redibujo que asienta un cambio de foto (metro_screen_photo_viewer_take_slide()
 * devuelve la dirección solo esa vez, y 0 en cualquier otro caso --
 * scrubbing en curso, o ya asentado y sin nada nuevo). Un solo sitio
 * para el mismo patrón que antes vivía inline solo en el despacho de
 * acciones: ahora también hace falta desde el sondeo periódico
 * (metro_screen_photo_viewer_wants_ticks()), porque asentar ocurre
 * por el paso del RELOJ, no por una acción nueva. */
static void redraw_viewer_settled(void)
{
    int dir = metro_screen_photo_viewer_take_slide();

    if (dir != 0)
        metro_transitions_slide_fast(redraw_current, dir);
    else
        redraw_current();
}

/* M-123: geometria de la pantalla de espera. Un solo bloque: titulo,
 * fase, barra, y la pista de MENU abajo (donde ya vivia la de descartar
 * un error). */
#define SYNC_TITLE_Y   84
#define SYNC_PHASE_Y   124
#define SYNC_BAR_Y     152
#define SYNC_BAR_H     4
#define SYNC_PCT_Y     162
#define SYNC_HINT_Y    206
#define SYNC_MARGIN_X  12
#define SYNC_BAR_W     (LCD_WIDTH - 2 * SYNC_MARGIN_X)

/* M-123: carril sin relleno determinado -- se usa cuando la fase corre
 * pero todavia no publica ninguna cifra.
 *
 * El bloque se MUEVE solo bajo las dos puertas de siempre:
 * `lcd_active()` y el nivel de animacion. Con la pantalla dormida el
 * trabajo sigue igual, asi que animar seria gastar cuadros que nadie
 * ve; con animations=off el bloque se queda quieto a la izquierda en
 * vez de desaparecer, porque un carril vacio diria que no pasa nada y
 * si esta pasando. */
#define SYNC_IND_W (SYNC_BAR_W / 4)

static void draw_indeterminate_bar(void)
{
    int x = SYNC_MARGIN_X;

    lcd_set_foreground(metro_color_tertiary());
    lcd_fillrect(SYNC_MARGIN_X, SYNC_BAR_Y, SYNC_BAR_W, SYNC_BAR_H);

    if (lcd_active() && metro_settings.animations != METRO_ANIM_OFF)
    {
        int span = SYNC_BAR_W - SYNC_IND_W;
        int period = 2 * span; /* ida y vuelta, sin flotantes */
        int t = span > 0 ? (int)((current_tick * 2) % period) : 0;

        x += (t < span) ? t : (period - t);
    }

    lcd_set_foreground(metro_color_accent());
    lcd_fillrect(x, SYNC_BAR_Y, SYNC_IND_W, SYNC_BAR_H);
}

/* M-123: la fase de base de datos, en DOS TRAMOS -- porte del D-344 de
 * Aura (`aura_sync.c`, leido solo como referencia), traido tras el aviso
 * de que en el iPod del dueno esta fase dura unos cuatro minutos.
 *
 * El primer intento aqui usaba `total_entries` como denominador y la
 * barra se quedaba clavada en 0 toda la fase: ese campo vale 0 mientras
 * tagcache recorre el disco por primera vez, que es justo la parte
 * larga. Lo que si avanza son otras dos cosas, de naturaleza distinta:
 *
 *   - ESCANEO: `stat->progress`, un porcentaje ESTIMADO (0 al principio
 *     y con la cache de directorios fria). Se le da el primer tramo,
 *     0..200 de 256.
 *   - INDEXADO (commit): `commit_step` sobre
 *     `tagcache_get_max_commit_step()`, que si es una cuenta exacta.
 *     Segundo tramo, 200..256.
 *
 * La escala 0..256 es la de Aura y se conserva para que los dos
 * firmwares repartan el recorrido igual; aqui se convierte a porcentaje
 * al dibujar.
 *
 * El DETALLE no repite el porcentaje del escaneo, que es una estimacion
 * y se queda quieta a ratos, sino el conteo real de entradas ya vistas
 * -- un numero que avanza a ojo aunque la barra no se mueva. En una
 * fase de cuatro minutos es lo unico que distingue "va lento" de "se
 * colgo".
 *
 * Devuelve el avance en 0..256, o -1 si todavia no hay nada que decir. */
#define SYNC_DB_SCAN_SPAN 200
#define SYNC_DB_FULL_SPAN 256

static int sync_db_progress_256(char *detail, size_t detail_len)
{
    struct tagcache_stat *st = tagcache_get_stat();
    int max_step, pct;

    if (detail && detail_len)
        detail[0] = '\0';
    if (!st)
        return -1;

    max_step = tagcache_get_max_commit_step();
    if (st->commit_step > 0 && max_step > 0)
    {
        if (detail && detail_len)
            snprintf(detail, detail_len, metro_lang_str(LANG_SYNC_DB_INDEX),
                     st->commit_step, max_step);
        return SYNC_DB_SCAN_SPAN
             + ((SYNC_DB_FULL_SPAN - SYNC_DB_SCAN_SPAN) * st->commit_step) / max_step;
    }

    /* El conteo se rellena SIEMPRE, aunque no haya barra que dibujar:
     * son dos cosas distintas y se desacoplan a proposito (el
     * porcentaje es la estimacion de tagcache; el contador, archivos
     * ya procesados de verdad). */
    pct = st->progress;
    if (detail && detail_len)
        snprintf(detail, detail_len, metro_lang_str(LANG_SYNC_DB_SCAN),
                 st->processed_entries);

    /* `progress` NEGATIVO es "no se sabe", no "cero": tagcache lo usa
     * cuando todavia no puede estimar. Se devuelve -1 para que el
     * llamador deje el carril sin relleno en vez de dibujar un 0 % que
     * afirma algo falso -- el contador de arriba sigue moviendose y es
     * el que dice que hay vida. */
    if (pct < 0)
        return -1;
    if (pct > 100) pct = 100;
    if (pct == 0 && st->processed_entries == 0)
        return -1;
    return (SYNC_DB_SCAN_SPAN * pct) / 100;
}

/* F6: the one full-screen wait state in Metro (PLAN_MAESTRO.md S4.3) --
 * drawn straight from metro_main.c, not a metro_screen_* module, same
 * as the plan's own file list for this phase (no new screen file).
 * MENU postpones a running job (metro_sync_postpone(), the job keeps
 * going in the background) or dismisses an error (metro_sync_dismiss()) --
 * either way the loop below exits as soon as metro_sync_needs_screen()
 * goes false.
 *
 * M-123: hasta aqui la pantalla decia "actualizando biblioteca..." y
 * nada mas -- ni en que fase iba ni cuanto faltaba, asi que una
 * biblioteca grande se veia igual que una colgada. Ahora nombra la fase
 * (base de datos -> caratulas -> fotos de artista -> imagenes), dibuja
 * una barra determinada con su cifra y su porcentaje cuando la fase
 * publica una, y el carril con el bloque cuando todavia no. */
static void draw_sync_screen(void)
{
    enum metro_lang_id msg = LANG_MUSIC_DB_UPDATING;
    bool is_error = false;

    switch (metro_sync_state())
    {
        case METRO_SYNC_ERROR_VERSION:
            msg = LANG_SYNC_ERROR_VERSION;
            is_error = true;
            break;
        case METRO_SYNC_ERROR_ATTEMPTS:
            msg = LANG_SYNC_ERROR_ATTEMPTS;
            is_error = true;
            break;
        default:
            break;
    }

    metro_draw_clear();
    metro_draw_header("");
    metro_draw_text_cut_right(MFONT_TITLE, SYNC_MARGIN_X, SYNC_TITLE_Y,
                              metro_lang_str(msg), metro_color_fg(),
                              LCD_WIDTH - 2 * SYNC_MARGIN_X);
    if (is_error)
    {
        metro_draw_text(MFONT_CAPTION, SYNC_MARGIN_X, SYNC_HINT_Y,
                         metro_lang_str(LANG_SYNC_DISMISS_HINT),
                         metro_color_secondary());
        lcd_update();
        return;
    }

    {
        /* M-100: the image phase shares this screen -- it is part of
         * "preparing the library", not a second wait state (S4.3 says
         * there is exactly one). */
        metro_master_art_phase_t phase = METRO_MASTER_ART_PHASE_IDLE;
        int done = 0, total = 0;
        int pct = -1;
        char line[64];

        /* M-123: la linea de fase SIEMPRE se arma con snprintf sobre la
         * cadena del catalogo. Ninguna de estas lleva su cifra ya
         * puesta, asi que dibujar la cadena cruda enseñaria el "%d" al
         * usuario -- que es exactamente lo que le paso a moonlit. */
        if (metro_sync_art_progress(&phase, &done, &total))
        {
            if (phase == METRO_MASTER_ART_PHASE_PHOTOS)
            {
                /* Recorrido en flujo: hay conteo, no hay total. */
                snprintf(line, sizeof(line),
                         metro_lang_str(LANG_SYNC_ART_PHOTOS), done);
            }
            else
            {
                snprintf(line, sizeof(line),
                         metro_lang_str(phase == METRO_MASTER_ART_PHASE_ARTISTS
                                          ? LANG_SYNC_ART_ARTISTS
                                          : LANG_SYNC_ART_ALBUMS),
                         done, total);
                if (total > 0)
                {
                    pct = (int)(((long)done * 100) / total);
                    if (pct > 100) pct = 100;
                }
            }
        }
        else
        {
            int p256 = sync_db_progress_256(line, sizeof(line));

            if (!line[0])
                snprintf(line, sizeof(line), "%s",
                         metro_lang_str(LANG_SYNC_DB_BUSY));
            if (p256 >= 0)
                pct = (p256 * 100) / SYNC_DB_FULL_SPAN;
        }

        metro_draw_text_cut_right(MFONT_CAPTION, SYNC_MARGIN_X, SYNC_PHASE_Y,
                                  line, metro_color_secondary(),
                                  LCD_WIDTH - 2 * SYNC_MARGIN_X);

        if (pct >= 0)
        {
            char pctbuf[8];
            int w;

            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            metro_draw_progress(SYNC_MARGIN_X, SYNC_BAR_Y, SYNC_BAR_W,
                                 SYNC_BAR_H, pct);
            snprintf(pctbuf, sizeof(pctbuf), "%d%%", pct);
            metro_draw_text_size(MFONT_CAPTION, pctbuf, &w, NULL);
            metro_draw_text(MFONT_CAPTION, LCD_WIDTH - SYNC_MARGIN_X - w,
                             SYNC_PCT_Y, pctbuf, metro_color_secondary());
        }
        else
        {
            draw_indeterminate_bar();
        }

        metro_draw_text(MFONT_CAPTION, SYNC_MARGIN_X, SYNC_HINT_Y,
                         metro_lang_str(LANG_SYNC_POSTPONE_HINT),
                         metro_color_secondary());
    }
    lcd_update();
}

/* F9: with show_shutdown_message=false (M-019), Rockbox's own
 * "Shutting down..." splash never runs at all -- without this, the
 * screen would just go dark with zero feedback on power-off. Drawn
 * before default_event_handler(), which proceeds to actually power
 * down right after -- unlike the USB case (metro_screen_usb_show(),
 * DESVIACIONES.md F9-1), nothing else ever draws over this one, so it
 * really is the last thing shown. */
static void draw_shutdown_screen(void)
{
    const char *text = metro_lang_str(LANG_SHUTTING_DOWN);
    int w, h;

    metro_draw_clear();
    metro_draw_text_size(MFONT_TITLE, text, &w, &h); /* M-117 */
    metro_draw_text(MFONT_TITLE, (LCD_WIDTH - w) / 2, (LCD_HEIGHT - h) / 2,
                     text, metro_color_fg());
    lcd_update();
}

void metro_run_sync_screen_if_needed(void)
{
    if (!metro_sync_needs_screen())
        return;

    draw_sync_screen();

    while (metro_sync_needs_screen())
    {
        int action = metro_input_next(MCTX_DIALOG, HZ / 10, NULL);

        if (action & SYS_EVENT)
        {
            default_event_handler(action);
            continue;
        }

        if (action == MACT_BACK)
        {
            if (metro_sync_state() == METRO_SYNC_ERROR_VERSION ||
                metro_sync_state() == METRO_SYNC_ERROR_ATTEMPTS)
                metro_sync_dismiss();
            else
                metro_sync_postpone();
            continue;
        }

        /* M-123: se repinta en CADA vuelta del bucle (HZ/10, unas diez
         * veces por segundo), no solo cuando metro_sync_tick() dice que
         * algo cambio. El avance de tagcache no pasa por tick -- lo
         * publica el propio tagcache en su `stat` -- asi que atarse a
         * tick dejaba la cifra congelada toda la fase.
         *
         * Repintar por VUELTA y no por ELEMENTO es lo que evita competir
         * con el constructor de maestras: el ritmo lo pone la espera de
         * metro_input_next(), no cuantas caratulas se hayan preparado
         * entre dos cuadros. */
        metro_sync_tick();
        draw_sync_screen();
    }
}

/* Boot, and every return from the USB screen -- the only two moments
 * the firmware ever recovers the disk (PLAN_MAESTRO.md S1.2). */
static void metro_disk_handoff(void)
{
    metro_settings_apply_pending_clock();
    /* M-110 (contrato v19 SS A.2.2): mismo punto que la hora -- si
     * /.aura/settings.cfg trae una rev mas nueva que la que este
     * arbol ya aplico, se aplica ahora. */
    metro_settings_shared_apply_pending();
    /* R2-F1/DD-4 (M-054): a fresh disk (first boot, or a USB session
     * that just mounted a different volume) may not have any of the
     * four media folders yet -- ensure they exist before anything
     * else in this handoff (sync, device reload) tries to read from
     * them. */
    metro_ensure_media_dirs();
    metro_device_reload();
    metro_manifest_reload(); /* R5-F1 (M-081): About reads the RAM copy */
    metro_sync_check_pending();
    metro_run_sync_screen_if_needed();
}

/* F9: the splash's progress bar (S1.4) covers tagcache's initial "is
 * there already a usable database on disk" determination -- fast
 * (~1s, D-206-class async check) regardless of library size, NOT a
 * full rebuild/scan (that can take much longer for a real library and
 * has its own screen, F6's metro_run_sync_screen_if_needed() -- the
 * splash must never block boot on that). Capped so a wedged tagcache
 * thread can never hang the splash forever. */
#define METRO_SPLASH_MAX_WAIT_TICKS (HZ * 5)

static void wait_for_tagcache_with_splash(void)
{
    long start = current_tick;

    metro_screen_splash_progress(0);
    while (!tagcache_is_fully_initialized())
    {
        long elapsed = current_tick - start;
        if (elapsed >= METRO_SPLASH_MAX_WAIT_TICKS)
            break;
        metro_screen_splash_progress((int)(elapsed * 100 / METRO_SPLASH_MAX_WAIT_TICKS));
        sleep(HZ / 10);
    }
    metro_screen_splash_progress(100);
}

void metro_main(void)
{
    long last_player_tick = 0;
    bool index_letter_was_pending = false;
    long last_hub_tick = 0; /* R5-F5 */
    long last_dbready_tick = 0; /* M-098 */

    /* metro_apply_hygiene() already ran inside init() (apps/main.c) --
     * see metro_main.h for why it can't run here, after init() returns. */
    metro_settings_load();
    metro_master_art_init(); /* M-097: the decode/tagcache lock, before any user */
    metro_fonts_init();
    /* R2-F1/DD-1 (M-051): DRMODE_FG is the drawmode every apps/metro/
     * text draw expects -- metro_draw_text()/metro_draw_text_cut_right()
     * also set it per call, but the LCD starts in Rockbox's default
     * DRMODE_SOLID and nothing has drawn text yet at this point (the
     * splash screen is next), so set the baseline here too.
     * metro_photos.c and metro_video.c re-set it after their
     * plugin_load() calls return, since imageviewer/mpegplayer are
     * free to leave the LCD in DRMODE_SOLID behind them. */
    lcd_set_drawmode(DRMODE_FG);
    metro_theme_init();
    /* metro_theme_init()/metro_lang.c's own module-level statics set
     * the compiled defaults; applying the loaded settings right after
     * is what makes them the actual starting values -- one place,
     * F8 does the same thing again whenever a Settings row changes one
     * live. Fonts/theme both have to be ready BEFORE the first splash
     * draw (F9: the real splash uses MFONT_DISPLAY/metro_color_fg(),
     * unlike F1's placeholder, which drew before either existed with
     * raw FONT_SYSFIXED). */
    metro_theme_set(metro_settings.theme);
    metro_accent_set(metro_settings.accent);
    metro_lang_set(metro_settings.language);

    /* M-112 (hallazgo de moonlit D-079, aplicado aquí): el candado se
     * cobra un poco más abajo, ANTES del resto de metro_disk_handoff()
     * -- así que si /.aura/settings.cfg trae screen_lock_enabled: 0
     * (la salida de emergencia por USB documentada: conectar y editar
     * el archivo compartido) hay que aplicarlo AQUÍ, antes de
     * metro_screen_lock_run_if_active(), para que ese arranque en frío
     * ya salga desbloqueado -- no uno después, que es como quedó
     * documentado (y aceptado) para el CASO CONTRARIO en M-110 (activar
     * el candado por archivo sí tarda un arranque, porque nada de
     * seguridad se pierde por ese lado). Solo necesita el disco
     * montado (ya lo está, metro_settings_load() arriba mismo ya leyó
     * de él), no tagcache -- no hace falta esperar al splash.
     * metro_disk_handoff() más abajo la vuelve a llamar (para el
     * camino de retorno de USB, que sí la necesita ahí); en el
     * arranque esa segunda llamada no hace nada porque `rev` ya quedó
     * aplicada aquí. */
    metro_settings_shared_apply_pending();

    metro_screen_splash_show();
    wait_for_tagcache_with_splash();

    metro_screen_list_init();

    /* R3-F7/DD-8 (M-068): el candado se arma aquí y se cobra AQUÍ, antes
     * de metro_disk_handoff() -- que puede levantar la pantalla de
     * "actualizando biblioteca" (metro_run_sync_screen_if_needed()) y
     * dejarla visible por encima del candado. Todo lo que esta pantalla
     * necesita ya está listo a esta altura (fuentes, tema, idioma); lo
     * único que corrió antes es el splash. Al desbloquear, el arranque
     * sigue normal y el handoff recoge cualquier marcador que Studio
     * hubiera dejado. */
    metro_screen_lock_init();
    metro_screen_lock_run_if_active();

    metro_disk_handoff();

    /* M-102 (contract v18): if the derived caches on this disk were
     * built by rules older than this firmware's, they go now -- BEFORE
     * the builder thread exists, so nothing is writing masters into a
     * directory that is about to be emptied. It is the only moment in
     * the boot where that is true, and it is why this call sits between
     * the handoff and the builder instead of riding
     * metro_apply_hygiene() with the rest of the one-shot disk work. */
    metro_master_art_check_format_version();

    /* M-097 (contract v16): the shared master art cache builds itself
     * in the background from here on -- no screen, low priority, idle
     * only (metro_master_art_builder.h). */
    metro_master_art_builder_init();

    /* F3: the twist navigation core supersedes the F2 type/palette
     * specimen as the running UI (metro_screen_specimen.c stays in
     * the tree as a visual regression reference, just unused here --
     * see docs/DESVIACIONES.md F2-1). */
    redraw_current();

    while (1)
    {
        bool at_root;
        bool at_player;
        bool at_viewer;

        /* R3-F7/DD-8 (M-068): la interceptación, antes de CUALQUIER
         * despacho de pantalla -- eso es lo que hace que el candado
         * alcance a todo el aparato y no solo a una pantalla. No-op
         * mientras el estado no sea ACTIVE (el caso normal: ya se cobró
         * arriba, en el arranque), así que no cuesta nada por vuelta.
         * Sigue aquí igual, y no solo en el arranque, porque es esta
         * línea -- no el orden de las llamadas de más arriba -- la que
         * vuelve estructuralmente cierto que ninguna otra pantalla es
         * alcanzable con el candado puesto. */
        metro_screen_lock_run_if_active();

        /* M-104 (plan maestro SS D): el interruptor Hold NO genera
         * eventos de boton en el 6G -- se lee por sondeo. Una lectura
         * por vuelta, aqui, cubre las dos cosas que dependen de el: el
         * icono de candado de la barra (que si no, aparecia recien la
         * proxima vez que algo mas provocara un redibujo) y la maquina
         * de estados del bloqueo. Va DESPUES de run_if_active() a
         * proposito: si el flanco de subida acaba de dejar el estado en
         * ACTIVE, la vuelta siguiente lo cobra por el camino de
         * siempre, sin duplicar la pantalla de codigo aqui. */
        if (metro_screen_lock_poll_hold())
            redraw_current();

        at_root = metro_nav_is_root(metro_screen_nav());
        at_player = !at_root && metro_screen_nowplaying_is_current();
        /* R2-F3: mutually exclusive with at_player -- only one sentinel
         * page can be current at a time (metro_screen_photo_viewer.h). */
        at_viewer = !at_root && metro_screen_photo_viewer_is_current();
        enum metro_context ctx = at_root ? MCTX_HUB
                                          : (at_player ? MCTX_PLAYER
                                                        : (at_viewer ? MCTX_VIEWER : MCTX_LIST));
        int steps = 1;
        /* R5-F5 (M-085): espera más corta mientras la fila
         * "reproduciendo" del hub se anima, para que el tick llegue a
         * ~20 Hz; el resto del tiempo, la de siempre. */
        /* M-106: misma cadencia mientras una marquesina esté
         * desplazando. Se apaga sola en cuanto el texto cabe, el LCD se
         * duerme o las animaciones están apagadas -- la puerta la
         * decide metro_marquee_draw() en cada dibujo, no una pantalla
         * declarando "yo animo".
         * M-109: y mientras el visor de fotos esté en su ventana de
         * quietud (scrubbing) -- sin esto, un giro rápido de rueda
         * seguiría notándose recién 100 ms después de que el usuario
         * se detiene, no 150 ms; la espera más corta es lo que hace
         * que el debounce de la vista previa se sienta al tacto y no
         * a tirones. */
        int action = metro_input_next(ctx,
                                      ((at_root && metro_screen_hub_wants_ticks()) ||
                                       metro_marquee_wants_ticks() ||
                                       (at_viewer && metro_screen_photo_viewer_wants_ticks()))
                                          ? HZ / 20 : HZ / 10,
                                      &steps);

        if (action & SYS_EVENT)
        {
            /* default_event_handler() handles SYS_POWEROFF (clean
             * shutdown) and SYS_USB_CONNECTED (mounts as storage,
             * blocks until the cable is unplugged) -- see
             * PLAN_MAESTRO.md M-006/A.1. metro_input_next() never
             * calls it itself, see metro_input.h. F9: Metro's own
             * "connected" screen draws first -- default_event_handler()
             * hands the screen to the stock gui_usb_screen_run() for
             * the actual mounted duration right after, see
             * metro_screen_usb.h and DESVIACIONES.md F9-1. */
            if (action == SYS_USB_CONNECTED)
                metro_screen_usb_show();
            else if (action == SYS_POWEROFF || action == SYS_REBOOT)
            {
                draw_shutdown_screen();
                /* R3-F4/DD-5 (M-065): stock Rockbox flushes tagcache's
                 * async command queue (queued playcount/lastplayed/
                 * rating writes, tagcache.c's CMD_UPDATE_NUMERIC) via
                 * tree_flush() -> tagcache_shutdown(), called from deep
                 * inside root_menu()'s own shutdown path. Metro replaces
                 * root_menu() entirely (main.c's own comment, apps/tree.c
                 * is off-limits per this file's header) so that call
                 * never happened -- found while verifying Quickplay's
                 * "order survives a restart" criterion: a normal
                 * shutdown was silently dropping whatever hadn't been
                 * force-flushed yet (the queue only self-flushes at 32
                 * pending entries, tagcache.c's
                 * TAGCACHE_COMMAND_QUEUE_LENGTH). Same risk for R3-F5's
                 * ratings import, next phase -- fixing here, once, in
                 * Metro's own shutdown handling rather than per-feature. */
                tagcache_shutdown();
            }

            if (default_event_handler(action) == SYS_USB_CONNECTED)
            {
                metro_disk_handoff();
                redraw_current();
            }
            continue;
        }

        if (action != MACT_NONE)
            metro_master_art_builder_note_input(); /* M-097: 2s idle window */

        if (action == MACT_NONE)
        {
            /* A postponed sync job keeps running in the background
             * with no screen showing -- still needs polling so it
             * actually finishes (marker cleared) instead of sitting
             * forever in METRO_SYNC_POSTPONED. */
            if (metro_sync_job_active())
                metro_sync_tick();

            /* M-098: poll from every idle tick, not just Music menu entry
             * (metro_screen_hub.c's hub_on_select(), case "Musica") -- the
             * master art builder thread (metro_master_art_builder.c) waits
             * on tagcache_is_usable() by itself but never calls this, so a
             * database tagcache marks "not usable" only ever got repaired
             * if the user happened to open Music first. Same criterion as
             * Aura's aura_music_db_ready() poll (aura_main.c, D-021): no
             * lcd_active() gate -- tagcache repair work has to keep going
             * even with the screen asleep, same as on a real device. Self
             * -throttled to once a second: the idle branch itself already
             * runs at the loop's 10-20 Hz input cadence, and nothing here
             * needs to run that fast (checking tagcache's status is cheap,
             * but there's no reason to pay it more than once a second).
             * This also replaces the metro_music_bootstrap_tick() call
             * that used to live here directly: metro_music_db_ready()
             * already calls it internally (metro_music.c), with the same
             * metro_sync_job_active() early-out, so calling both back to
             * back would just repeat that check for nothing. */
            if (current_tick - last_dbready_tick >= HZ)
            {
                last_dbready_tick = current_tick;
                metro_music_db_ready();
            }

            /* Now Playing has no input of its own most of the time
             * (elapsed time, the progress bar, and the volume overlay's
             * 1.5s countdown all need to update on their own) -- redraw
             * it about once a second even without a button, instead of
             * only reacting to input like every other screen. */
            /* R5-F3 (M-083): mientras el nivel de volumen está en
             * pantalla (3 s quieto + 1 s de fundido) se redibuja a
             * ~8 Hz para que el fundido se vea como tal; el resto del
             * tiempo, la cadencia de siempre. */
            if (at_player && current_tick - last_player_tick >=
                    (metro_screen_nowplaying_volume_visible() ? HZ / 8 : HZ))
            {
                last_player_tick = current_tick;
                redraw_current();
            }

            /* F10: the floating index letter (metro_screen_list.c)
             * needs one more redraw right after it expires to clear
             * itself -- nothing else about the list changed to
             * trigger that on its own otherwise. R2-F3: excluded while
             * in the photo viewer too -- neither this nor the thumb
             * engine below has anything to do on a full-screen photo. */
            /* R5-F5 (M-085): la fila "reproduciendo" del hub se anima
             * por su cuenta (marquesina / respiración) -- un repintado
             * parcial a ~20 Hz, solo mientras hay audio y la fila está a
             * la vista; metro_screen_hub_tick() decide y devuelve false
             * cuando no hay nada que hacer. */
            if (at_root && current_tick - last_hub_tick >= HZ / 20)
            {
                last_hub_tick = current_tick;
                metro_screen_hub_tick();
            }

            /* M-106: un cuadro más de marquesina. Va antes del reparto
             * por pantalla porque la marquesina existe en listas,
             * cuadrículas y "Ahora Suena" por igual -- y
             * redraw_current() ya sabe cuál dibujar. */
            if (metro_marquee_wants_ticks())
                redraw_current();

            /* M-109: un sondeo más del visor de fotos -- es el único
             * sitio donde el debounce de 150 ms puede terminar de
             * vencer sin que llegue ningún botón nuevo (el usuario dejó
             * de girar la rueda y no volvió a tocar nada). Mientras
             * siga en ventana de quietud, esto solo refresca la vista
             * previa (no hace nada nuevo visualmente, pero mantiene el
             * sondeo vivo); en la vuelta exacta en que el debounce
             * vence, dispara el decode real y, si corresponde, el
             * deslizamiento -- ver redraw_viewer_settled(). */
            if (at_viewer && metro_screen_photo_viewer_wants_ticks())
                redraw_viewer_settled();

            if (!at_root && !at_player && !at_viewer)
            {
                bool pending = metro_screen_list_has_pending_redraw();
                if (pending || index_letter_was_pending)
                    redraw_current();
                index_letter_was_pending = pending;

                /* R2-F2/DD-9, generalized R3-F1/DD-1: budget one
                 * thumbnail decode per idle tick, same poll as the
                 * index-letter redraw above -- a no-op (returns false
                 * immediately) on any screen that hasn't queued
                 * anything (the engine is shared across every tile
                 * grid, not just Photos anymore). Redraw only when it
                 * actually decoded something, so a freshly-ready tile
                 * replaces its placeholder without redrawing every
                 * single idle tick for nothing. */
                if (metro_thumbs_tick())
                    redraw_current();
            }

            continue;
        }

        {
            /* F11: transitions are picked by diffing metro_nav_t
             * before/after the action instead of each screen module
             * announcing "I just pushed" -- one place knows the nav
             * stack shape, metro_screen_hub/list/nowplaying.c stay
             * exactly as they were before this phase. Order matters:
             * entering/leaving a sentinel page (Now Playing, F5; the
             * photo viewer, R2-F3) also changes depth (pushed like any
             * other page), so the sentinel-specific checks must win
             * over the generic push/pop ones, or "push(sentinel)"
             * would slide instead of fade. A push that lands on a LIST
             * page while ALREADY on a sentinel (e.g. MACT_OPTIONS from
             * Now Playing) falls through to the generic push case on
             * purpose -- see metro_transitions.h. Now Playing and the
             * viewer are mutually exclusive (metro_screen_photo_viewer.h),
             * so "either sentinel" is just player_x || viewer_x below,
             * never both true at once. */
            metro_nav_t *nav = metro_screen_nav();
            int depth_before = metro_nav_depth(nav);
            int pivot_before = metro_nav_pivot(nav);
            bool player_before = at_player;
            bool viewer_before = at_viewer;
            int depth_after, pivot_after;
            bool root_after, player_after, viewer_after;
            int photo_slide_dir;

            if (at_root)
                metro_screen_hub_handle(action, steps);
            else if (at_player)
                metro_screen_nowplaying_handle(action, steps);
            else if (at_viewer)
                metro_screen_photo_viewer_handle(action, steps);
            else
                metro_screen_list_handle(action, steps);

            /* M-106: se consume SIEMPRE, justo después de la acción,
             * aunque la rama elegida acabe siendo otra -- un anuncio
             * que sobrevive a su propia acción reaparecería en la
             * siguiente, deslizando algo que no cambió. */
            photo_slide_dir = metro_screen_photo_viewer_take_slide();

            depth_after = metro_nav_depth(nav);
            pivot_after = metro_nav_pivot(nav);
            root_after = metro_nav_is_root(nav);
            player_after = !root_after && metro_screen_nowplaying_is_current();
            viewer_after = !root_after && metro_screen_photo_viewer_is_current();

            if (depth_after > depth_before && (player_after || viewer_after))
                metro_transitions_fade(redraw_current);
            else if (depth_after > depth_before)
            {
                metro_transitions_push(redraw_current, 1);
                /* F12: the cascade only makes sense on the list this
                 * push landed on, never the hub (no rows, its own
                 * *_show()) or a sentinel (handled by the fade branch
                 * above, never reaches here). */
                if (!root_after && !player_after && !viewer_after)
                    metro_screen_list_run_feather_if_pending();
            }
            else if (depth_after < depth_before && (player_before || viewer_before) &&
                     !player_after && !viewer_after)
                metro_transitions_fade(redraw_current);
            else if (depth_after < depth_before)
                metro_transitions_push(redraw_current, -1);
            else if (!root_after && pivot_after != pivot_before)
                metro_transitions_slide(redraw_current, pivot_after > pivot_before ? 1 : -1);
            else if (viewer_after && photo_slide_dir != 0)
                /* M-106: cambio de foto dentro del visor. No mueve
                 * profundidad ni pivot, así que no hay nada que
                 * diffear -- el visor lo ANUNCIA y aquí se elige la
                 * transición, igual que CONTINUUM y FEATHER. Rápida
                 * (tope de 150 ms del plan maestro): una pantalla
                 * entera de foto se lee mucho antes que una lista. */
                metro_transitions_slide_fast(redraw_current, photo_slide_dir);
            else
                redraw_current();
        }
    }
}

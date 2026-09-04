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

/* F6: the one full-screen wait state in Metro (PLAN_MAESTRO.md S4.3) --
 * drawn straight from metro_main.c, not a metro_screen_* module, same
 * as the plan's own file list for this phase (no new screen file).
 * MENU postpones a running job (metro_sync_postpone(), the job keeps
 * going in the background) or dismisses an error (metro_sync_dismiss()) --
 * either way the loop below exits as soon as metro_sync_needs_screen()
 * goes false. */
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
    metro_draw_text(MFONT_TITLE, 12, 100, metro_lang_str(msg), metro_color_fg());
    if (is_error)
        metro_draw_text(MFONT_CAPTION, 12, 140, metro_lang_str(LANG_SYNC_DISMISS_HINT),
                         metro_color_secondary());
    else
    {
        /* M-100: the image phase shares this screen -- it is part of
         * "preparing the library", not a second wait state (S4.3 says
         * there is exactly one). */
        metro_master_art_phase_t phase = METRO_MASTER_ART_PHASE_IDLE;
        int done = 0, total = 0;

        if (metro_sync_art_progress(&phase, &done, &total))
        {
            char line[48];
            enum metro_lang_id fmt;

            if (phase == METRO_MASTER_ART_PHASE_PHOTOS)
                fmt = LANG_SYNC_ART_PHOTOS;
            else if (phase == METRO_MASTER_ART_PHASE_ARTISTS)
                fmt = LANG_SYNC_ART_ARTISTS;
            else
                fmt = LANG_SYNC_ART_ALBUMS;

            if (phase == METRO_MASTER_ART_PHASE_PHOTOS)
                snprintf(line, sizeof(line), metro_lang_str(fmt), done);
            else
                snprintf(line, sizeof(line), metro_lang_str(fmt), done, total);
            metro_draw_text(MFONT_CAPTION, 12, 140, line, metro_color_secondary());
        }
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
    lcd_setfont(metro_font_id(MFONT_TITLE));
    lcd_getstringsize((const unsigned char *)text, &w, &h);
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

        if (metro_sync_tick())
            draw_sync_screen();
    }
}

/* Boot, and every return from the USB screen -- the only two moments
 * the firmware ever recovers the disk (PLAN_MAESTRO.md S1.2). */
static void metro_disk_handoff(void)
{
    metro_settings_apply_pending_clock();
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
        int action = metro_input_next(ctx,
                                      (at_root && metro_screen_hub_wants_ticks())
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

            if (at_root)
                metro_screen_hub_handle(action, steps);
            else if (at_player)
                metro_screen_nowplaying_handle(action, steps);
            else if (at_viewer)
                metro_screen_photo_viewer_handle(action, steps);
            else
                metro_screen_list_handle(action, steps);

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
            else
                redraw_current();
        }
    }
}

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
/* Orchestrates /.aura/sync-pending.json (metro_sync_marker.c parses the
 * text; this drives tagcache from it). Music is the only section that
 * does real work in v1 -- video/images have nothing cached to
 * invalidate (metro_video.c/metro_photos.c, F7, re-scan their directory
 * fresh every time their page is entered, same pattern metro_music.c
 * already uses for its own lists), so this just clears those two
 * sections immediately and moves on. Narrower than Aura-Firmware's
 * aura_sync.c on purpose: no per-section progress screen (nothing here
 * takes long enough to need one beyond music), no cfcache/album-art
 * invalidation (metro_albumart.c has no disk cache to invalidate, F5),
 * no manual "rebuild" trigger yet (F8 wires that from Settings).
 * DESVIACIONES.md F6-1. */
#ifndef METRO_SYNC_H
#define METRO_SYNC_H

#include <stdbool.h>

typedef enum {
    METRO_SYNC_IDLE = 0,
    METRO_SYNC_WAIT_TAGCACHE, /* marker read, waiting on tagcache_is_fully_initialized() */
    METRO_SYNC_RUNNING,       /* tagcache_update()/tagcache_rebuild() in flight */
    METRO_SYNC_POSTPONED,     /* user pressed MENU on the screen; job still finishing in the background */
    METRO_SYNC_ERROR_VERSION, /* marker's "version" is newer than METRO_SYNC_MARKER_VERSION_SUPPORTED */
    METRO_SYNC_ERROR_ATTEMPTS,/* marker's "attempts" hit METRO_SYNC_MARKER_MAX_ATTEMPTS */
} metro_sync_state_t;

/* Reads /.aura/sync-pending.json if present and decides what to do --
 * call once at boot and once after returning from the USB screen (the
 * only two moments the firmware ever recovers the disk). No-op while
 * a job from a previous call is still active. */
void metro_sync_check_pending(void);

/* True while metro_main.c should be showing the sync screen instead
 * of whatever it would draw otherwise. */
bool metro_sync_needs_screen(void);

/* True while a rebuild/update job is in flight or postponed-but-still-
 * finishing in the background -- metro_music_db_ready() cedes to this
 * (never starts its own tagcache_rebuild()/tagcache_start_scan() while
 * true) so the two don't race over the same database. */
bool metro_sync_job_active(void);

metro_sync_state_t metro_sync_state(void);

/* Advances the state machine one step; call on every redraw cycle
 * while metro_sync_needs_screen() is true. Returns true if the screen
 * should redraw (state or progress changed). */
bool metro_sync_tick(void);

/* MENU on the running screen: stops tagcache, leaves the marker
 * intact for the next boot/USB-return to pick back up. */
void metro_sync_postpone(void);

/* MENU on an error screen (unsupported version / too many attempts):
 * dismisses it, back to idle. The marker itself is untouched -- an
 * unsupported-version marker waits for a newer firmware; a
 * too-many-attempts one waits for the user's manual retry (F8, this
 * function). */
void metro_sync_dismiss(void);

/* F8: Settings -> "library" row. Writes a fresh marker with all three
 * sections marked and attempts reset to 0 (same shape
 * aura_sync_request_manual() writes), superseding any error currently
 * on screen, and immediately calls metro_sync_check_pending() so the
 * job starts on the same redraw cycle that follows. Returns true if a
 * job is now active/queued (the caller should show the sync screen). */
bool metro_sync_request_manual(void);

/* R5 (M-090, contrato v10): deja /.aura/sync-pending.json con music=true
 * y attempts=0 -- lo que el firmware que DESPIERTA tras un cambio de
 * firmware necesita para reconstruir su propia base de datos (la base
 * vive dentro de cada arbol). Misma escritura que usa el propio ciclo
 * de sync; el marcador se interpreta al siguiente arranque. */
bool metro_sync_write_music_pending_marker(void);

/* R5 (M-091, contrato v12; M-095, v15): sello de biblioteca.
 * /.aura/library-stamp solo cambia cuando un sync de Studio toca la
 * musica; /.aura/tagcache/db_stamp.txt anota contra que sello se
 * construyo la base COMPARTIDA (ya no una por arbol).
 *
 * metro_sync_record_db_stamp(): al terminar BIEN una (re)construccion --
 * la base ahora describe la biblioteca vigente. Crea el sello de
 * biblioteca si no existe.
 *
 * metro_sync_db_stamp_present(): hay sello de base en disco.
 *
 * metro_sync_migrate_db_stamp(db_was_migrated): una vez, al arrancar
 * (desde metro_force_shared_db_path()): el sello por arbol pre-v15
 * (.rockbox/aura/db_stamp.txt) se renombra al compartido SOLO si la
 * base que describia acaba de migrar con el y no hay sello compartido;
 * en cualquier otro caso se borra.
 *
 * metro_sync_switch_needs_rebuild(): para el cambio de firmware, antes
 * del reinicio. Sin sello de biblioteca (arranque en frio) sella la
 * base compartida, que esta al dia, y devuelve false; con el, devuelve
 * true si la base no tiene sello o tiene uno distinto -- es decir, si
 * hay que dejar el marcador. No depende de ningun arbol: la base es una. */
void metro_sync_record_db_stamp(void);
bool metro_sync_db_stamp_present(void);
void metro_sync_migrate_db_stamp(bool db_was_migrated);
bool metro_sync_switch_needs_rebuild(void);

#endif /* METRO_SYNC_H */

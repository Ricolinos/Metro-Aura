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
#include <stdio.h>

/* config.h before tagcache.h -- see DECISIONS.md M-030. */
#include "config.h"
#include "tagcache.h"
#include "file.h"
#include "dir.h"
#include "timefuncs.h"
#include "misc.h" /* read_line()/settings_parseline() -- R3-F5/DD-7 */

#include "metro_sync.h"
#include "metro_master_art_builder.h" /* M-100 */
#include "metro_sync_marker.h"
#include "metro_settings.h"
#include "metro_thumbs.h" /* metro_thumbs_mark_dirty() -- M-096 */

/* Contract S4: the marker lives at the disk ROOT, not under
 * /.rockbox/aura -- it's the one thing Studio leaves for the firmware
 * as "mail", kept separate from Metro's own settings/caches. */
#define METRO_SYNC_DIR         "/.aura"
#define METRO_SYNC_MARKER_PATH METRO_SYNC_DIR "/sync-pending.json"
#define METRO_LIBRARY_STAMP_PATH METRO_SYNC_DIR "/library-stamp" /* M-091, v12 */
/* M-095 (v15): the stamp sits NEXT TO the database it describes -- the
 * shared one under AURA_SHARED_DB_DIR -- so a firmware switch compares
 * "the database on disk" against "the library on disk", whichever
 * family built either. The per-tree location is only read to migrate. */
#define METRO_DB_STAMP_PATH        AURA_SHARED_DB_DIR "/db_stamp.txt"
#define METRO_LEGACY_DB_STAMP_PATH ROCKBOX_DIR "/aura/db_stamp.txt"

/* A real marker is ~150 bytes; 1KB leaves room for future keys. */
#define MARKER_BUF_SIZE 1024

typedef enum { JOB_NONE = 0, JOB_UPDATE, JOB_REBUILD } job_kind_t;

static metro_sync_state_t s_state = METRO_SYNC_IDLE;
/* M-100: the image pass does NOT start the moment tagcache finishes.
 * Measured in Aura-Firmware (D-344) with traces: at that instant the
 * album walk still sees 0 albums -- the database was just committed and
 * is not queryable yet -- so the albums phase did nothing and the pass
 * reported itself complete without preparing a single cover. Metro's
 * builder already retries albums on its own (`albums_pending`), but the
 * screen must not hand control back until the real pass ran. */
static bool s_art_started = false;
static int  s_art_wait_ticks = 0;
#define METRO_ART_DB_WAIT_MAX_TICKS (HZ * 10)
static metro_sync_marker_t s_marker;
static job_kind_t s_job = JOB_NONE;
static unsigned s_jobs_before = 0;
static int s_attempts_before = 0;
static bool s_recovery_done = false;

static int read_marker_text(char *buf, size_t bufsize)
{
    int fd, n;

    fd = open(METRO_SYNC_MARKER_PATH, O_RDONLY);
    if (fd < 0)
        return -1;
    n = read(fd, buf, bufsize - 1);
    close(fd);
    if (n < 0)
        return -1;
    if ((size_t)n >= bufsize - 1)
        return -2; /* bigger than any legitimate marker: not ours */
    buf[n] = '\0';
    return n;
}

static bool write_marker(const metro_sync_marker_t *m)
{
    char buf[MARKER_BUF_SIZE];
    int fd, n = metro_sync_marker_serialize(m, buf, sizeof(buf));

    if (n < 0)
        return false;
    if (!dir_exists(METRO_SYNC_DIR))
        mkdir(METRO_SYNC_DIR);

    fd = creat(METRO_SYNC_MARKER_PATH, 0666);
    if (fd < 0)
        return false;
    write(fd, buf, (size_t)n);
    close(fd);
    return true;
}

static void remove_marker(void)
{
    remove(METRO_SYNC_MARKER_PATH);
}

/* --- R5 (M-091, contrato v12): sello de biblioteca ------------------- */

#define STAMP_BUF 64

static int read_small_file(const char *path, char *buf, size_t bufsz)
{
    int fd = open(path, O_RDONLY);
    ssize_t n;

    if (fd < 0)
        return -1;
    n = read(fd, buf, bufsz - 1);
    close(fd);
    if (n <= 0)
        return -1;
    buf[n] = '\0';
    /* una sola linea: recortar el salto final */
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = '\0';
    return (int)n;
}

static bool write_small_file(const char *path, const char *text)
{
    int fd = creat(path, 0666);
    size_t len = strlen(text);

    if (fd < 0)
        return false;
    write(fd, text, len);
    write(fd, "\n", 1);
    close(fd);
    return true;
}

/* Sello nuevo: fecha-hora del RTC + ticks, opaco. Solo importa que sea
 * distinto de cualquier anterior; se compara por igualdad exacta. */
static void make_stamp(char *out, size_t outsz)
{
    struct tm *now = get_time();

    if (now)
        snprintf(out, outsz, "fw-%04d%02d%02dT%02d%02d%02d-%08lx",
                 now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,
                 now->tm_hour, now->tm_min, now->tm_sec,
                 (unsigned long)current_tick);
    else
        snprintf(out, outsz, "fw-%08lx", (unsigned long)current_tick);
}

/* Lee el sello compartido; si no existe lo crea. */
static void ensure_library_stamp(char *out, size_t outsz)
{
    if (read_small_file(METRO_LIBRARY_STAMP_PATH, out, outsz) > 0)
        return;
    make_stamp(out, outsz);
    if (!dir_exists(METRO_SYNC_DIR))
        mkdir(METRO_SYNC_DIR);
    write_small_file(METRO_LIBRARY_STAMP_PATH, out);
}

void metro_sync_record_db_stamp(void)
{
    char stamp[STAMP_BUF];

    ensure_library_stamp(stamp, sizeof(stamp));
    if (!dir_exists(AURA_SHARED_DB_DIR))
        mkdir(AURA_SHARED_DB_DIR); /* /.aura itself: ensure_library_stamp() */
    write_small_file(METRO_DB_STAMP_PATH, stamp);
}

bool metro_sync_db_stamp_present(void)
{
    return file_exists(METRO_DB_STAMP_PATH);
}

void metro_sync_migrate_db_stamp(bool db_was_migrated)
{
    if (!file_exists(METRO_LEGACY_DB_STAMP_PATH))
        return;
    if (db_was_migrated && !file_exists(METRO_DB_STAMP_PATH))
    {
        /* AURA_SHARED_DB_DIR was just created by the caller. */
        if (rename(METRO_LEGACY_DB_STAMP_PATH, METRO_DB_STAMP_PATH) == 0)
            return;
    }
    /* Either the database it described didn't travel (so it describes
     * nothing on disk anymore) or the shared stamp already exists. */
    remove(METRO_LEGACY_DB_STAMP_PATH);
}

bool metro_sync_switch_needs_rebuild(void)
{
    char stamp[STAMP_BUF], recorded[STAMP_BUF];
    bool had_stamp = read_small_file(METRO_LIBRARY_STAMP_PATH, stamp, sizeof(stamp)) > 0;

    if (!had_stamp)
    {
        /* Arranque en frio del mecanismo: el saliente acaba de estar
         * corriendo con la base compartida, que SI esta al dia -- se
         * sella y se anota, para que el proximo cambio ya no reconstruya. */
        metro_sync_record_db_stamp();
        return false;
    }

    if (read_small_file(METRO_DB_STAMP_PATH, recorded, sizeof(recorded)) <= 0)
        return true; /* nadie anoto contra que biblioteca se construyo la base */
    return strcmp(recorded, stamp) != 0;
}

bool metro_sync_write_music_pending_marker(void)
{
    metro_sync_marker_t m;

    metro_sync_marker_init(&m);
    m.version = METRO_SYNC_MARKER_VERSION_SUPPORTED;
    m.music = true;
    return write_marker(&m);
}

static void go_idle(void)
{
    s_state = METRO_SYNC_IDLE;
    s_job = JOB_NONE;
}

void metro_sync_check_pending(void)
{
    static char buf[MARKER_BUF_SIZE];
    metro_sync_marker_status_t st;
    int n;

    /* A job in flight rules: don't re-read the marker out from under it. */
    if (metro_sync_job_active())
        return;

    n = read_marker_text(buf, sizeof(buf));
    if (n == -1)
    {
        go_idle();
        return;
    }
    if (n == -2)
    {
        remove_marker();
        go_idle();
        return;
    }

    st = metro_sync_marker_parse(buf, &s_marker);

    switch (st)
    {
        case METRO_SYNC_MARKER_MALFORMED:
        case METRO_SYNC_MARKER_MISSING_VERSION:
            /* Corrupt (Studio writes atomically, so this shouldn't
             * happen): nothing reliable to rebuild from, and leaving
             * it would trip every boot over it. Gone. */
            remove_marker();
            go_idle();
            return;

        case METRO_SYNC_MARKER_UNSUPPORTED:
            /* Contract S4: ignored (nothing gets rebuilt with rules we
             * don't understand) and reported on screen. The marker
             * stays -- a newer firmware will resolve it. */
            s_state = METRO_SYNC_ERROR_VERSION;
            return;

        case METRO_SYNC_MARKER_OK:
        default:
            break;
    }

    if (!metro_sync_marker_has_work(&s_marker))
    {
        remove_marker();
        go_idle();
        return;
    }

    if (s_marker.attempts >= METRO_SYNC_MARKER_MAX_ATTEMPTS)
    {
        s_state = METRO_SYNC_ERROR_ATTEMPTS;
        return;
    }

    s_state = METRO_SYNC_WAIT_TAGCACHE;
}

bool metro_sync_needs_screen(void)
{
    return s_state == METRO_SYNC_WAIT_TAGCACHE
        || s_state == METRO_SYNC_RUNNING
        || s_state == METRO_SYNC_BUILDING_ART
        || s_state == METRO_SYNC_ERROR_VERSION
        || s_state == METRO_SYNC_ERROR_ATTEMPTS;
}

bool metro_sync_job_active(void)
{
    return s_state == METRO_SYNC_WAIT_TAGCACHE
        || s_state == METRO_SYNC_RUNNING
        || s_state == METRO_SYNC_POSTPONED
        /* M-100: the image phase is work in progress too -- metro_main.c
         * only ticks the state machine while this holds. The builder's
         * own may_run() would refuse to work while this is true, which
         * is why foreground mode skips that gate. */
        || s_state == METRO_SYNC_BUILDING_ART;
}

metro_sync_state_t metro_sync_state(void)
{
    return s_state;
}

static void finish_ok(void)
{
    /* R5 (M-091, v12): la base recien (re)construida describe la
     * biblioteca vigente -- se anota el sello, y con eso el proximo
     * cambio de firmware de ida y vuelta sin sync no reconstruye. */
    metro_sync_record_db_stamp();
    /* M-096: albums removed by this sync leave orphan thumbnails under
     * /.aura/thumbs/albums -- swept on the next entry into Music. */
    if (s_marker.music)
        metro_thumbs_mark_dirty();
    remove_marker();

    /* M-100 (owner's request, 2026-08-27): "library prepared" includes
     * the images. Handing control back here means Music/Artists/Albums
     * show placeholders again while the builder walks in the background
     * -- exactly the wait that was reported. Only when music was in
     * scope: a videos/photos-only marker touches neither albums nor
     * artist photos. */
    if (s_marker.music)
    {
        metro_master_art_builder_set_foreground(true);
        s_art_started = false;
        s_art_wait_ticks = 0;
        s_state = METRO_SYNC_BUILDING_ART;
        return;
    }
    go_idle();
}

/* M-100: leave the image phase -- the builder goes back to its
 * background cadence no matter how we got out (finished, postponed,
 * interrupted). */
static void leave_art_phase(void)
{
    metro_master_art_builder_set_foreground(false);
}

bool metro_sync_art_progress(metro_master_art_phase_t *phase, int *done, int *total)
{
    if (s_state != METRO_SYNC_BUILDING_ART)
        return false;
    return metro_master_art_builder_progress(phase, done, total);
}

/* R3-F5/DD-7 (M-066): one-way import of Studio's ratings.cfg --
 * `<ruta absoluta>: <rating 0-10>` por línea, calcado del import de
 * Aura (`aura_music.c`, consultado read-only) salvo el propio
 * `metro_settings_ratings_cfg_path()` en vez de una ruta armada a
 * mano (regla de rutas del CLAUDE.md). Mismo patrón de parseo que
 * `metro_media_categories.c`'s `load_index()` -- `read_line()` +
 * `settings_parseline()`, sin cargar el archivo completo a RAM.
 * Silenciosamente ignora líneas que no calcen (comentarios, formato
 * roto) o cuya ruta ya no exista en tagcache (pista borrada/renombrada
 * desde el último sync) -- mismo espíritu "forward-compat, ignorado"
 * que el resto de los parsers de este archivo. NO hay camino de
 * vuelta: una calificación puesta en el dispositivo se pierde en el
 * siguiente sync si Studio no la conoce todavía -- asimetría real,
 * documentada en `docs/COMPAT_STUDIO.md`, no escondida. */
static void import_ratings(void)
{
    char path[MAX_PATH];
    char line[MAX_PATH + 16];
    int fd;

    metro_settings_ratings_cfg_path(path, sizeof(path));
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return; /* no ratings.cfg this sync: nothing to do, not an error */

    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char *track_path, *value;
        struct tagcache_search tcs;
        int rating;

        if (!settings_parseline(line, &track_path, &value))
            continue;

        rating = atoi(value);
        if (rating < 0)
            rating = 0;
        else if (rating > 10)
            rating = 10;

        if (!tagcache_find_index(&tcs, track_path))
            continue; /* not (or no longer) in the library: skip */

        tagcache_update_numeric(tcs.idx_id, tag_rating, rating);
        tagcache_search_finish(&tcs);
    }

    close(fd);

    /* R3-F5/DD-7 (M-066): tagcache_update_numeric() only QUEUES the
     * write (apps/tagcache.c's async command queue, same mechanism
     * R3-4/docs/DESVIACIONES.md already found for lastplayed) -- it
     * only reaches the RAM cache (what every subsequent read actually
     * sees, tagcache_get_numeric()'s use_ram=true path) once the queue
     * is flushed, which otherwise only happens at 32 pending entries or
     * a real shutdown. Without forcing it here, a rating imported by
     * "sincronizar ahora" would stay invisible -- not wrong, just
     * silently delayed, possibly for the rest of the session -- which
     * defeats the point of a manual sync. tagcache_shutdown() is just
     * `run_command_queue(true)` on this target (HAVE_EEPROM_SETTINGS,
     * the only other thing it can do, isn't defined for ipod6g) -- a
     * plain synchronous flush, safe to call mid-session, not only at
     * real shutdown. */
    tagcache_shutdown();
}

static void start_job(void)
{
    /* The attempt counts from the moment it starts: if power is lost
     * halfway through, the marker already carries the raised counter.
     * Postponing restores it. */
    s_attempts_before = s_marker.attempts;
    s_marker.attempts++;
    write_marker(&s_marker);

    /* Video/images: nothing cached to invalidate yet (F7's
     * metro_video.c/metro_photos.c re-scan their directory fresh every
     * time their page is entered, same as metro_music.c's own lists) --
     * see the module comment in metro_sync.h / DESVIACIONES.md F6-1. */
    if (!s_marker.music)
    {
        finish_ok();
        return;
    }

    /* With a usable database and no orphaned temp file, tagcache_update():
     * walks the whole tree, re-reads files whose mtime changed (Studio
     * never preserves dates when copying, so anything it touched has a
     * new mtime), adds new ones, drops deleted ones -- the OLD database
     * stays usable meanwhile. No database (first boot, or an old Studio
     * that deleted it), or an orphaned temp from an aborted pass:
     * tagcache_rebuild() from scratch. */
    s_jobs_before = tagcache_get_build_jobs_done();
    if (tagcache_is_usable() && !tagcache_has_pending_temp())
    {
        s_job = JOB_UPDATE;
        tagcache_update();
    }
    else
    {
        s_job = JOB_REBUILD;
        tagcache_rebuild();
    }
    s_recovery_done = false;
    s_state = METRO_SYNC_RUNNING;
}

static void job_ended(void)
{
    bool ok = tagcache_is_usable() && !tagcache_has_pending_temp();

    if (ok)
    {
        /* R3-F5/DD-7 (M-066): "al terminar bien [el import de música]"
         * (library-layout-v1.md) -- justo aquí, no en el atajo de
         * !s_marker.music de start_job() (ese camino no tocó tagcache
         * en absoluto esta vez). */
        import_ratings();
        finish_ok();
        return;
    }

    if (!tagcache_is_usable() && !tagcache_has_pending_temp() && !s_recovery_done)
    {
        /* Finished, but the database ended up disabled: tagcache found
         * corrupt structures while reloading it. The only way out is
         * building it from scratch -- same job, same screen, once. */
        s_recovery_done = true;
        s_jobs_before = tagcache_get_build_jobs_done();
        s_job = JOB_REBUILD;
        tagcache_rebuild();
        if (s_state != METRO_SYNC_POSTPONED)
            s_state = METRO_SYNC_RUNNING;
        return;
    }

    if (tagcache_get_stat()->commit_delayed)
    {
        /* tagcache scanned everything but couldn't get a temp buffer to
         * sort the indices, and left the commit "for the next boot"
         * (its own mechanism -- the thread confirms it at boot, before
         * anything else). Not a failure of ours: the counter goes back
         * to where it was, the marker stays, and next boot the update
         * completes (everything already indexed, a fast pass) and
         * clears it. */
        s_marker.attempts = s_attempts_before;
        write_marker(&s_marker);
        s_state = METRO_SYNC_IDLE;
        s_job = JOB_NONE;
        return;
    }

    if (s_state == METRO_SYNC_POSTPONED)
    {
        /* Aborted on purpose (Menu): not a failure -- the counter goes
         * back to where it was and the marker stays for next boot. An
         * aborted update leaves a half-done temp file that would block
         * any later build this boot; the old database is intact, so
         * it's discarded. An aborted rebuild has no old database left:
         * the temp is kept so tagcache's own boot confirms (commits)
         * it instead of losing what was already scanned. */
        s_marker.attempts = s_attempts_before;
        write_marker(&s_marker);
        if (s_job == JOB_UPDATE && tagcache_has_pending_temp())
            tagcache_discard_pending_temp();
        go_idle();
        return;
    }

    /* Real failure (USB plugged in halfway, database that never came
     * back usable...): the counter already went up; the marker stays
     * and gets retried next boot, unless that's already too many. */
    if (s_marker.attempts >= METRO_SYNC_MARKER_MAX_ATTEMPTS)
    {
        s_state = METRO_SYNC_ERROR_ATTEMPTS;
        s_job = JOB_NONE;
    }
    else
        go_idle();
}

bool metro_sync_tick(void)
{
    switch (s_state)
    {
        case METRO_SYNC_WAIT_TAGCACHE:
            /* Same precaution as metro_music_db_ready() (M-030-adjacent,
             * D-206 in Aura-Firmware): don't decide update-vs-rebuild
             * before tagcache has determined whether a usable database
             * already exists on disk (~1s after boot). */
            if (!tagcache_is_fully_initialized())
                return false;
            start_job();
            return true;

        case METRO_SYNC_RUNNING:
        case METRO_SYNC_POSTPONED:
            if (tagcache_get_build_jobs_done() == s_jobs_before)
                return s_state == METRO_SYNC_RUNNING; /* still going: redraw */
            job_ended();
            return true;

        case METRO_SYNC_BUILDING_ART:
            if (!s_art_started)
            {
                /* The database has to be queryable BEFORE walking it, or
                 * the albums phase is skipped in silence. */
                if (!tagcache_is_usable() && s_art_wait_ticks < METRO_ART_DB_WAIT_MAX_TICKS)
                {
                    s_art_wait_ticks++;
                    return true;
                }
                s_art_started = true;
                metro_master_art_builder_begin_full_pass();
                return true;
            }
            if (metro_master_art_builder_pass_done()
                || !metro_master_art_builder_is_running())
            {
                leave_art_phase();
                go_idle();
                return true;
            }
            return true; /* progress: redraw */

        default:
            return false;
    }
}

void metro_sync_postpone(void)
{
    switch (s_state)
    {
        case METRO_SYNC_WAIT_TAGCACHE:
            /* Nothing started yet: the marker is left exactly as is. */
            go_idle();
            break;
        case METRO_SYNC_RUNNING:
            tagcache_stop_scan();
            s_state = METRO_SYNC_POSTPONED;
            break;
        case METRO_SYNC_BUILDING_ART:
            /* M-100: MENU closes the screen; the builder is NOT
             * cancelled -- it goes back to its background cadence and
             * finishes on its own. */
            leave_art_phase();
            go_idle();
            break;
        default:
            break;
    }
}

void metro_sync_dismiss(void)
{
    if (s_state == METRO_SYNC_ERROR_VERSION || s_state == METRO_SYNC_ERROR_ATTEMPTS)
        go_idle();
}

bool metro_sync_request_manual(void)
{
    metro_sync_marker_t m;
    struct tm *now;

    if (metro_sync_job_active())
        return true; /* already running: the screen already shows it */

    metro_sync_marker_init(&m);
    m.version = METRO_SYNC_MARKER_VERSION_SUPPORTED;
    m.music = m.video = m.images = true;
    m.attempts = 0;
    now = get_time();
    if (now && valid_time(now))
        snprintf(m.timestamp, sizeof(m.timestamp), "%04d-%02d-%02dT%02d:%02d:%02d",
                 now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,
                 now->tm_hour, now->tm_min, now->tm_sec);
    else
        strcpy(m.timestamp, "1970-01-01T00:00:00");

    if (!write_marker(&m))
        return false;

    /* A pending error on screen (version/attempts) is superseded by
     * the marker just written. */
    go_idle();
    metro_sync_check_pending();
    return metro_sync_needs_screen();
}

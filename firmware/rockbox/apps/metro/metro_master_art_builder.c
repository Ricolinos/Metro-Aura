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
#include <stdlib.h>

#include "config.h"
#include "kernel.h"
#include "thread.h"
#include "audio.h"
#include "buffering.h"
#include "playback.h"
#include "tagcache.h"
#include "dir.h"
#include "file.h"
#include "string-extra.h"
#include "crc32.h"
#include "debug.h"

#include "metro_master_art_builder.h"
#include "metro_master_art.h"
#include "metro_music.h"
#include "metro_albumart.h"
#include "metro_thumbs.h"
#include "metro_photos.h"
#include "metro_settings.h"
#include "metro_sync.h"

/* Plenty for get_metadata()+find_albumart()+read_jpeg_file() (all
 * their big state is static -- metro_albumart.c's mp3entry, jpeg_load's
 * decoder) plus metro_music.c's TAGCACHE_BUFSZ stack strings. Same
 * order of magnitude as the UI thread's own stack. */
#define BUILDER_STACK_SIZE (8 * 1024)
static long s_stack[BUILDER_STACK_SIZE / sizeof(long)];
static const char s_thread_name[] = "metro_art";

#define IDLE_BEFORE_START (2 * HZ)
#define GAP_BETWEEN_ITEMS (HZ / 20)
#define GAP_WHEN_PLAYING  (HZ / 4)

/* Album seeks and the live-key table for the orphan sweep -- same
 * caps and crc scheme as metro_thumbs.c's GC (M-096). */
#define MAX_ALBUMS  METRO_MUSIC_MAX_GROUPS
#define MAX_LIVE    2048
static int32_t  s_seeks[MAX_ALBUMS];
static uint32_t s_live[MAX_LIVE];
static int      s_live_n;

static volatile bool s_paused = false;
static volatile bool s_pass_requested = false;
/* M-100: progress of the pass, written only by this thread and read
 * only by the UI thread. Word-sized, and Rockbox schedules
 * cooperatively (a switch only happens at yield/sleep/blocking), so no
 * lock -- same discipline as s_paused. */
static volatile metro_master_art_phase_t s_phase = METRO_MASTER_ART_PHASE_IDLE;
static volatile int  s_phase_done = 0;
static volatile int  s_phase_total = 0;   /* 0 = unknown (photos) */
static volatile bool s_pass_done = false;
static volatile bool s_foreground = false;
static volatile long s_last_input_tick = 0;
static bool s_started = false;

static int cmp_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : x > y;
}

static void live_reset(void) { s_live_n = 0; }
static void live_add(const char *key)
{
    if (s_live_n < MAX_LIVE)
        s_live[s_live_n++] = crc_32(key, strlen(key), 0xffffffff);
}
/* Only when every item was seen (a capped table can't tell live from
 * orphan) -- GC removes unknown files, so a partial table is unsafe. */
static void live_sweep(const char *subdir, int total)
{
    if (total > MAX_LIVE)
        return;
    qsort(s_live, s_live_n, sizeof(s_live[0]), cmp_u32);
    metro_master_art_gc(subdir, s_live, s_live_n);
}

/* "Cargando buffer": playback is on and the file buffer is below half
 * -- the buffering thread is (or is about to be) hitting the disk for
 * audio; our own reads/decodes wait. There is no direct "buffering
 * now" query in playback.h; this proxy errs on the side of the music. */
static bool audio_wants_the_disk(void)
{
    if (!(audio_status() & AUDIO_STATUS_PLAY))
        return false;
    return buf_used() < audio_get_filebuflen() / 2;
}

static bool may_run(void)
{
    /* M-100: en primer plano el usuario esta MIRANDO esta pasada -- las
     * tres puertas de "cede al usuario" (pausa de transicion, ventana de
     * inactividad, job de sync en curso) estarian las tres activas a la
     * vez justo cuando hay que trabajar, y el hilo no avanzaria nunca.
     * La del audio se conserva: si suena musica, el disco es suyo. */
    if (!s_foreground)
    {
        if (s_paused)
            return false;
        if (TIME_BEFORE(current_tick, s_last_input_tick + IDLE_BEFORE_START))
            return false;
        if (metro_sync_job_active())
            return false;
    }
    if (audio_wants_the_disk())
        return false;
    return true;
}

/* Sleeps until may_run() -- the wait between elements. */
static void wait_turn(void)
{
    /* M-100: en primer plano solo se cede el turno (la pantalla tiene que
     * poder redibujarse), sin la pausa que existe para no competir con
     * el usuario. */
    if (s_foreground)
        yield();
    else
        sleep((audio_status() & AUDIO_STATUS_PLAY) ? GAP_WHEN_PLAYING : GAP_BETWEEN_ITEMS);
    while (!may_run())
        sleep(HZ / 4);
}

/* --- albums ------------------------------------------------------------ */

static bool build_album(int32_t seek)
{
    char key[METRO_MASTER_ART_KEY_LEN];
    char path[MAX_PATH];
    metro_music_item_t track;
    fb_data *master;
    bool ok;

    metro_master_art_lock();
    if (!metro_music_album_art_key(seek, key, sizeof(key)))
    {
        metro_master_art_unlock();
        return false;
    }
    live_add(key);
    if (metro_master_art_probe("albums", key) != METRO_MASTER_ART_MISSING)
    {
        metro_master_art_unlock();
        return false; /* nothing to do: no decode, no sleep needed */
    }
    master = metro_master_art_scratch();
    ok = metro_music_songs_of_album(seek, &track, 1) >= 1
        && metro_music_track_path(track.seek, path, sizeof(path))
        && metro_albumart_decode_track_master(path, master);
    if (ok)
        metro_master_art_write("albums", key, master, METRO_MASTER_ART_ALBUM_PX);
    else
        metro_master_art_write_none("albums", key);
    metro_master_art_unlock();
    return true;
}

/* Returns false if the database was not usable (retry later). */
static bool build_albums(void)
{
    int n, i;

    if (!tagcache_is_usable() || !tagcache_is_fully_initialized())
        return false;
    n = metro_music_album_seeks(s_seeks, MAX_ALBUMS);
    s_phase_total = n > 0 ? n : 0;
    live_reset();
    for (i = 0; i < n; i++)
    {
        s_phase_done = i;
        if (s_pass_requested)
            return true; /* a newer request supersedes this pass */
        if (build_album(s_seeks[i]))
            wait_turn();
        else
            yield();
    }
    s_phase_done = n;
    live_sweep("albums", n);
    return true;
}

/* --- artists ----------------------------------------------------------- */

static void build_artists(void)
{
    char filename[METRO_FSUTIL_NAME_LEN];
    char key[METRO_MASTER_ART_KEY_LEN];
    char dir[MAX_PATH], path[MAX_PATH];
    long mtime;
    int n, i;

    metro_music_reload_artist_images();
    n = metro_music_artist_image_count();
    s_phase_total = n > 0 ? n : 0;
    live_reset();
    metro_settings_artists_dir(dir, sizeof(dir));
    for (i = 0; i < n; i++)
    {
        bool worked = false;

        s_phase_done = i;
        if (s_pass_requested)
            return;
        if (!metro_music_artist_image_at(i, filename, sizeof(filename), &mtime))
            continue;
        metro_music_artist_image_master_key(filename, mtime, key, sizeof(key));
        live_add(key);

        metro_master_art_lock();
        if (metro_master_art_probe("artists", key) == METRO_MASTER_ART_MISSING)
        {
            fb_data *master = metro_master_art_scratch();
            snprintf(path, sizeof(path), "%s/%s", dir, filename);
            if (metro_thumbs_decode_jpeg_cover(path, master, METRO_MASTER_ART_ARTIST_PX))
                metro_master_art_write("artists", key, master, METRO_MASTER_ART_ARTIST_PX);
            else
                metro_master_art_write_none("artists", key);
            worked = true;
        }
        metro_master_art_unlock();
        if (worked)
            wait_turn();
        else
            yield();
    }
    live_sweep("artists", n);
}

/* --- photos ------------------------------------------------------------ */

/* Walks /Photos one entry at a time (no 500-entry name table: the
 * grid's own list is the UI's, this thread pays only one DIR handle,
 * held across the sleeps). mtime from dir_get_info(), same source as
 * metro_fsutil_list_by_ext_mtime() -- so the key matches the grid's. */
static void build_photos(void)
{
    char key[METRO_MASTER_ART_KEY_LEN];
    char path[MAX_PATH];
    DIR *d;
    struct DIRENT *e;
    int total = 0;
    bool aborted = false;

    d = opendir(METRO_PHOTOS_DIR);
    if (!d)
        return;
    live_reset();
    while ((e = readdir(d)) != NULL)
    {
        bool worked = false;
        long mtime;

        if (s_pass_requested)
        {
            aborted = true;
            break;
        }
        if (!metro_photos_name_is_photo(e->d_name))
            continue;
        mtime = dir_get_info(d, e).mtime;
        total++;
        s_phase_done = total;
        metro_photos_master_key(e->d_name, mtime, key, sizeof(key));
        live_add(key);

        metro_master_art_lock();
        if (metro_master_art_probe("photos", key) == METRO_MASTER_ART_MISSING)
        {
            fb_data *master = metro_master_art_scratch();
            snprintf(path, sizeof(path), "%s/%s", METRO_PHOTOS_DIR, e->d_name);
            if (metro_thumbs_decode_jpeg_cover(path, master, METRO_MASTER_ART_PHOTO_PX))
                metro_master_art_write("photos", key, master, METRO_MASTER_ART_PHOTO_PX);
            else
                metro_master_art_write_none("photos", key);
            worked = true;
        }
        metro_master_art_unlock();
        if (worked)
            wait_turn();
        else
            yield();
    }
    closedir(d);
    if (!aborted)
        live_sweep("photos", total);
}

/* --- the thread -------------------------------------------------------- */

static void builder_thread(void)
{
    bool albums_pending = false;

    for (;;)
    {
        if (!s_pass_requested && !albums_pending)
        {
            sleep(HZ / 2);
            continue;
        }
        if (!may_run())
        {
            sleep(HZ / 4);
            continue;
        }
        if (s_pass_requested)
        {
            s_pass_requested = false;
            DEBUGF("metro_art: pass start\n");
            s_phase = METRO_MASTER_ART_PHASE_ALBUMS;
            s_phase_done = 0; s_phase_total = 0;
            albums_pending = !build_albums();
            if (!s_pass_requested)
            {
                s_phase = METRO_MASTER_ART_PHASE_ARTISTS;
                s_phase_done = 0; s_phase_total = 0;
                build_artists();
            }
            if (!s_pass_requested)
            {
                s_phase = METRO_MASTER_ART_PHASE_PHOTOS;
                s_phase_done = 0; s_phase_total = 0;
                build_photos();
            }
            s_phase = METRO_MASTER_ART_PHASE_IDLE;
            /* M-100: only a pass that walked EVERYTHING counts. A newer
             * request superseded this one, or the database was not
             * usable and albums have to be retried -- neither is a
             * finished preparation, and the screen must keep waiting. */
            if (!s_pass_requested && !albums_pending)
                s_pass_done = true;
            DEBUGF("metro_art: pass end (albums %s)\n",
                   albums_pending ? "pending: database not usable yet" : "done");
        }
        else
        {
            /* The database came up after the pass (bootstrap rebuild,
             * first boot): albums only. */
            s_phase = METRO_MASTER_ART_PHASE_ALBUMS;
            if (build_albums())
            {
                albums_pending = false;
                /* M-100: la base llego tarde (rebuild de primer arranque):
                 * las otras dos fases ya corrieron en la pasada anterior,
                 * asi que con los albumes hechos la preparacion esta
                 * completa. */
                s_phase = METRO_MASTER_ART_PHASE_IDLE;
                if (!s_pass_requested)
                    s_pass_done = true;
            }
            else
            {
                s_phase = METRO_MASTER_ART_PHASE_IDLE;
                sleep(HZ);
            }
        }
    }
}

void metro_master_art_builder_init(void)
{
    if (s_started)
        return;
    s_started = true;
    s_last_input_tick = current_tick;
    s_pass_requested = true;
    create_thread(builder_thread, s_stack, sizeof(s_stack), 0, s_thread_name
                  IF_PRIO(, PRIORITY_BACKGROUND) IF_COP(, CPU));
}

void metro_master_art_builder_pause(bool paused)
{
    s_paused = paused;
    if (paused)
    {
        /* Barrier: an element in flight holds the lock; taking it
         * once means it has finished before the animation starts. */
        metro_master_art_lock();
        metro_master_art_unlock();
    }
}

void metro_master_art_builder_note_input(void)
{
    s_last_input_tick = current_tick;
}

void metro_master_art_builder_request_pass(void)
{
    s_pass_requested = true;
}

/* -- M-100: explicit preparation ---------------------------------------- */

void metro_master_art_builder_set_foreground(bool foreground)
{
    s_foreground = foreground;
}

bool metro_master_art_builder_progress(metro_master_art_phase_t *phase,
                                        int *done, int *total)
{
    if (phase)
        *phase = s_phase;
    if (done)
        *done = s_phase_done;
    if (total)
        *total = s_phase_total;
    return s_started && s_phase != METRO_MASTER_ART_PHASE_IDLE;
}

bool metro_master_art_builder_pass_done(void)
{
    return s_pass_done;
}

bool metro_master_art_builder_is_running(void)
{
    return s_started;
}

void metro_master_art_builder_begin_full_pass(void)
{
    s_pass_done = false;
    metro_master_art_builder_init(); /* no-op if the thread already exists */
    s_pass_requested = true;
}

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

/* tagcache.h checks "#ifdef HAVE_TAGCACHE" before including config.h
 * itself (that macro comes from config.h via config/ipod6g.h); if
 * tagcache.h were the first header in this file, HAVE_TAGCACHE would
 * not exist yet and its whole contents would silently disappear. Same
 * gotcha as Aura-Firmware's aura_music.c -- see DECISIONS.md M-030. */
#include "config.h"
#include "tagcache.h"
#include "playlist.h"
#include "playlist_catalog.h"
#include "audio.h"
#include "sound.h"
#include "settings.h"
#include "dir.h"
#include "file.h"
#include "misc.h" /* read_line() */
#include "string-extra.h"
#include "kernel.h" /* current_tick, for playlist_randomise()'s seed */
#include "crc32.h"  /* crc_32() -- album art key, M-096 */

#include "metro_music.h"
#include "metro_lang.h"
#include "metro_sync.h"
#include "metro_settings.h"
#include "metro_artist_images.h"
#include "metro_fsutil.h"
#include "metro_volume.h"
#include "metro_thumbs.h" /* metro_thumbs_mark_dirty() -- M-096 */
#include "metro_master_art.h" /* the tagcache/decode lock -- M-097 */

/* M-097: every tagcache walk in this file runs under the master-art
 * lock. The static scratch it shares (s_uniqbuf, the key memo, the
 * artist image tables) is touched from the UI thread AND from the
 * background master builder; tagcache_get_next() can yield on a disk
 * read (tag_filename lives on disk), so two walks could interleave.
 * The lock is recursive, so nested calls within this file are fine. */
#define LOCK()   metro_master_art_lock()
#define UNLOCK() metro_master_art_unlock()

/* Enough unique values for a few thousand artists/albums/genres --
 * same size Aura-Firmware settled on for the same purpose (D-021).
 * tagcache_search_set_uniqbuf() only needs this for tags with a small
 * unique-value space (artist/album/genre); it ignores it for tag_title. */
static uint32_t s_uniqbuf[2048];

static bool s_scan_triggered = false;
static bool s_update_triggered = false;
static bool s_bootstrap_sealed = false; /* M-095 */

/* M-096: album art key memo (metro_music_album_art_key()) -- cleared
 * whenever the album list is re-read, since seeks may be renumbered. */
#define ART_KEY_MEMO_N 64
static struct { int32_t seek; uint32_t crc; long mtime; } s_art_key_memo[ART_KEY_MEMO_N];
static int s_art_key_memo_n = 0, s_art_key_memo_ring = 0;

/* Forward decl -- defined below, called from metro_music_db_ready()
 * above its own definition (M-098: no longer a public function, see
 * its doc comment further down). */
static void metro_music_bootstrap_tick(void);

bool metro_music_is_playing(void)
{
    return (audio_status() & AUDIO_STATUS_PLAY) != 0;
}

void metro_music_playpause(void)
{
    /* El orden importa: una pista pausada tiene AMBOS bits
     * (AUDIO_STATUS_PLAY | AUDIO_STATUS_PAUSE) en Rockbox, así que
     * preguntar por PLAY primero nunca reanudaría nada. Preguntar por
     * PAUSE primero es lo que ya hacían Now Playing y el visor; se
     * conserva tal cual, no se "arregla" al centralizarlo. */
    if (audio_status() & AUDIO_STATUS_PAUSE)
        audio_resume();
    else if (audio_status() & AUDIO_STATUS_PLAY)
        audio_pause();
}

int metro_music_volume_level(void)
{
    return metro_volume_level_from_db(global_status.volume,
                                      sound_min(SOUND_VOLUME),
                                      sound_max(SOUND_VOLUME));
}

void metro_music_volume_step(int delta)
{
    int level = metro_music_volume_level() + delta;
    int db;

    if (level < 0)
        level = 0;
    if (level > METRO_VOLUME_MAX_LEVEL)
        level = METRO_VOLUME_MAX_LEVEL;

    db = metro_volume_db_from_level(level, sound_min(SOUND_VOLUME),
                                    sound_max(SOUND_VOLUME));
    /* Same path as apps/misc.c adjust_volume(): write the status value
     * and let setvol() clamp against volume_limit, push it to the codec
     * and mark status dirty for persistence. */
    global_status.volume = db;
    setvol();
}

int metro_music_volume_limit_level(void)
{
    return metro_volume_level_from_db(global_settings.volume_limit,
                                      sound_min(SOUND_VOLUME),
                                      sound_max(SOUND_VOLUME));
}

void metro_music_set_volume_limit_level(int level)
{
    if (level < 0)
        level = 0;
    if (level > METRO_VOLUME_MAX_LEVEL)
        level = METRO_VOLUME_MAX_LEVEL;

    global_settings.volume_limit = metro_volume_db_from_level(
        level, sound_min(SOUND_VOLUME), sound_max(SOUND_VOLUME));
    settings_save();
    /* Re-apply: sound_set_volume() clamps the current volume to the
     * new limit, so lowering the limit below the current level takes
     * effect right away instead of at the next wheel move. */
    setvol();
}

bool metro_music_now_playing(char *title_out, size_t title_sz,
                              char *sub_out, size_t sub_sz)
{
    struct mp3entry *id3;
    const char *base;

    if (!metro_music_is_playing())
        return false;

    id3 = audio_current_track();
    if (!id3)
        return false;

    if (id3->title)
        strlcpy(title_out, id3->title, title_sz);
    else
    {
        /* id3->path is a fixed char[MAX_PATH], never NULL -- only ever
         * empty. */
        base = strrchr(id3->path, '/');
        strlcpy(title_out, base ? base + 1 : id3->path, title_sz);
    }

    if (id3->artist && id3->album)
        snprintf(sub_out, sub_sz, "%s - %s", id3->artist, id3->album);
    else if (id3->artist)
        strlcpy(sub_out, id3->artist, sub_sz);
    else
        sub_out[0] = '\0';

    return true;
}

bool metro_music_db_ready(void)
{
    /* F6: metro_sync.c owns every tagcache_update()/tagcache_rebuild()
     * driven by an actual Aura Studio sync -- calling either one here
     * too while that job is in flight would race over the same
     * database. This keeps only the "no database at all" bootstrap
     * below, for the case nothing has ever written a sync marker
     * (music copied by hand over USB, no Studio involved). */
    if (metro_sync_job_active())
        return tagcache_is_usable();

    /* Same reasoning as aura_music_db_ready() (D-021): Rockbox only
     * scans the library on its own from the folder-browser "Database >
     * Initialize now" screen, which Metro doesn't have. tagcache_init()
     * decides asynchronously whether a valid database already exists
     * on disk; rebuilding before that decision lands would blow away a
     * database that was actually fine. tagcache_start_scan() is NOT
     * the right call for a first-time build -- its handler bails out
     * if tc_stat.ready is false, it only refreshes an existing one. */
    if (tagcache_is_fully_initialized() && !tagcache_is_usable() && !s_scan_triggered)
    {
        tagcache_rebuild();
        s_scan_triggered = true;
    }

    metro_music_bootstrap_tick();

    /* A database built in an earlier session never otherwise learns
     * about files added since (USB sync, D-206 in Aura-Firmware: files
     * copied over USB, library empty on the device) -- Rockbox only
     * refreshes it from the same folder-browser screen Metro doesn't
     * have. One pass per boot, on the tagcache thread, cheap when
     * nothing changed. tagcache_is_fully_initialized() guards this the
     * same way as the rebuild above: is_usable() can go true before
     * the background "is there already a database" check lands. */
    if (tagcache_is_usable() && tagcache_is_fully_initialized() && !s_update_triggered)
    {
        tagcache_start_scan();
        s_update_triggered = true;
    }

    return tagcache_is_usable();
}

/* M-095 (v15): seals the shared database's stamp once after the
 * bootstrap rebuild metro_music_db_ready() may have started (or when
 * the database has no stamp at all). Cheap no-op otherwise. Called
 * only from metro_music_db_ready() below (M-098) -- it used to be
 * polled directly from metro_main.c's idle loop as its own public
 * function, back when metro_music_db_ready() itself was only ever
 * called from the Music menu's hub_on_select() and so couldn't be
 * trusted to run on its own every idle tick; now that the idle loop
 * polls metro_music_db_ready() once a second regardless of which
 * screen is up (M-098), that call already carries this one along
 * for free, on the same cadence, with the same metro_sync_job_active()
 * early-out. */
static void metro_music_bootstrap_tick(void)
{
    /* The bootstrap rebuild in metro_music_db_ready() never goes
     * through metro_sync.c's finish_ok(), so nothing recorded which
     * library it describes -- and the first firmware switch rebuilt
     * the (now shared) database again for nothing. Seal it once, the
     * first time it comes up usable after that rebuild. Same for a
     * database that simply has no stamp yet (migrated from a pre-v12
     * tree, or built by an older build): with no sync marker in play
     * (state IDLE -- a pending job never gets here because db_ready()
     * cedes to it, an errored marker keeps forcing its own rebuild
     * regardless of the stamp) the once-per-boot tagcache_start_scan()
     * is what makes it current, so it describes the library on disk.
     * The album thumbnails are keyed by track path (M-096) so none of
     * them need touching -- only the orphan sweep is flagged. */
    if (s_bootstrap_sealed || metro_sync_job_active())
        return;
    if (!s_scan_triggered && !s_update_triggered)
        return; /* db_ready() hasn't run yet this session: nothing to judge */
    if (!tagcache_is_usable() || !tagcache_is_fully_initialized())
        return;
    if (metro_sync_state() != METRO_SYNC_IDLE)
        return;
    if (s_scan_triggered || !metro_sync_db_stamp_present())
    {
        metro_sync_record_db_stamp();
        if (s_scan_triggered)
            metro_thumbs_mark_dirty(); /* a rebuild may have dropped albums */
    }
    s_bootstrap_sealed = true;
}

static enum metro_lang_id untagged_label_for(int tag)
{
    switch (tag)
    {
    case tag_artist: return LANG_UNKNOWN_ARTIST;
    case tag_genre:  return LANG_UNKNOWN_GENRE;
    case tag_title:  return LANG_UNKNOWN_TITLE;
    default:         return LANG_UNKNOWN_ALBUM;
    }
}

/* Filename without path or extension -- title fallback for a track
 * with no tag_title (real-world case: CD rips with no ID3 at all). */
static bool title_from_filename(struct tagcache_search *tcs, char *out, size_t outsz)
{
    char path[MAX_PATH];
    const char *base;
    char *dot;

    if (!tagcache_retrieve(tcs, tcs->idx_id, tag_filename, path, sizeof(path)))
        return false;

    base = strrchr(path, '/');
    base = base ? base + 1 : path;
    if (!base[0])
        return false;

    strlcpy(out, base, outsz);
    dot = strrchr(out, '.');
    if (dot && dot != out)
        *dot = '\0';
    return out[0] != '\0';
}

/* R4 (M-079): el plegado de acentos vive en metro_lang.c -- puro y
 * host-testeable, a diferencia de este archivo. Antes esto recorría
 * bytes y mandaba cualquier inicial acentuada detrás de la Z. */
static int label_cmp(const char *a, const char *b)
{
    return metro_lang_collate(a, b);
}

static void format_duration(char *out, size_t outsz, long ms)
{
    int total_s = ms > 0 ? (int)(ms / 1000) : 0;
    snprintf(out, outsz, "%d:%02d", total_s / 60, total_s % 60);
}

/* Scratch for run_search()'s two sort orders -- static, not on the
 * 8KB UI thread stack (same D-226 concern Aura-Firmware documents for
 * its own equivalent buffers). */
static long s_tracknum[METRO_MUSIC_MAX_SONGS];

static void sort_by_label(metro_music_item_t *items, int n)
{
    int a, b;

    for (a = 1; a < n; a++)
    {
        metro_music_item_t key = items[a];
        b = a - 1;
        while (b >= 0 && label_cmp(items[b].label, key.label) > 0)
        {
            items[b + 1] = items[b];
            b--;
        }
        items[b + 1] = key;
    }
}

static void sort_by_tracknum(metro_music_item_t *items, int n)
{
    int a, b;

    for (a = 1; a < n; a++)
    {
        metro_music_item_t key = items[a];
        long key_num = s_tracknum[a];
        b = a - 1;
        while (b >= 0 && s_tracknum[b] > key_num)
        {
            items[b + 1] = items[b];
            s_tracknum[b + 1] = s_tracknum[b];
            b--;
        }
        items[b + 1] = key;
        s_tracknum[b + 1] = key_num;
    }
}

/* Single-filter tagcache search -- Metro's page tree is only ever two
 * levels deep (artist -> albums, album/genre -> songs), so unlike
 * Aura-Firmware's run_search() (which combines up to four filters for
 * its deeper composer/genre hierarchy) this only ever needs one.
 * filter_tag < 0 means unfiltered. */
static int run_search(int tag, int filter_tag, int32_t filter_seek,
                       metro_music_item_t *out, int max)
{
    struct tagcache_search tcs;
    char buf[TAGCACHE_BUFSZ];
    int n = 0;
    bool album_order = (tag == tag_title && filter_tag == tag_album);

    if (!tagcache_is_usable())
        return 0;
    LOCK();
    if (!tagcache_search(&tcs, tag))
    {
        UNLOCK();
        return 0;
    }

    tagcache_search_set_uniqbuf(&tcs, s_uniqbuf, sizeof(s_uniqbuf));
    if (filter_tag >= 0)
        tagcache_search_add_filter(&tcs, filter_tag, filter_seek);

    while (n < max && tagcache_get_next(&tcs, buf, sizeof(buf)))
    {
        if (!strcmp(buf, UNTAGGED))
        {
            if (tag == tag_title && title_from_filename(&tcs, out[n].label, METRO_MUSIC_ITEM_LEN))
                ; /* filename already written */
            else
                strlcpy(out[n].label, metro_lang_str(untagged_label_for(tag)),
                        METRO_MUSIC_ITEM_LEN);
        }
        else
            strlcpy(out[n].label, buf, METRO_MUSIC_ITEM_LEN);

        out[n].seek = (tag == tag_title) ? tcs.idx_id : tcs.result_seek;
        out[n].subtitle[0] = '\0';
        if (tag == tag_title)
        {
            format_duration(out[n].subtitle, sizeof(out[n].subtitle),
                             tagcache_get_numeric(&tcs, tag_length));
            s_tracknum[n] = tagcache_get_numeric(&tcs, tag_tracknumber);
            if (s_tracknum[n] <= 0)
                s_tracknum[n] = 0x7fffffff; /* untracked: after everything, stable */
        }
        n++;
    }

    tagcache_search_finish(&tcs);
    UNLOCK();

    /* Album songs keep disc order (same criterion metro_music_play_songs_of_album()
     * uses to build the playback playlist -- the row picked on screen and the
     * track that starts playing always match). Everything else is alphabetical. */
    if (album_order)
        sort_by_tracknum(out, n);
    else
        sort_by_label(out, n);

    return n;
}

/* Artist label shown under an album row. Prefers tag_albumartist (the
 * correct grouping tag for a compilation whose tracks carry different
 * tag_artist values but one shared album artist); falls back to
 * tag_artist when the track never set it, which is most personal
 * libraries. PLAN_MAESTRO.md S5 F4. [ESTIMADO: no compilation fixture
 * exercises the fallback path end to end, only unit-level confidence
 * that both filters resolve through the same seek mechanism as
 * metro_music_albums_of_artist(), already proven.] */
static void album_artist_label(int32_t album_seek, char *out, size_t outsz)
{
    struct tagcache_search tcs;
    bool found = false;

    out[0] = '\0';
    if (!tagcache_is_usable())
        return;

    if (tagcache_search(&tcs, tag_albumartist))
    {
        tagcache_search_add_filter(&tcs, tag_album, album_seek);
        if (tagcache_get_next(&tcs, out, outsz) && strcmp(out, UNTAGGED) != 0)
            found = true;
        tagcache_search_finish(&tcs);
    }

    if (!found && tagcache_search(&tcs, tag_artist))
    {
        tagcache_search_add_filter(&tcs, tag_album, album_seek);
        if (tagcache_get_next(&tcs, out, outsz))
            found = true;
        tagcache_search_finish(&tcs);
    }

    if (!found)
        out[0] = '\0';
    else if (!strcmp(out, UNTAGGED))
        strlcpy(out, metro_lang_str(LANG_UNKNOWN_ARTIST), outsz);
}

int metro_music_artists(metro_music_item_t *out, int max)
{
    return run_search(tag_artist, -1, 0, out, max);
}

int metro_music_albums(metro_music_item_t *out, int max)
{
    s_art_key_memo_n = 0; /* M-096: seeks may have been renumbered */
    s_art_key_memo_ring = 0;
    int n = run_search(tag_album, -1, 0, out, max);
    int i;

    for (i = 0; i < n; i++)
        album_artist_label(out[i].seek, out[i].subtitle, sizeof(out[i].subtitle));
    return n;
}

/* R3-F4/DD-5 (M-065): Quickplay's own query -- "the N albums with the
 * most recent tag_lastplayed among any of their tracks". No agrupada
 * query exists for this (INVESTIGACION-metro-r3.md D.2): tag_lastplayed
 * is a per-TRACK numeric tag, not something tagcache can group/sort
 * albums by directly. Scans every track -- keyed by tag_filename, not
 * tag_title, on purpose: filenames are inherently unique, so there's
 * no risk of two different tracks silently colliding into one result
 * the way a uniqbuf'd tag_title scan (run_search()'s own pattern)
 * would for two same-named songs on different albums, which would
 * undercount real plays. Aggregates max(lastplayed) per album NAME in
 * a local table (no cap on the SCAN itself, only on how many distinct
 * albums it can hold -- D.2's own concern about METRO_MUSIC_MAX_GROUPS
 * biasing toward the first tracks in index order doesn't apply here,
 * every track gets visited), then resolves each of the top `max`
 * picks back to a real metro_music_albums() entry (its actual
 * tag_album seek, needed for on_select() to filter tracks by it) by
 * matching the album name -- reuses that function's own proven
 * grouping instead of reaching into tagcache's internal seek scheme
 * directly. */
struct metro_music_recent_agg {
    char album[METRO_MUSIC_ITEM_LEN];
    long lastplayed;
};

int metro_music_recent_albums(metro_music_item_t *out, int max)
{
    struct tagcache_search tcs;
    char buf[MAX_PATH];
    /* static: 300 * ~72 =~ 21KB, same D-226 stack concern as the other
     * large tables in this file. */
    static struct metro_music_recent_agg agg[METRO_MUSIC_MAX_GROUPS];
    static metro_music_item_t all_albums[METRO_MUSIC_MAX_GROUPS];
    int agg_n = 0;
    int all_n, i, a, b, out_n;

    if (!tagcache_is_usable())
        return 0;

    LOCK();
    if (!tagcache_search(&tcs, tag_filename))
    {
        UNLOCK();
        return 0;
    }
    tagcache_search_set_uniqbuf(&tcs, s_uniqbuf, sizeof(s_uniqbuf));

    while (tagcache_get_next(&tcs, buf, sizeof(buf)))
    {
        long lastplayed = tagcache_get_numeric(&tcs, tag_lastplayed);
        char album[METRO_MUSIC_ITEM_LEN];

        if (lastplayed <= 0)
            continue; /* never played -- doesn't affect "most recent" */
        if (!tagcache_retrieve(&tcs, tcs.idx_id, tag_album, album, sizeof(album)) ||
            !strcmp(album, UNTAGGED))
            continue; /* no meaningful album to resume into */

        for (i = 0; i < agg_n; i++)
            if (!strcmp(agg[i].album, album))
                break;

        if (i == agg_n)
        {
            if (agg_n >= METRO_MUSIC_MAX_GROUPS)
                continue; /* distinct-album index full -- same 300 cap as everywhere else */
            strlcpy(agg[i].album, album, sizeof(agg[i].album));
            agg[i].lastplayed = lastplayed;
            agg_n++;
        }
        else if (lastplayed > agg[i].lastplayed)
        {
            agg[i].lastplayed = lastplayed;
        }
    }
    tagcache_search_finish(&tcs);
    UNLOCK();

    /* Insertion sort by lastplayed descending -- agg_n capped at 300,
     * same shape as the sort insert_matching_tracks() already does
     * above for a whole library's worth of tracks. */
    for (a = 1; a < agg_n; a++)
    {
        struct metro_music_recent_agg key = agg[a];
        b = a - 1;
        while (b >= 0 && agg[b].lastplayed < key.lastplayed)
        {
            agg[b + 1] = agg[b];
            b--;
        }
        agg[b + 1] = key;
    }

    /* Resolve each pick's real tag_album seek -- see the function's
     * own doc comment above for why. */
    all_n = metro_music_albums(all_albums, METRO_MUSIC_MAX_GROUPS);
    out_n = 0;
    for (i = 0; i < agg_n && out_n < max; i++)
    {
        for (a = 0; a < all_n; a++)
            if (!strcmp(all_albums[a].label, agg[i].album))
            {
                out[out_n] = all_albums[a];
                out_n++;
                break;
            }
    }
    return out_n;
}

int metro_music_songs(metro_music_item_t *out, int max)
{
    return run_search(tag_title, -1, 0, out, max);
}

int metro_music_genres(metro_music_item_t *out, int max)
{
    return run_search(tag_genre, -1, 0, out, max);
}

int metro_music_albums_of_artist(int32_t artist_seek, metro_music_item_t *out, int max)
{
    int n = run_search(tag_album, tag_artist, artist_seek, out, max);
    int i;

    for (i = 0; i < n; i++)
        album_artist_label(out[i].seek, out[i].subtitle, sizeof(out[i].subtitle));
    return n;
}

int metro_music_songs_of_album(int32_t album_seek, metro_music_item_t *out, int max)
{
    return run_search(tag_title, tag_album, album_seek, out, max);
}

bool metro_music_track_path(int32_t idx_id, char *out, size_t outsz)
{
    struct tagcache_search tcs;
    bool ok;

    if (!tagcache_is_usable())
        return false;
    LOCK();
    if (!tagcache_search(&tcs, tag_filename))
    {
        UNLOCK();
        return false;
    }

    ok = tagcache_retrieve(&tcs, idx_id, tag_filename, out, outsz);
    tagcache_search_finish(&tcs);
    UNLOCK();
    return ok;
}

/* M-096: path + tagcache's stored mtime of one track, one search. */
static bool track_path_mtime(int32_t idx_id, char *out, size_t outsz, long *mtime)
{
    struct tagcache_search tcs;
    bool ok;

    if (!tagcache_is_usable())
        return false;
    LOCK();
    if (!tagcache_search(&tcs, tag_filename))
    {
        UNLOCK();
        return false;
    }

    ok = tagcache_retrieve(&tcs, idx_id, tag_filename, out, outsz);
    if (ok)
    {
        tcs.idx_id = idx_id; /* tagcache_get_numeric() reads tcs->idx_id */
        *mtime = tagcache_get_numeric(&tcs, tag_mtime);
    }
    tagcache_search_finish(&tcs);
    UNLOCK();
    return ok;
}

bool metro_music_album_art_key(int32_t album_seek, char *out, size_t outsz)
{
    metro_music_item_t track;
    char path[MAX_PATH];
    uint32_t crc;
    long mtime;
    int i;

    LOCK();
    for (i = 0; i < s_art_key_memo_n; i++)
        if (s_art_key_memo[i].seek == album_seek)
        {
            snprintf(out, outsz, "a-%08lx.%ld",
                     (unsigned long)s_art_key_memo[i].crc, s_art_key_memo[i].mtime);
            UNLOCK();
            return true;
        }

    if (metro_music_songs_of_album(album_seek, &track, 1) < 1 ||
        !track_path_mtime(track.seek, path, sizeof(path), &mtime))
    {
        UNLOCK();
        return false;
    }
    crc = crc_32(path, strlen(path), 0xffffffff);

    i = s_art_key_memo_n < ART_KEY_MEMO_N ? s_art_key_memo_n++ : s_art_key_memo_ring;
    s_art_key_memo_ring = (s_art_key_memo_ring + 1) % ART_KEY_MEMO_N;
    s_art_key_memo[i].seek = album_seek;
    s_art_key_memo[i].crc = crc;
    s_art_key_memo[i].mtime = mtime;

    snprintf(out, outsz, "a-%08lx.%ld", (unsigned long)crc, mtime);
    UNLOCK();
    return true;
}

bool metro_music_album_key_for_track(const char *track_path, char *out, size_t outsz)
{
    struct tagcache_search tcs;
    struct tagcache_search_clause clause;
    char buf[TAGCACHE_BUFSZ];
    int32_t album_seek = -1;

    if (!tagcache_is_usable() || !track_path || !track_path[0])
        return false;

    LOCK();
    if (!tagcache_search(&tcs, tag_album))
    {
        UNLOCK();
        return false;
    }
    tagcache_search_set_uniqbuf(&tcs, s_uniqbuf, sizeof(s_uniqbuf));
    memset(&clause, 0, sizeof(clause));
    clause.tag = tag_filename;
    clause.type = clause_is;
    clause.numeric = false;
    clause.str = (char *)track_path;
    tagcache_search_add_clause(&tcs, &clause);
    if (tagcache_get_next(&tcs, buf, sizeof(buf)))
        album_seek = tcs.result_seek;
    tagcache_search_finish(&tcs);
    UNLOCK();

    if (album_seek < 0)
        return false;
    return metro_music_album_art_key(album_seek, out, outsz);
}

int metro_music_album_seeks(int32_t *out, int max)
{
    struct tagcache_search tcs;
    char buf[TAGCACHE_BUFSZ];
    int n = 0;

    if (!tagcache_is_usable())
        return 0;
    LOCK();
    if (!tagcache_search(&tcs, tag_album))
    {
        UNLOCK();
        return 0;
    }
    tagcache_search_set_uniqbuf(&tcs, s_uniqbuf, sizeof(s_uniqbuf));
    while (n < max && tagcache_get_next(&tcs, buf, sizeof(buf)))
        out[n++] = tcs.result_seek;
    tagcache_search_finish(&tcs);
    UNLOCK();
    return n;
}

int metro_music_songs_of_genre(int32_t genre_seek, metro_music_item_t *out, int max)
{
    return run_search(tag_title, tag_genre, genre_seek, out, max);
}

/* Builds the dynamic playlist for one of the play_* entry points below
 * and inserts every matching track -- same two orderings as
 * run_search() above (disc order for one album, alphabetical
 * otherwise), so the row index chosen on screen is always the track
 * that starts playing. Static scratch: up to 300 tracks' worth of ids
 * (1.2KB) or titles (18.75KB) doesn't fit the 8KB UI thread stack
 * (D-226, same as Aura-Firmware). R5/M-087: song-sized (5,000). */
static bool insert_matching_tracks(int filter_tag, int32_t filter_seek, bool album_order)
{
    struct tagcache_search tcs;
    char path[MAX_PATH];
    static int32_t s_ids[METRO_MUSIC_MAX_SONGS];
    static long s_nums[METRO_MUSIC_MAX_SONGS];
    static char s_titles[METRO_MUSIC_MAX_SONGS][METRO_MUSIC_ITEM_LEN];
    int n = 0, a, b, inserted = 0;

    if (!tagcache_is_usable())
        return false;
    LOCK();
    if (!tagcache_search(&tcs, tag_title))
    {
        UNLOCK();
        return false;
    }

    tagcache_search_set_uniqbuf(&tcs, s_uniqbuf, sizeof(s_uniqbuf));
    if (filter_tag >= 0)
        tagcache_search_add_filter(&tcs, filter_tag, filter_seek);

    playlist_create(NULL, NULL);

    while (n < METRO_MUSIC_MAX_SONGS && tagcache_get_next(&tcs, path, sizeof(path)))
    {
        s_ids[n] = tcs.idx_id;
        if (album_order)
        {
            s_nums[n] = tagcache_get_numeric(&tcs, tag_tracknumber);
            if (s_nums[n] <= 0)
                s_nums[n] = 0x7fffffff;
        }
        else
            strlcpy(s_titles[n], path, METRO_MUSIC_ITEM_LEN);
        n++;
    }
    /* Search stays open on purpose: tagcache_retrieve() below still
     * needs it to resolve each idx_id to a real path. */

    for (a = 1; a < n; a++)
    {
        int32_t key_id = s_ids[a];

        if (album_order)
        {
            long key_num = s_nums[a];
            b = a - 1;
            while (b >= 0 && s_nums[b] > key_num)
            {
                s_ids[b + 1] = s_ids[b];
                s_nums[b + 1] = s_nums[b];
                b--;
            }
            s_ids[b + 1] = key_id;
            s_nums[b + 1] = key_num;
        }
        else
        {
            char key_title[METRO_MUSIC_ITEM_LEN];
            strlcpy(key_title, s_titles[a], METRO_MUSIC_ITEM_LEN);
            b = a - 1;
            while (b >= 0 && label_cmp(s_titles[b], key_title) > 0)
            {
                s_ids[b + 1] = s_ids[b];
                strlcpy(s_titles[b + 1], s_titles[b], METRO_MUSIC_ITEM_LEN);
                b--;
            }
            s_ids[b + 1] = key_id;
            strlcpy(s_titles[b + 1], key_title, METRO_MUSIC_ITEM_LEN);
        }
    }

    for (a = 0; a < n; a++)
    {
        if (tagcache_retrieve(&tcs, s_ids[a], tag_filename, path, sizeof(path)))
        {
            playlist_insert_track(NULL, path, PLAYLIST_INSERT_LAST, false, true);
            inserted++;
        }
    }

    tagcache_search_finish(&tcs);
    UNLOCK();
    return inserted > 0;
}

bool metro_music_play_all_songs(int start_index)
{
    if (!insert_matching_tracks(-1, 0, false))
        return false;
    playlist_start(start_index, 0, 0);
    return true;
}

bool metro_music_play_songs_of_album(int32_t album_seek, int start_index)
{
    if (!insert_matching_tracks(tag_album, album_seek, true))
        return false;
    playlist_start(start_index, 0, 0);
    return true;
}

bool metro_music_play_songs_of_genre(int32_t genre_seek, int start_index)
{
    if (!insert_matching_tracks(tag_genre, genre_seek, false))
        return false;
    playlist_start(start_index, 0, 0);
    return true;
}

bool metro_music_shuffle_all(void)
{
    if (!insert_matching_tracks(-1, 0, false))
        return false;
    playlist_randomise(NULL, current_tick, true);
    playlist_start(0, 0, 0);
    return true;
}

int metro_music_list_playlists(char labels[][METRO_MUSIC_ITEM_LEN], int max)
{
    char dir[MAX_PATH];
    DIR *d;
    struct DIRENT *entry;
    int n = 0;

    catalog_get_directory(dir, sizeof(dir));

    d = opendir(dir);
    if (!d)
        return 0;

    while (n < max && (entry = readdir(d)) != NULL)
    {
        size_t len = strlen(entry->d_name);
        bool is_m3u = (len > 4 && !strcasecmp(entry->d_name + len - 4, ".m3u"));
        bool is_m3u8 = (len > 5 && !strcasecmp(entry->d_name + len - 5, ".m3u8"));

        if (metro_fsutil_is_hidden_name(entry->d_name))
            continue; /* R4/FA-2: `._Mi Lista.m3u8` no es una playlist */
        if (!is_m3u && !is_m3u8)
            continue;

        strlcpy(labels[n], entry->d_name, METRO_MUSIC_ITEM_LEN);
        n++;
    }
    closedir(d);

    return n;
}

void metro_music_playlist_display_name(const char *filename, char *out, size_t outsz)
{
    size_t len;

    strlcpy(out, filename, outsz);
    len = strlen(out);
    if (len > 4 && !strcasecmp(out + len - 4, ".m3u"))
        out[len - 4] = '\0';
    else if (len > 5 && !strcasecmp(out + len - 5, ".m3u8"))
        out[len - 5] = '\0';
}

bool metro_music_play_playlist(int index)
{
    char dir[MAX_PATH];
    /* static: 300*64 = 18.75KB, same D-226 stack concern as above. */
    static char labels[METRO_MUSIC_MAX_GROUPS][METRO_MUSIC_ITEM_LEN];
    int n;

    catalog_get_directory(dir, sizeof(dir));
    n = metro_music_list_playlists(labels, METRO_MUSIC_MAX_GROUPS);
    if (index < 0 || index >= n)
        return false;

    if (playlist_create(dir, labels[index]) == -1)
        return false;

    playlist_start(0, 0, 0);
    return true;
}

/* R3-F3/DD-6 (M-064): artist_images.cfg -- pure index/lookup logic
 * lives in metro_artist_images.c (host-tested), this is just the
 * Rockbox-only file I/O feeding it one line at a time, same
 * streaming-read shape as metro_media_categories.c's load_index()
 * (no reason to hold the whole file in RAM at once). Paired with a
 * scan of artists/ itself (metro_fsutil_list_by_ext_mtime(), already
 * proven by metro_photos.c) so metro_music_artist_image() can also
 * hand back each file's current mtime -- metro_thumbs.c's cache key. */
static struct metro_artist_images s_artist_images;
static char s_artist_image_files[METRO_ARTIST_IMAGES_MAX][METRO_FSUTIL_NAME_LEN];
static long s_artist_image_mtimes[METRO_ARTIST_IMAGES_MAX];
static int s_artist_image_files_n;

void metro_music_reload_artist_images(void)
{
    char path[MAX_PATH];
    /* filename (128) + ": " + artist (64) + margin. */
    char line[256];
    int fd;
    static const char *const jpg_ext[] = { ".jpg" };

    LOCK();
    metro_artist_images_init(&s_artist_images);

    metro_settings_artist_images_cfg_path(path, sizeof(path));
    fd = open(path, O_RDONLY);
    if (fd >= 0)
    {
        while (read_line(fd, line, sizeof(line)) > 0)
            metro_artist_images_add_line(&s_artist_images, line);
        close(fd);
    }
    /* else: no artist_images.cfg -- cfg index stays empty, not an error */

    metro_settings_artists_dir(path, sizeof(path));
    s_artist_image_files_n = metro_fsutil_list_by_ext_mtime(
        path, jpg_ext, 1, s_artist_image_files, s_artist_image_mtimes,
        METRO_ARTIST_IMAGES_MAX);
    UNLOCK();
}

int metro_music_artist_image_count(void)
{
    return s_artist_image_files_n;
}

bool metro_music_artist_image_at(int i, char *filename_out, size_t filename_sz,
                                 long *mtime_out)
{
    bool ok;
    LOCK();
    ok = i >= 0 && i < s_artist_image_files_n;
    if (ok)
    {
        strlcpy(filename_out, s_artist_image_files[i], filename_sz);
        *mtime_out = s_artist_image_mtimes[i];
    }
    UNLOCK();
    return ok;
}

void metro_music_artist_image_master_key(const char *filename, long mtime,
                                         char *out, size_t outsz)
{
    char path[MAX_PATH];
    size_t n;

    metro_settings_artists_dir(path, sizeof(path));
    n = strlen(path);
    snprintf(path + n, sizeof(path) - n, "/%s", filename);
    metro_master_art_format_key('r', crc_32(path, strlen(path), 0xffffffff),
                                mtime, out, outsz);
}

bool metro_music_artist_image(const char *artist_tag, char *filename_out,
                               size_t filename_sz, long *mtime_out)
{
    const char *filename;
    int i;

    LOCK();
    filename = metro_artist_images_lookup(&s_artist_images, artist_tag);
    if (filename)
        for (i = 0; i < s_artist_image_files_n; i++)
            if (!strcmp(s_artist_image_files[i], filename))
            {
                strlcpy(filename_out, filename, filename_sz);
                *mtime_out = s_artist_image_mtimes[i];
                UNLOCK();
                return true;
            }
    UNLOCK();
    return false; /* not mapped, or referenced file doesn't exist on disk right now */
}

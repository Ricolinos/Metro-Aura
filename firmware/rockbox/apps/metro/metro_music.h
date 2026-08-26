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
/* Music library infra: wraps tagcache (apps/tagcache.h) and playlist
 * (apps/playlist.h) -- the screens layer (metro_screen_hub.c) only
 * decides what to query and when, same split as Aura-Firmware's
 * aura_music.c/.h (PLAN_MAESTRO.md S1.1 point 1, S5 F4). Deliberately
 * narrower than Aura's at F4: no composers, no ratings import, no
 * album art precache. Ratings/artist photos were explicitly out of
 * F4's plan, not out of Metro's forever -- both are backlog items
 * ronda 3 picks up: artist photos here (R3-F3/DD-6), ratings import
 * in metro_sync.c (R3-F5/DD-7). Composers remain out of scope.
 */
#ifndef METRO_MUSIC_H
#define METRO_MUSIC_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define METRO_MUSIC_ITEM_LEN      64
#define METRO_MUSIC_SUBTITLE_LEN  40
/* R5 (M-087): capacity of the library lists. They were a single
 * METRO_MUSIC_MAX_ITEMS = 300 (inherited from Aura-Firmware) for every
 * list, and tagcache hands titles back already sorted -- so a 1,200-song
 * library showed exactly the first 300 titles and stopped at the "E",
 * with nothing on screen to say so (owner's report). One cap per kind
 * of list now, sized for a real library on a 64MB device:
 *   - songs (the flat list, a genre's songs, the playlist built when a
 *     song is picked): 5,000 -- 108 bytes each = 540KB per array.
 *   - groups (artists, albums, genres, playlists, an artist's albums,
 *     an album's songs): 2,000 -- 216KB per array.
 * All static (never on the 8KB UI stack, D-226); the whole set is
 * ~3.5MB of .bss, which on this target only shrinks the audio buffer.
 * The tagcache walk that fills them runs against the RAM copy
 * (tagcache_ram, metro_main.c), so a full walk is RAM-speed. */
#define METRO_MUSIC_MAX_SONGS     5000
#define METRO_MUSIC_MAX_GROUPS    2000
/* Kept as the group cap for the few callers that don't care which
 * list they hold (artist image index, playlists). */
#define METRO_MUSIC_MAX_ITEMS     METRO_MUSIC_MAX_GROUPS

typedef struct {
    char label[METRO_MUSIC_ITEM_LEN];
    /* Empty if the row has none. Songs: track duration ("3:24").
     * Albums: artist/albumartist label. Never both -- which one a row
     * carries depends only on which list produced it. */
    char subtitle[METRO_MUSIC_SUBTITLE_LEN];
    /* tagcache seek: result_seek for group tags (artist/album/genre),
     * idx_id for tag_title (a specific track). */
    int32_t seek;
} metro_music_item_t;

/* True once the database can be searched. Triggers the one-time
 * initial build (tagcache_rebuild()) the first time it is called after
 * tagcache confirms there is no usable database on disk -- Rockbox
 * never does this on its own without the folder-browser "Database >
 * Initialize now" screen, which Metro doesn't have (same gap as
 * Aura-Firmware, D-021). Cheap to call on every attempt to enter
 * Music; the caller (metro_screen_hub.c) pushes a plain "updating
 * library..." placeholder page while this returns false. */
bool metro_music_db_ready(void);

/* M-095: seals the shared database's stamp once after the bootstrap
 * rebuild db_ready() may have started (or when the database has no
 * stamp at all). Cheap no-op otherwise; poll from the idle loop. */
void metro_music_bootstrap_tick(void);

bool metro_music_is_playing(void);

/* R4/FA-8 (M-071): alterna reproducción/pausa de lo que esté sonando.
 * **No-op si no hay nada en reproducción** -- pulsar PLAY con la
 * biblioteca detenida no debe arrancar algo por sorpresa, y ese es
 * justamente el caso nuevo que trae esta fase (antes PLAY solo existía
 * en pantallas que presuponen una pista viva).
 *
 * Compartido por las CUATRO pantallas que responden a PLAY: el hub y
 * las listas (nuevas aquí), Now Playing y el visor de fotos. Las dos
 * últimas ya traían esta misma pareja de llamadas copiada palabra por
 * palabra; esta fase habría hecho cuatro copias de lo mismo. */
void metro_music_playpause(void);

/* R5-F3 (M-083): volume on Metro's 00..15 scale (metro_volume.h). No
 * state of its own -- always derived from global_status.volume, which
 * Rockbox persists, via the round-trip-safe mapping. */
int  metro_music_volume_level(void);
/* Moves `delta` levels (clamped to 0..15), applies it through setvol()
 * so Rockbox's own limit/persistence path runs. */
void metro_music_volume_step(int delta);
/* The "volume limit" setting on the same scale. Setting it also
 * re-applies the current volume so it gets clamped immediately, and
 * persists through settings_save(). */
int  metro_music_volume_limit_level(void);
void metro_music_set_volume_limit_level(int level);

/* Current track's visible title/subtitle ("<artist> - <album>"),
 * filled only if metro_music_is_playing(). Returns false (buffers left
 * untouched) otherwise. */
bool metro_music_now_playing(char *title_out, size_t title_sz,
                              char *sub_out, size_t sub_sz);

int metro_music_artists(metro_music_item_t *out, int max);
int metro_music_albums(metro_music_item_t *out, int max);
int metro_music_songs(metro_music_item_t *out, int max);
int metro_music_genres(metro_music_item_t *out, int max);

/* R3-F4/DD-5 (M-065): the `max` albums with the most recent
 * tag_lastplayed among any of their tracks, most recent first --
 * requires global_settings.runtimedb (metro_apply_hygiene() turns it
 * on) to have anything to return at all. Empty (0) on a library with
 * no play history yet, never a partially-wrong guess. */
int metro_music_recent_albums(metro_music_item_t *out, int max);

/* R3-F4/DD-5 (M-065): resolves the real file path for `idx_id` (a
 * metro_music_item_t.seek from a tag_title search, e.g. one
 * metro_music_songs_of_album() returned) -- Quickplay's tile decode
 * needs a real path to hand to metro_albumart_decode_track_cover(),
 * which reads the track's tags itself via get_metadata() rather than
 * trusting anything tagcache already has cached for it. */
bool metro_music_track_path(int32_t idx_id, char *out, size_t outsz);

/* M-096 (contract v15): stable on-disk cache key for an album's cover
 * thumbnail -- "a-<crc32 of the representative track's path, 8 hex>-
 * <that track's mtime as tagcache stores it>". Both halves come from
 * the FILE, not from the database: a tagcache rebuild renumbers every
 * seek (the pre-v15 key), but the path and mtime of the album's first
 * song only change when Studio re-syncs that song -- which is exactly
 * when the cover may have changed. Memoized (small ring) because the
 * grid asks for it on every redraw of every visible tile. Returns
 * false if the album has no resolvable track (or tagcache is down). */
bool metro_music_album_art_key(int32_t album_seek, char *out, size_t outsz);

int metro_music_albums_of_artist(int32_t artist_seek,
                                  metro_music_item_t *out, int max);
int metro_music_songs_of_album(int32_t album_seek,
                                metro_music_item_t *out, int max);
int metro_music_songs_of_genre(int32_t genre_seek,
                                metro_music_item_t *out, int max);

/* Same order as the matching list above (alphabetical, or by track
 * number for an album) -- the row index the user selected on screen is
 * always the same track that starts playing. */
bool metro_music_play_all_songs(int start_index);
bool metro_music_play_songs_of_album(int32_t album_seek, int start_index);
bool metro_music_play_songs_of_genre(int32_t genre_seek, int start_index);
bool metro_music_shuffle_all(void);

int metro_music_list_playlists(char labels[][METRO_MUSIC_ITEM_LEN], int max);
bool metro_music_play_playlist(int index);
void metro_music_playlist_display_name(const char *filename, char *out,
                                        size_t outsz);

/* R3-F3/DD-6 (M-064): artist_images.cfg, Studio's index mapping an
 * artist tag to a cached photo filename under artists/
 * (CONTRATO-firmware-studio.md §D.3), plus a scan of artists/ itself
 * for each listed file's current mtime (metro_thumbs.c's cache
 * invalidation key, DD-1 -- same "<name>.<mtime>" scheme as photos and
 * quickplay). Re-parses/re-scans fresh from disk -- same "refresh on
 * enter" rule metro_photos_list() and the rest of this file's lists
 * already follow (DESVIACIONES.md F6-1) -- call once when the Music
 * page is entered, before any artist row/tile is drawn. */
void metro_music_reload_artist_images(void);

/* Resolves `artist_tag` (an exact match against the raw tag string,
 * same one metro_music_artists() already exposes as
 * metro_music_item_t.label) to its image: writes the filename
 * (relative to artists/, NOT a full path) into filename_out and its
 * current mtime into *mtime_out. Returns false if artist_images.cfg
 * has no entry for this tag, OR the file it names doesn't exist on
 * disk right now ("archivo referenciado ausente -> placeholder, no
 * error", B.1) -- either way the caller falls back to the accent-tile
 * placeholder, same as metro_thumbs_get() returning NULL for any other
 * reason. */
bool metro_music_artist_image(const char *artist_tag, char *filename_out,
                               size_t filename_sz, long *mtime_out);

#endif /* METRO_MUSIC_H */

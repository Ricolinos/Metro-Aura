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
#include <stdbool.h>
#include <string.h>
#include "lcd.h"
#include "string-extra.h"
#include "fs_defines.h"

#include "metro_screen_hub.h"
#include "metro_screen_list.h"
#include "metro_screen_settings.h"
#include "audio.h"
#include "backlight.h"
#include "metro_screen_nowplaying.h"
#include "metro_fb.h"
#include "metro_music.h"
#include "metro_video.h"
#include "metro_photos.h"
#include "metro_thumbs.h"
#include "metro_albumart.h" /* R3-F4/DD-5 -- metro_albumart_decode_track_master() */
#include "metro_master_art_format.h" /* METRO_MASTER_ART_*_PX -- M-097 */
#include "metro_fsutil.h"
#include "metro_settings.h"
#include "metro_screen_photo_viewer.h"
#include "metro_draw.h"
#include "metro_theme.h"
#include "metro_lang.h"
#include "metro_keymap.h"

#define METRO_HUB_FIRST_Y 32
#define METRO_HUB_PITCH   52
#define METRO_HUB_VISIBLE 4

static void open_album_songs(int32_t album_seek, const char *album_label);
static void open_genre_songs(int32_t genre_seek, const char *genre_label);

/* --- videos: real files under /Videos (F7) -- category pivots
 * (movies/series/clips) only appear when video_categories.cfg tagged
 * at least one currently-listed file; otherwise just "all"
 * (PLAN_MAESTRO.md S2.2, contract D-316: index absent/empty is a
 * supported case, not an error). video_pivots[]/videos_page.npivots
 * are rebuilt by build_videos_page() each time the videos row is
 * entered -- same reuse-one-static-instance rule as the music
 * subpages (metro_page.h: only one such page can be on the nav stack
 * at a time). */

static metro_video_item_t s_video_all[METRO_VIDEO_MAX];
static int s_video_all_n;
static metro_video_item_t s_video_movies[METRO_VIDEO_MAX];
static int s_video_movies_n;
static metro_video_item_t s_video_series[METRO_VIDEO_MAX];
static int s_video_series_n;
static metro_video_item_t s_video_clips[METRO_VIDEO_MAX];
static int s_video_clips_n;

struct video_pivot_ctx {
    metro_video_item_t *items;
    int *count;
};

static struct video_pivot_ctx video_all_ctx    = { s_video_all,    &s_video_all_n };
static struct video_pivot_ctx video_movies_ctx = { s_video_movies, &s_video_movies_n };
static struct video_pivot_ctx video_series_ctx = { s_video_series, &s_video_series_n };
static struct video_pivot_ctx video_clips_ctx  = { s_video_clips,  &s_video_clips_n };

static int video_pivot_count(void *ctx)
{
    return *((struct video_pivot_ctx *)ctx)->count;
}

static void video_pivot_get_row(void *ctx, int index, struct metro_row *out)
{
    struct video_pivot_ctx *c = ctx;
    out->title = c->items[index].filename;
    out->subtitle = NULL;
    out->kind = METRO_ROW_ACTION;
}

static void video_pivot_on_select(void *ctx, int index)
{
    struct video_pivot_ctx *c = ctx;
    metro_video_play(c->items[index].filename);
}

static struct metro_pivot video_pivots[4];
static struct metro_page videos_page = { LANG_HUB_VIDEOS, video_pivots, 1, NULL };

static void build_videos_page(void)
{
    int i, n = 0;

    s_video_all_n = metro_video_list(s_video_all, METRO_VIDEO_MAX);
    s_video_movies_n = s_video_series_n = s_video_clips_n = 0;
    for (i = 0; i < s_video_all_n; i++)
    {
        switch (s_video_all[i].category)
        {
            case METRO_VIDEO_CAT_MOVIE:  s_video_movies[s_video_movies_n++] = s_video_all[i]; break;
            case METRO_VIDEO_CAT_SERIES: s_video_series[s_video_series_n++] = s_video_all[i]; break;
            case METRO_VIDEO_CAT_CLIP:   s_video_clips[s_video_clips_n++]  = s_video_all[i];  break;
            default: break;
        }
    }

    video_pivots[n++] = (struct metro_pivot){
        LANG_PIVOT_ALL, video_pivot_count, video_pivot_get_row, video_pivot_on_select, &video_all_ctx };
    if (s_video_movies_n || s_video_series_n || s_video_clips_n)
    {
        video_pivots[n++] = (struct metro_pivot){
            LANG_PIVOT_MOVIES, video_pivot_count, video_pivot_get_row, video_pivot_on_select, &video_movies_ctx };
        video_pivots[n++] = (struct metro_pivot){
            LANG_PIVOT_SERIES, video_pivot_count, video_pivot_get_row, video_pivot_on_select, &video_series_ctx };
        video_pivots[n++] = (struct metro_pivot){
            LANG_PIVOT_CLIPS, video_pivot_count, video_pivot_get_row, video_pivot_on_select, &video_clips_ctx };
    }
    videos_page.npivots = n;
}

/* --- photos: real files under /Photos (F7) -- same conditional-pivot
 * rule as videos, against photo_categories.cfg (photo/image/ai). */

static metro_photo_item_t s_photo_all[METRO_PHOTOS_MAX];
static int s_photo_all_n;
static metro_photo_item_t s_photo_photos[METRO_PHOTOS_MAX];
static int s_photo_photos_n;
static metro_photo_item_t s_photo_images[METRO_PHOTOS_MAX];
static int s_photo_images_n;
static metro_photo_item_t s_photo_ai[METRO_PHOTOS_MAX];
static int s_photo_ai_n;

struct photo_pivot_ctx {
    metro_photo_item_t *items;
    int *count;
};

static struct photo_pivot_ctx photo_all_ctx    = { s_photo_all,    &s_photo_all_n };
static struct photo_pivot_ctx photo_photos_ctx = { s_photo_photos, &s_photo_photos_n };
static struct photo_pivot_ctx photo_images_ctx = { s_photo_images, &s_photo_images_n };
static struct photo_pivot_ctx photo_ai_ctx     = { s_photo_ai,     &s_photo_ai_n };

static int photo_pivot_count(void *ctx)
{
    return *((struct photo_pivot_ctx *)ctx)->count;
}

static void photo_pivot_get_row(void *ctx, int index, struct metro_row *out)
{
    struct photo_pivot_ctx *c = ctx;
    out->title = c->items[index].filename;
    out->subtitle = NULL;
    out->kind = METRO_ROW_ACTION;
}

static void photo_pivot_on_select(void *ctx, int index)
{
    /* R2-F3/DD-10 (M-058): Metro's own viewer, not imageviewer.rock
     * anymore -- browses the SAME array this pivot draws from, so
     * SCROLL_FWD/BACK inside the viewer moves through this exact
     * category, not the unfiltered "todos" list. */
    struct photo_pivot_ctx *c = ctx;
    metro_screen_photo_viewer_push(c->items, *c->count, index);
}

/* R3-F1/DD-1: Photos' own metro_thumb_source -- key is "<filename>.
 * <mtime>" (the same pair the old metro_photo_thumbs.c used to
 * invalidate a stale cache entry), decode is a plain JPEG-cover read
 * from /Photos/<filename> via the engine's shared helper. */
#define PHOTOS_DIR METRO_PHOTOS_DIR

static bool photo_thumb_cache_key(void *ctx, int index, char *out, size_t out_len)
{
    struct photo_pivot_ctx *c = ctx;
    if (index < 0 || index >= *c->count)
        return false;
    snprintf(out, out_len, "%s.%ld", c->items[index].filename, c->items[index].mtime);
    return true;
}

/* M-097 (contract v16): master key "p-<crc32 of /Photos/<file>>.<mtime>"
 * -- the .mth key above stays "<file>.<mtime>" (moonlit shares those
 * files under that name); the shared master is a separate file. */
static bool photo_thumb_master_key(void *ctx, int index, char *out, size_t out_len)
{
    struct photo_pivot_ctx *c = ctx;
    if (index < 0 || index >= *c->count)
        return false;
    metro_photos_master_key(c->items[index].filename, c->items[index].mtime, out, out_len);
    return true;
}

static bool photo_thumb_decode(void *ctx, int index, fb_data *dst)
{
    struct photo_pivot_ctx *c = ctx;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", PHOTOS_DIR, c->items[index].filename);
    return metro_thumbs_decode_jpeg_cover(path, dst, METRO_MASTER_ART_PHOTO_PX);
}

static const struct metro_thumb_source photo_thumb_source = {
    "photos", photo_thumb_cache_key, photo_thumb_master_key, photo_thumb_decode
};

/* R2-F2/DD-7/DD-9: bitmap for the grid's get_tile() -- delegates
 * straight to the (now generic) thumbnail engine. */
static const fb_data *photo_pivot_get_tile(void *ctx, int index)
{
    return metro_thumbs_get(&photo_thumb_source, ctx, index);
}

static struct metro_pivot photo_pivots[4];
static struct metro_page photos_page = { LANG_HUB_PHOTOS, photo_pivots, 1, NULL };

static void build_photos_page(void)
{
    int i, n = 0;

    /* R2-F2/DD-9: a fresh visit to Photos always starts the thumbnail
     * engine clean -- otherwise the RAM window/pending queue could
     * still be serving or decoding for a pivot the user isn't even
     * looking at anymore (e.g. left "ia" scrolled deep, came back into
     * "todos"). */
    metro_thumbs_reset();

    s_photo_all_n = metro_photos_list(s_photo_all, METRO_PHOTOS_MAX);
    s_photo_photos_n = s_photo_images_n = s_photo_ai_n = 0;
    for (i = 0; i < s_photo_all_n; i++)
    {
        switch (s_photo_all[i].category)
        {
            case METRO_PHOTO_CAT_PHOTO: s_photo_photos[s_photo_photos_n++] = s_photo_all[i]; break;
            case METRO_PHOTO_CAT_IMAGE: s_photo_images[s_photo_images_n++] = s_photo_all[i]; break;
            case METRO_PHOTO_CAT_AI:    s_photo_ai[s_photo_ai_n++]         = s_photo_all[i]; break;
            default: break;
        }
    }

    photo_pivots[n++] = (struct metro_pivot){
        LANG_PIVOT_ALL, photo_pivot_count, photo_pivot_get_row, photo_pivot_on_select, &photo_all_ctx,
        METRO_TILE_COLS, photo_pivot_get_tile };
    if (s_photo_photos_n || s_photo_images_n || s_photo_ai_n)
    {
        photo_pivots[n++] = (struct metro_pivot){
            LANG_PIVOT_PHOTOS, photo_pivot_count, photo_pivot_get_row, photo_pivot_on_select, &photo_photos_ctx,
            METRO_TILE_COLS, photo_pivot_get_tile };
        photo_pivots[n++] = (struct metro_pivot){
            LANG_PIVOT_IMAGES, photo_pivot_count, photo_pivot_get_row, photo_pivot_on_select, &photo_images_ctx,
            METRO_TILE_COLS, photo_pivot_get_tile };
        photo_pivots[n++] = (struct metro_pivot){
            LANG_PIVOT_AI, photo_pivot_count, photo_pivot_get_row, photo_pivot_on_select, &photo_ai_ctx,
            METRO_TILE_COLS, photo_pivot_get_tile };
    }
    photos_page.npivots = n;
}

/* --- music: real tagcache-backed lists (F4) -----------------------------
 * Each top-level pivot (artists/albums/songs/genres/playlists) caches
 * its query result in a static array, refreshed once when the user
 * enters Music (music_lists_refresh()) -- count()/get_row() just index
 * into it, no tagcache traffic on every redraw/keypress. Drilling into
 * an artist or an album/genre reuses ONE shared subpage per kind
 * (single path down the twist stack at a time, see metro_page.h), its
 * cache overwritten by open_album_songs()/open_genre_songs() right
 * before each push. */

/* R3-F4/DD-5 (M-065), DA-2: 8 tiles -- two full METRO_TILE_COLS rows,
 * the plan's recommended default over 4 (one row, feels thin for a
 * landing pivot) or 12 (a third partial row most screens won't even
 * scroll to). */
#define METRO_QUICKPLAY_MAX 8
static metro_music_item_t s_quickplay[METRO_QUICKPLAY_MAX];
static int s_quickplay_n;

static metro_music_item_t s_artists[METRO_MUSIC_MAX_GROUPS];
static int s_artists_n;
static metro_music_item_t s_albums[METRO_MUSIC_MAX_GROUPS];
static int s_albums_n;
static metro_music_item_t s_songs[METRO_MUSIC_MAX_SONGS];
static int s_songs_n;
static metro_music_item_t s_genres[METRO_MUSIC_MAX_GROUPS];
static int s_genres_n;
static char s_playlist_files[METRO_MUSIC_MAX_GROUPS][METRO_MUSIC_ITEM_LEN];
static char s_playlist_names[METRO_MUSIC_MAX_GROUPS][METRO_MUSIC_ITEM_LEN];
static int s_playlists_n;

static void album_thumbs_gc_if_dirty(void);

static void music_lists_refresh(void)
{
    int i;

    /* R3-F3/DD-6 (M-064): same reason build_photos_page() resets
     * first -- the thumbnail engine's RAM window is shared across
     * every tile grid in the app (DD-1), so a fresh visit to Music
     * (whose Artists pivot is now one) shouldn't keep serving/decoding
     * for whatever grid the user was looking at before (Photos, most
     * likely). */
    metro_thumbs_reset();

    /* R3-F4/DD-5 (M-065): empty (0) on a library with no play history
     * yet -- quickplay_pivot's empty_message (LANG_QUICKPLAY_EMPTY)
     * covers that case, not a placeholder row. */
    s_quickplay_n = metro_music_recent_albums(s_quickplay, METRO_QUICKPLAY_MAX);

    s_artists_n = metro_music_artists(s_artists, METRO_MUSIC_MAX_GROUPS);
    s_albums_n  = metro_music_albums(s_albums, METRO_MUSIC_MAX_GROUPS);
    s_songs_n   = metro_music_songs(s_songs, METRO_MUSIC_MAX_SONGS);
    s_genres_n  = metro_music_genres(s_genres, METRO_MUSIC_MAX_GROUPS);

    s_playlists_n = metro_music_list_playlists(s_playlist_files, METRO_MUSIC_MAX_GROUPS);
    for (i = 0; i < s_playlists_n; i++)
        metro_music_playlist_display_name(s_playlist_files[i], s_playlist_names[i],
                                           METRO_MUSIC_ITEM_LEN);

    /* R3-F3/DD-6 (M-064): same "refresh on enter" rule as the lists
     * above -- a photo added/changed by Studio mid-session shows up
     * next time Music is (re)entered, not stale from boot. */
    metro_music_reload_artist_images();

    album_thumbs_gc_if_dirty(); /* M-096 */
}

/* pivot: quickplay -> the most-recently-played albums (DA-1: first
 * pivot of Música, the plan's recommended default -- the Zune-style
 * "you'll probably want to keep listening to this" landing surface).
 * on_select drills into that album's songs, same as the albums pivot
 * below.
 *
 * R4/FA-5b (M-074): la fuente de miniaturas dejó de ser exclusiva de
 * Quickplay. El pivot Álbumes necesita exactamente lo mismo -- una
 * cuadrícula de álbumes con su carátula real -- así que en vez de
 * duplicar el par cache_key/decode, la fuente lee AHORA la lista desde
 * el `ctx` del pivot (el campo que struct metro_pivot ya tenía y que
 * estos proveedores ignoraban). Una sola implementación sirve a los
 * dos, y a cualquier cuadrícula de álbumes futura.
 *
 * R3-F4/DD-5 (M-065): la carátula se resuelve desde un track
 * REPRESENTATIVO del álbum (su primera canción, en el mismo orden que
 * metro_music_songs_of_album() ya devuelve), no desde
 * audio_current_track() -- puede no haber nada sonando. */
struct album_grid {
    const metro_music_item_t *items;
    const int *count;   /* puntero: la lista se re-llena en cada visita */
};

static bool album_thumb_cache_key(void *ctx, int index, char *out, size_t out_len)
{
    const struct album_grid *g = (const struct album_grid *)ctx;

    if (!g || index < 0 || index >= *g->count)
        return false;

    /* La clave es el álbum (no la casilla de la cuadrícula), así que
     * Quickplay y Álbumes COMPARTEN la caché en disco. M-096: ya no es
     * el seek de tagcache (cambiaba en cada reconstrucción y dejaba
     * carátulas huérfanas o cruzadas) sino crc32 de la ruta de la
     * pista representativa + su mtime -- estable entre rebuilds y
     * entre familias de firmware (contrato v15, /.aura/thumbs). */
    return metro_music_album_art_key(g->items[index].seek, out, out_len);
}

static bool album_thumb_decode(void *ctx, int index, fb_data *dst)
{
    const struct album_grid *g = (const struct album_grid *)ctx;
    metro_music_item_t track;
    char path[MAX_PATH];

    if (!g || index < 0 || index >= *g->count)
        return false;
    if (metro_music_songs_of_album(g->items[index].seek, &track, 1) < 1)
        return false;
    if (!metro_music_track_path(track.seek, path, sizeof(path)))
        return false;

    /* M-097: decodes the 130px MASTER (the engine derives the tile). */
    return metro_albumart_decode_track_master(path, dst);
}

/* M-097: the album's master key IS its cache key (a-<crc>.<mtime>,
 * M-096) -- one string, shared by the .mth and the .art. */
static const struct metro_thumb_source album_thumb_source = {
    "albums", album_thumb_cache_key, album_thumb_cache_key, album_thumb_decode
};

static const fb_data *album_pivot_get_tile(void *ctx, int index)
{
    return metro_thumbs_get(&album_thumb_source, ctx, index);
}

static const struct album_grid s_quickplay_grid = { s_quickplay, &s_quickplay_n };
static const struct album_grid s_albums_grid    = { s_albums,    &s_albums_n };

/* M-096: orphan sweep of albums/ -- only when something raised the
 * dirty flag (sync with music, bootstrap rebuild, cache relocation),
 * and only here, where s_albums was just re-read. */
static void album_thumbs_gc_if_dirty(void)
{
    if (metro_thumbs_take_dirty())
        metro_thumbs_gc(&album_thumb_source, (void *)&s_albums_grid, s_albums_n);
}

static int quickplay_count(void *ctx) { (void)ctx; return s_quickplay_n; }

static void quickplay_get_row(void *ctx, int index, struct metro_row *out)
{
    (void)ctx;
    out->title = s_quickplay[index].label;
    out->subtitle = s_quickplay[index].subtitle[0] ? s_quickplay[index].subtitle : NULL;
    out->kind = METRO_ROW_NAV;
}

static void quickplay_on_select(void *ctx, int index)
{
    (void)ctx;
    open_album_songs(s_quickplay[index].seek, s_quickplay[index].label);
}

/* pivot: artists -> on_select drills into that artist's albums, same
 * as before R3-F3. R3-F3/DD-6 (M-064) additionally wires it as a tile
 * grid: get_tile() resolves each row's tag_artist (s_artists[index].label,
 * the exact string metro_music_artist_image() matches against) to an
 * image via the shared thumbnail engine (metro_thumbs.c, DD-1) -- an
 * artist with no mapped/missing photo returns NULL and the grid falls
 * back to its usual accent-tile-with-initial placeholder, no special
 * casing needed here. */
static bool artist_thumb_cache_key(void *ctx, int index, char *out, size_t out_len)
{
    char filename[METRO_FSUTIL_NAME_LEN];
    long mtime;
    (void)ctx;

    if (index < 0 || index >= s_artists_n)
        return false;
    if (!metro_music_artist_image(s_artists[index].label, filename, sizeof(filename), &mtime))
        return false;

    snprintf(out, out_len, "%s.%ld", filename, mtime);
    return true;
}

/* M-097: master key "r-<crc32 of <artists dir>/<file>>.<mtime>". */
static bool artist_thumb_master_key(void *ctx, int index, char *out, size_t out_len)
{
    char filename[METRO_FSUTIL_NAME_LEN];
    long mtime;
    (void)ctx;

    if (index < 0 || index >= s_artists_n)
        return false;
    if (!metro_music_artist_image(s_artists[index].label, filename, sizeof(filename), &mtime))
        return false;
    metro_music_artist_image_master_key(filename, mtime, out, out_len);
    return true;
}

static bool artist_thumb_decode(void *ctx, int index, fb_data *dst)
{
    char filename[METRO_FSUTIL_NAME_LEN];
    long mtime;
    char dir[MAX_PATH], path[MAX_PATH];
    (void)ctx;

    if (!metro_music_artist_image(s_artists[index].label, filename, sizeof(filename), &mtime))
        return false; /* changed between the get_tile() and tick() passes -- skip */

    /* metro_settings_artists_dir() (metro_settings.c) is the only
     * function allowed to build this path -- CLAUDE.md's compat-path
     * rule. */
    metro_settings_artists_dir(dir, sizeof(dir));
    snprintf(path, sizeof(path), "%s/%s", dir, filename);
    return metro_thumbs_decode_jpeg_cover(path, dst, METRO_MASTER_ART_ARTIST_PX);
}

static const struct metro_thumb_source artist_thumb_source = {
    "artists", artist_thumb_cache_key, artist_thumb_master_key, artist_thumb_decode
};

static const fb_data *artist_pivot_get_tile(void *ctx, int index)
{
    return metro_thumbs_get(&artist_thumb_source, ctx, index);
}

static int artists_count(void *ctx) { (void)ctx; return s_artists_n; }

static void artists_get_row(void *ctx, int index, struct metro_row *out)
{
    (void)ctx;
    out->title = s_artists[index].label;
    out->subtitle = NULL;
    out->kind = METRO_ROW_NAV;
}

static metro_music_item_t s_artist_albums[METRO_MUSIC_MAX_GROUPS];
static int s_artist_albums_n;
static char s_artist_albums_title[METRO_MUSIC_ITEM_LEN];

static int artist_albums_count(void *ctx) { (void)ctx; return s_artist_albums_n; }

static void artist_albums_get_row(void *ctx, int index, struct metro_row *out)
{
    (void)ctx;
    out->title = s_artist_albums[index].label;
    out->subtitle = s_artist_albums[index].subtitle[0] ? s_artist_albums[index].subtitle : NULL;
    out->kind = METRO_ROW_NAV;
}

static void artist_albums_on_select(void *ctx, int index)
{
    (void)ctx;
    open_album_songs(s_artist_albums[index].seek, s_artist_albums[index].label);
}

static const struct metro_pivot artist_albums_pivots[] = {
    { LANG_PIVOT_ALBUMS, artist_albums_count, artist_albums_get_row, artist_albums_on_select, NULL },
};
static struct metro_page artist_albums_page = { LANG_PIVOT_ALBUMS, artist_albums_pivots, 1, NULL };

static void artists_on_select(void *ctx, int index)
{
    (void)ctx;
    s_artist_albums_n = metro_music_albums_of_artist(s_artists[index].seek,
                                                       s_artist_albums, METRO_MUSIC_MAX_GROUPS);
    strlcpy(s_artist_albums_title, s_artists[index].label, sizeof(s_artist_albums_title));
    artist_albums_page.title_dynamic = s_artist_albums_title;
    metro_screen_list_push(&artist_albums_page);
}

/* pivot: albums -> on_select drills into that album's songs */

static int albums_count(void *ctx) { (void)ctx; return s_albums_n; }

static void albums_get_row(void *ctx, int index, struct metro_row *out)
{
    (void)ctx;
    out->title = s_albums[index].label;
    out->subtitle = s_albums[index].subtitle[0] ? s_albums[index].subtitle : NULL;
    out->kind = METRO_ROW_NAV;
}

static void albums_on_select(void *ctx, int index)
{
    (void)ctx;
    open_album_songs(s_albums[index].seek, s_albums[index].label);
}

/* R5 (M-087): una lista que llego a su tope lo DICE -- una fila final,
 * terciaria, sin accion. Antes el tope (300) era silencioso y una
 * biblioteca de 1,200 canciones "terminaba en la E" sin pista alguna.
 * Solo para las listas de filas; las cuadriculas (artistas, albumes)
 * no reciben una fila extra porque su indice es el de un tile real. */
static int truncated_count(int n, int max)
{
    return n >= max ? n + 1 : n;
}

static bool truncated_row(int index, int n, int max, struct metro_row *out)
{
    if (n >= max && index >= n)
    {
        out->title = metro_lang_str(LANG_LIST_TRUNCATED);
        out->subtitle = NULL;
        out->kind = METRO_ROW_ACTION;
        return true;
    }
    return false;
}

/* pivot: songs (all, alphabetical) -> on_select plays from that row on */

static int songs_count(void *ctx) { (void)ctx; return truncated_count(s_songs_n, METRO_MUSIC_MAX_SONGS); }

static void songs_get_row(void *ctx, int index, struct metro_row *out)
{
    (void)ctx;
    if (truncated_row(index, s_songs_n, METRO_MUSIC_MAX_SONGS, out))
        return;
    out->title = s_songs[index].label;
    out->subtitle = s_songs[index].subtitle[0] ? s_songs[index].subtitle : NULL;
    out->kind = METRO_ROW_ACTION;
}

static void songs_on_select(void *ctx, int index)
{
    (void)ctx;
    if (index >= s_songs_n)
        return; /* la fila "...y mas" no hace nada */
    if (metro_music_play_all_songs(index))
        metro_screen_nowplaying_push();
}

/* pivot: genres -> on_select drills into that genre's songs */

static int genres_count(void *ctx) { (void)ctx; return truncated_count(s_genres_n, METRO_MUSIC_MAX_GROUPS); }

static void genres_get_row(void *ctx, int index, struct metro_row *out)
{
    (void)ctx;
    if (truncated_row(index, s_genres_n, METRO_MUSIC_MAX_GROUPS, out))
        return;
    out->title = s_genres[index].label;
    out->subtitle = NULL;
    out->kind = METRO_ROW_NAV;
}

static void genres_on_select(void *ctx, int index)
{
    (void)ctx;
    if (index >= s_genres_n)
        return; /* la fila "...y mas" no hace nada */
    open_genre_songs(s_genres[index].seek, s_genres[index].label);
}

/* pivot: playlists -> on_select plays the whole saved playlist */

static int playlists_count(void *ctx) { (void)ctx; return s_playlists_n; }

static void playlists_get_row(void *ctx, int index, struct metro_row *out)
{
    (void)ctx;
    out->title = s_playlist_names[index];
    out->subtitle = NULL;
    out->kind = METRO_ROW_ACTION;
}

static void playlists_on_select(void *ctx, int index)
{
    (void)ctx;
    if (metro_music_play_playlist(index))
        metro_screen_nowplaying_push();
}

static const struct metro_pivot music_pivots[] = {
    /* R3-F4/DD-5 (M-065), DA-1: first pivot -- the plan's recommended
     * default (open for the owner to flip at this phase's PARADA). */
    { LANG_PIVOT_QUICKPLAY, quickplay_count, quickplay_get_row, quickplay_on_select,
      (void *)&s_quickplay_grid,
      METRO_TILE_COLS, album_pivot_get_tile, LANG_QUICKPLAY_EMPTY },
    { LANG_PIVOT_ARTISTS,   artists_count,   artists_get_row,   artists_on_select,   NULL,
      METRO_TILE_COLS, artist_pivot_get_tile },
    /* R4/FA-5b (M-074): de lista de texto a cuadrícula con carátula
     * real, reusando la misma fuente que Quickplay. */
    { LANG_PIVOT_ALBUMS,    albums_count,    albums_get_row,    albums_on_select,
      (void *)&s_albums_grid,
      METRO_TILE_COLS, album_pivot_get_tile },
    { LANG_PIVOT_SONGS,     songs_count,     songs_get_row,     songs_on_select,     NULL },
    { LANG_PIVOT_GENRES,    genres_count,    genres_get_row,    genres_on_select,    NULL },
    { LANG_PIVOT_PLAYLISTS, playlists_count, playlists_get_row, playlists_on_select, NULL },
};
static const struct metro_page music_page = { LANG_HUB_MUSIC, music_pivots, 6, NULL };

/* Shared "songs of one album" subpage -- reached from either the
 * top-level albums pivot or an artist's albums (only one such page can
 * ever be on the nav stack at a time, see metro_page.h). */

static metro_music_item_t s_album_songs[METRO_MUSIC_MAX_GROUPS];
static int s_album_songs_n;
static int32_t s_album_songs_seek;
static char s_album_songs_title[METRO_MUSIC_ITEM_LEN];

static int album_songs_count(void *ctx) { (void)ctx; return truncated_count(s_album_songs_n, METRO_MUSIC_MAX_GROUPS); }

static void album_songs_get_row(void *ctx, int index, struct metro_row *out)
{
    (void)ctx;
    if (truncated_row(index, s_album_songs_n, METRO_MUSIC_MAX_GROUPS, out))
        return;
    out->title = s_album_songs[index].label;
    out->subtitle = s_album_songs[index].subtitle[0] ? s_album_songs[index].subtitle : NULL;
    out->kind = METRO_ROW_ACTION;
}

static void album_songs_on_select(void *ctx, int index)
{
    (void)ctx;
    if (index >= s_album_songs_n)
        return; /* la fila "...y mas" no hace nada */
    if (metro_music_play_songs_of_album(s_album_songs_seek, index))
        metro_screen_nowplaying_push();
}

static const struct metro_pivot album_songs_pivots[] = {
    { LANG_PIVOT_SONGS, album_songs_count, album_songs_get_row, album_songs_on_select, NULL },
};
static struct metro_page album_songs_page = { LANG_PIVOT_SONGS, album_songs_pivots, 1, NULL };

static void open_album_songs(int32_t album_seek, const char *album_label)
{
    s_album_songs_seek = album_seek;
    s_album_songs_n = metro_music_songs_of_album(album_seek, s_album_songs, METRO_MUSIC_MAX_GROUPS);
    strlcpy(s_album_songs_title, album_label, sizeof(s_album_songs_title));
    album_songs_page.title_dynamic = s_album_songs_title;
    metro_screen_list_push(&album_songs_page);
}

/* Shared "songs of one genre" subpage -- same reuse rule as above. */

static metro_music_item_t s_genre_songs[METRO_MUSIC_MAX_SONGS];
static int s_genre_songs_n;
static int32_t s_genre_songs_seek;
static char s_genre_songs_title[METRO_MUSIC_ITEM_LEN];

static int genre_songs_count(void *ctx) { (void)ctx; return truncated_count(s_genre_songs_n, METRO_MUSIC_MAX_SONGS); }

static void genre_songs_get_row(void *ctx, int index, struct metro_row *out)
{
    (void)ctx;
    if (truncated_row(index, s_genre_songs_n, METRO_MUSIC_MAX_SONGS, out))
        return;
    out->title = s_genre_songs[index].label;
    out->subtitle = s_genre_songs[index].subtitle[0] ? s_genre_songs[index].subtitle : NULL;
    out->kind = METRO_ROW_ACTION;
}

static void genre_songs_on_select(void *ctx, int index)
{
    (void)ctx;
    if (index >= s_genre_songs_n)
        return; /* la fila "...y mas" no hace nada */
    if (metro_music_play_songs_of_genre(s_genre_songs_seek, index))
        metro_screen_nowplaying_push();
}

static const struct metro_pivot genre_songs_pivots[] = {
    { LANG_PIVOT_SONGS, genre_songs_count, genre_songs_get_row, genre_songs_on_select, NULL },
};
static struct metro_page genre_songs_page = { LANG_PIVOT_SONGS, genre_songs_pivots, 1, NULL };

static void open_genre_songs(int32_t genre_seek, const char *genre_label)
{
    s_genre_songs_seek = genre_seek;
    s_genre_songs_n = metro_music_songs_of_genre(genre_seek, s_genre_songs, METRO_MUSIC_MAX_SONGS);
    strlcpy(s_genre_songs_title, genre_label, sizeof(s_genre_songs_title));
    genre_songs_page.title_dynamic = s_genre_songs_title;
    metro_screen_list_push(&genre_songs_page);
}

/* "updating library..." placeholder -- pushed instead of music_page
 * while metro_music_db_ready() is still false. */

static int updating_count(void *ctx) { (void)ctx; return 1; }

static void updating_get_row(void *ctx, int index, struct metro_row *out)
{
    (void)ctx;
    (void)index;
    out->title = metro_lang_str(LANG_MUSIC_DB_UPDATING);
    out->subtitle = NULL;
    out->kind = METRO_ROW_ACTION;
}

static void updating_on_select(void *ctx, int index) { (void)ctx; (void)index; }

static const struct metro_pivot updating_pivots[] = {
    { LANG_HUB_MUSIC, updating_count, updating_get_row, updating_on_select, NULL },
};
static const struct metro_page updating_page = { LANG_HUB_MUSIC, updating_pivots, 1, NULL };


/* --- the hub's own rows: [now playing] | music | videos | photos | settings */

static int hub_count(void *ctx)
{
    (void)ctx;
    return metro_music_is_playing() ? 5 : 4;
}

static void hub_get_row(void *ctx, int index, struct metro_row *out)
{
    static const enum metro_lang_id titles[4] = {
        LANG_HUB_MUSIC, LANG_HUB_VIDEOS, LANG_HUB_PHOTOS, LANG_HUB_SETTINGS
    };
    bool playing = metro_music_is_playing();

    (void)ctx;

    if (playing && index == 0)
    {
        static char buf[METRO_MUSIC_ITEM_LEN];
        char sub[METRO_MUSIC_ITEM_LEN];

        if (!metro_music_now_playing(buf, sizeof(buf), sub, sizeof(sub)))
            strlcpy(buf, metro_lang_str(LANG_HUB_NOWPLAYING), sizeof(buf));
        out->title = buf;
        out->subtitle = NULL;
        out->kind = METRO_ROW_NAV;
        return;
    }

    out->title = metro_lang_str(titles[playing ? index - 1 : index]);
    out->subtitle = NULL;
    out->kind = METRO_ROW_NAV;
}

static void hub_on_select(void *ctx, int index)
{
    bool playing = metro_music_is_playing();
    int base;

    (void)ctx;

    if (playing && index == 0)
    {
        metro_screen_nowplaying_push();
        return;
    }

    base = playing ? index - 1 : index;
    switch (base)
    {
        case 0:
            if (!metro_music_db_ready())
                metro_screen_list_push(&updating_page);
            else
            {
                music_lists_refresh();
                metro_screen_list_push(&music_page);
            }
            break;
        case 1: build_videos_page(); metro_screen_list_push(&videos_page); break;
        case 2: build_photos_page(); metro_screen_list_push(&photos_page); break;
        case 3: metro_screen_list_push(metro_screen_settings_page()); break;
        default: break;
    }
}

/* R5-F5 (M-085): la fila "reproduciendo" del hub se mueve. Sonando, el
 * título corre en bucle de derecha a izquierda bajo el margen izquierdo
 * (x=12, que no se invade: el texto desaparece por debajo de él y
 * reaparece por la derecha, donde sí puede cortarse); en pausa, se queda
 * quieto y "respira": un fundido hacia el fondo y de vuelta, con la fase
 * visible bastante más larga que la invisible. Todo relativo a
 * current_tick, sin estado acumulado: si el tick se salta cuadros (disco
 * ocupado, LCD dormido), la animación se reanuda donde le toca y no
 * donde se quedó. Puertas: lcd_active() y animations != off. */
#define METRO_HUB_MARQUEE_GAP      60        /* px entre repeticiones del título */
#define METRO_HUB_MARQUEE_TICKS_PX 4         /* 1 px cada 4 ticks = 25 px/s */
#define METRO_HUB_BREATH_PERIOD    (HZ * 3)  /* ciclo completo */
#define METRO_HUB_BREATH_ON        (HZ * 17 / 10) /* 1.7 s a la vista */
#define METRO_HUB_BREATH_FADE      (HZ * 4 / 10)  /* 0.4 s de ida, 0.4 s de vuelta */
#define METRO_HUB_TEXT_X           12

static bool hub_row_animates(void)
{
    return metro_settings.animations != METRO_ANIM_OFF && lcd_active();
}

/* 0 = color pleno, 256 = fondo. Triángulo con mesetas: ON visible,
 * FADE bajando, el resto del periodo menos FADE invisible, FADE subiendo. */
static int breath_alpha(long t)
{
    t %= METRO_HUB_BREATH_PERIOD;
    if (t < METRO_HUB_BREATH_ON)
        return 0;
    t -= METRO_HUB_BREATH_ON;
    if (t < METRO_HUB_BREATH_FADE)
        return (int)(t * 256 / METRO_HUB_BREATH_FADE);
    t -= METRO_HUB_BREATH_FADE;
    if (t < METRO_HUB_BREATH_PERIOD - METRO_HUB_BREATH_ON - 2 * METRO_HUB_BREATH_FADE)
        return 256;
    t -= METRO_HUB_BREATH_PERIOD - METRO_HUB_BREATH_ON - 2 * METRO_HUB_BREATH_FADE;
    return 256 - (int)(t * 256 / METRO_HUB_BREATH_FADE);
}

static void draw_now_playing_row(int y, bool selected)
{
    struct metro_row row;
    unsigned color = selected ? metro_color_fg() : metro_color_secondary();
    int clip_w = LCD_WIDTH - METRO_HUB_TEXT_X;

    hub_get_row(NULL, 0, &row);

    if (!hub_row_animates())
    {
        metro_draw_text(MFONT_DISPLAY, METRO_HUB_TEXT_X, y, row.title, color);
        return;
    }

    if (audio_status() & AUDIO_STATUS_PAUSE)
    {
        color = metro_fb_blend_color(color, metro_color_bg(),
                                     breath_alpha(current_tick));
        metro_draw_text(MFONT_DISPLAY, METRO_HUB_TEXT_X, y, row.title, color);
    }
    else
    {
        int w, h, span, offset;

        /* M-117: el ciclo de esta marquesina ES el ancho medido. Con
         * la primaria, un título cirílico medía de menos: `span` corto,
         * y las dos copias del bucle se encimaban en pantalla. */
        metro_draw_text_size(MFONT_DISPLAY, row.title, &w, &h);
        span = w + METRO_HUB_MARQUEE_GAP;
        offset = (int)((current_tick / METRO_HUB_MARQUEE_TICKS_PX) % span);

        /* Dos copias: la que sale por la izquierda y la que entra por
         * la derecha, a `span` de distancia -- el bucle no tiene
         * costura. La segunda solo hace falta cuando ya asoma. */
        metro_draw_text_clipped(MFONT_DISPLAY, METRO_HUB_TEXT_X, clip_w,
                                 METRO_HUB_TEXT_X - offset, y, row.title, color);
        if (METRO_HUB_TEXT_X - offset + span < LCD_WIDTH)
            metro_draw_text_clipped(MFONT_DISPLAY, METRO_HUB_TEXT_X, clip_w,
                                     METRO_HUB_TEXT_X - offset + span, y,
                                     row.title, color);
    }
}

void metro_screen_hub_show(void)
{
    metro_nav_t *nav = metro_screen_nav();
    int first = metro_nav_first_visible(nav);
    int sel = metro_nav_sel(nav);
    int count = hub_count(NULL);
    bool playing = metro_music_is_playing();
    int i, y = METRO_HUB_FIRST_Y;

    metro_draw_clear();
    metro_draw_header("");

    for (i = first; i < count && i < first + METRO_HUB_VISIBLE + 1; i++)
    {
        struct metro_row row;
        bool selected = (i == sel);

        if (playing && i == 0)
        {
            draw_now_playing_row(y, selected);
        }
        else
        {
            hub_get_row(NULL, i, &row);
            metro_draw_text(MFONT_DISPLAY, METRO_HUB_TEXT_X, y, row.title,
                             selected ? metro_color_fg() : metro_color_secondary());
        }
        y += METRO_HUB_PITCH;
    }

    lcd_update();
}

bool metro_screen_hub_wants_ticks(void)
{
    return metro_music_is_playing() && hub_row_animates() &&
           metro_nav_first_visible(metro_screen_nav()) == 0; /* fila a la vista */
}

bool metro_screen_hub_tick(void)
{
    metro_nav_t *nav = metro_screen_nav();

    if (!metro_screen_hub_wants_ticks())
        return false;

    /* Solo esa fila: limpiar su franja y volverla a pintar. Más barato
     * que un show() completo a 20 Hz (que redibuja cuatro textos de
     * 48 px y sube 150 KB al LCD por cuadro). */
    lcd_set_foreground(metro_color_bg());
    lcd_fillrect(0, METRO_HUB_FIRST_Y, LCD_WIDTH, METRO_HUB_PITCH);
    draw_now_playing_row(METRO_HUB_FIRST_Y, metro_nav_sel(nav) == 0);
    lcd_update_rect(0, METRO_HUB_FIRST_Y, LCD_WIDTH, METRO_HUB_PITCH);
    return true;
}

void metro_screen_hub_handle(int action, int steps)
{
    metro_nav_t *nav = metro_screen_nav();
    int count = hub_count(NULL);

    switch (action)
    {
        case MACT_PREV:
            metro_nav_move_sel(nav, -steps, count, METRO_HUB_VISIBLE);
            break;
        case MACT_NEXT:
            metro_nav_move_sel(nav, steps, count, METRO_HUB_VISIBLE);
            break;
        case MACT_SELECT:
            hub_on_select(NULL, metro_nav_sel(nav));
            break;
        case MACT_PLAYPAUSE:
            metro_music_playpause();
            break;
        default:
            break;
    }
}

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

#include "config.h" /* HAVE_ALBUMART/HAVE_JPEG, same ordering gotcha as
                        metro_music.c -- see DECISIONS.md M-030. */
#include "audio.h"
#include "albumart.h"
#include "jpeg_load.h"
#include "bmp.h"
#include "string-extra.h"
#include "metadata.h" /* get_metadata() -- R3-F4/DD-5 */
#include "debug.h"    /* DEBUGF -- M-097: every JPEG decode is logged in the sim */

#include "metro_albumart.h"
#include "metro_draw.h" /* METRO_TILE_SIZE */
#include "metro_master_art.h" /* M-097 */
#include "metro_music.h"      /* metro_music_album_key_for_track() -- M-097 */

/* NOT just width*height*sizeof(fb_data) -- FORMAT_RESIZE needs real
 * working room beyond the final bitmap (JPEG_DECODE_OVERHEAD,
 * recorder/jpeg_load.h, is ~39KB on its own) for the intermediate
 * decode before it downscales to METRO_ALBUMART_SIZE. Same formula and
 * same margin Aura-Firmware's aura_albumart.c settled on after finding
 * this the hard way: undersizing this doesn't fail loudly, it either
 * degrades silently to the no-art tile, or -- verified here, the
 * actual failure hit while building this file with a 1x buffer --
 * read_jpeg_file()/clip_jpeg_file() write past the end of an
 * undersized buffer before their own bounds check gives up, corrupting
 * whatever static data the linker placed next. M-097: sized for the
 * old 136px target still (a hair more room than 130 needs). */
#define METRO_ALBUMART_DECODE_PX 136
#define METRO_ALBUMART_SCRATCH_SIZE \
    (METRO_ALBUMART_DECODE_PX * METRO_ALBUMART_DECODE_PX * 2 * 2)

static unsigned char s_scratch[METRO_ALBUMART_SCRATCH_SIZE];

/* M-097: the Now Playing cover, METRO_ALBUMART_SIZE square -- the
 * master's own pixels (read from /.aura/art, or just cropped from a
 * fresh decode and written there). Separate from s_scratch, which is
 * only the decoder's working room now. "Cache of 1" keyed by track
 * path, as before. */
static fb_data s_tile[METRO_ALBUMART_SIZE * METRO_ALBUMART_SIZE];
static char s_loaded_path[MAX_PATH];
static bool s_loaded = false;

/* F12: same shape as s_scratch above, just full-screen instead of the
 * small NP tile -- the dimmed background (metro_screen_nowplaying.c,
 * graphics=full only). Same oversizing rule as METRO_ALBUMART_SCRATCH_SIZE
 * (M-033): FORMAT_RESIZE needs room for the full native decode before
 * it downscales, not just the final LCD_WIDTH*LCD_HEIGHT bitmap. */
#define METRO_ALBUMART_BG_SCRATCH_SIZE \
    (LCD_WIDTH * LCD_HEIGHT * 2 * 2)

static unsigned char s_bg_scratch[METRO_ALBUMART_BG_SCRATCH_SIZE];
static char s_bg_loaded_path[MAX_PATH];
static bool s_bg_loaded = false;

#define ALBUMS_SUBDIR  "albums"
#define ARTISTS_SUBDIR "artists"

/* Shared by both sizes (the small NP tile and F12's full-screen
 * background) -- only the destination width/height/buffer and the
 * FORMAT_* flags differ between them. Returns the decoded size in
 * *bm_out (KEEP_ASPECT lands below the requested box). */
static bool decode_file_into(const char *art_path, unsigned char *scratch,
                              size_t scratch_size, int width, int height, int format,
                              struct bitmap *bm_out)
{
    struct bitmap bm;
    size_t len = strlen(art_path);
    int ret;

    bm.width = width;
    bm.height = height;
    bm.data = scratch;
#if (LCD_DEPTH > 1)
    bm.maskdata = NULL;
#endif

    DEBUGF("metro_art: %s decode (%dx%d) %s\n",
           (len > 4 && !strcasecmp(art_path + len - 4, ".bmp")) ? "bmp" : "jpeg",
           width, height, art_path);
    if (len > 4 && !strcasecmp(art_path + len - 4, ".bmp"))
        ret = read_bmp_file(art_path, &bm, scratch_size, format, NULL);
    else
        ret = read_jpeg_file(art_path, &bm, scratch_size, format, NULL);
    if (ret <= 0 || bm.width <= 0 || bm.height <= 0)
        return false;
    if (bm_out)
        *bm_out = bm;
    return true;
}

static bool decode_embedded_into(struct mp3entry *id3, unsigned char *scratch,
                                  size_t scratch_size, int width, int height, int format,
                                  struct bitmap *bm_out)
{
    struct bitmap bm;
    int ret;

    if (!id3->has_embedded_albumart ||
        (id3->albumart.type & AA_CLEAR_FLAGS_MASK) != AA_TYPE_JPG)
        return false;

    bm.width = width;
    bm.height = height;
    bm.data = scratch;
#if (LCD_DEPTH > 1)
    bm.maskdata = NULL;
#endif

    DEBUGF("metro_art: embedded jpeg decode (%dx%d) %s\n", width, height, id3->path);
    ret = clip_jpeg_file(id3->path, id3->albumart.pos, id3->albumart.size,
                          &bm, scratch_size, format, NULL);
    if (ret <= 0 || bm.width <= 0 || bm.height <= 0)
        return false;
    if (bm_out)
        *bm_out = bm;
    return true;
}

/* M-097: decodes `id3`'s art (folder art first, embedded otherwise) at
 * the master size, KEEP_ASPECT, then fill-and-center-crops it into the
 * square `out`. The master geometry of contract v16. Caller holds the
 * lock (s_scratch is the decoder's room). */
static bool decode_master_from_id3(struct mp3entry *id3, fb_data *out)
{
    char art_path[MAX_PATH];
    struct dim dim = { METRO_ALBUMART_SIZE, METRO_ALBUMART_SIZE };
    struct bitmap bm;
    const int fmt = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
    bool ok;

    if (find_albumart(id3, art_path, sizeof(art_path), &dim))
        ok = decode_file_into(art_path, s_scratch, sizeof(s_scratch),
                              METRO_ALBUMART_SIZE, METRO_ALBUMART_SIZE, fmt, &bm);
    else
        ok = decode_embedded_into(id3, s_scratch, sizeof(s_scratch),
                                  METRO_ALBUMART_SIZE, METRO_ALBUMART_SIZE, fmt, &bm);
    if (!ok)
        return false;

    metro_master_art_cover((const fb_data *)s_scratch, bm.width, bm.height,
                           out, METRO_ALBUMART_SIZE, METRO_ALBUMART_SIZE);
    return true;
}

/* M-097: master-first resolution of `id3`'s cover into `out`. With a
 * resolvable album key: read the master, honour .none, or decode +
 * WRITE the master/.none. Without one (track not in the database):
 * plain decode, nothing written. Caller holds the lock. */
static bool resolve_master_from_id3(struct mp3entry *id3, fb_data *out)
{
    char key[METRO_MASTER_ART_KEY_LEN];

    if (!metro_music_album_key_for_track(id3->path, key, sizeof(key)))
        return decode_master_from_id3(id3, out);

    switch (metro_master_art_probe(ALBUMS_SUBDIR, key))
    {
    case METRO_MASTER_ART_PRESENT:
        if (metro_master_art_read(ALBUMS_SUBDIR, key, out, METRO_ALBUMART_SIZE))
            return true;
        /* FALLTHROUGH: unreadable -> rebuild */
    case METRO_MASTER_ART_MISSING:
        if (decode_master_from_id3(id3, out))
        {
            metro_master_art_write(ALBUMS_SUBDIR, key, out, METRO_ALBUMART_SIZE);
            return true;
        }
        metro_master_art_write_none(ALBUMS_SUBDIR, key);
        return false;
    case METRO_MASTER_ART_NONE:
    default:
        return false;
    }
}

bool metro_albumart_load_current(void)
{
    struct mp3entry *id3;
    bool ok;

    if (!(audio_status() & AUDIO_STATUS_PLAY))
        return false;

    id3 = audio_current_track();
    if (!id3)
        return false;

    /* "cache of 1" (module doc comment): reloads only when the track's
     * path changed since the last call. */
    if (s_loaded && !strcmp(s_loaded_path, id3->path))
        return true;

    metro_master_art_lock();
    ok = resolve_master_from_id3(id3, s_tile);
    metro_master_art_unlock();

    if (!ok)
    {
        s_loaded = false;
        s_loaded_path[0] = '\0';
        return false;
    }
    strlcpy(s_loaded_path, id3->path, sizeof(s_loaded_path));
    s_loaded = true;
    return true;
}

const fb_data *metro_albumart_bitmap(void)
{
    return s_tile;
}

/* M-097: screen-sized background from a 130px master -- bilinear,
 * fill-and-center-crop (no letterbox, cropped corners are the point of
 * a background, PLAN_MAESTRO.md S3.3). */
static void upscale_master_to_bg(const fb_data *master, int px)
{
    metro_master_art_cover_bilinear(master, px, px, (fb_data *)s_bg_scratch,
                                    LCD_WIDTH, LCD_HEIGHT);
}

bool metro_albumart_load_background(void)
{
    struct mp3entry *id3;
    bool ok;

    if (!(audio_status() & AUDIO_STATUS_PLAY))
        return false;
    id3 = audio_current_track();
    if (!id3)
        return false;
    if (s_bg_loaded && !strcmp(s_bg_loaded_path, id3->path))
        return true;

    /* The cover tile first (master read, or one decode that also
     * leaves the master on disk) -- then the background is derived
     * from it, no second decode. */
    ok = metro_albumart_load_current();
    if (ok)
    {
        upscale_master_to_bg(s_tile, METRO_ALBUMART_SIZE);
    }
    else
    {
        /* No key (track outside the database) -> load_current already
         * tried a plain decode and failed; nothing else to do. */
        s_bg_loaded = false;
        s_bg_loaded_path[0] = '\0';
        return false;
    }

    strlcpy(s_bg_loaded_path, id3->path, sizeof(s_bg_loaded_path));
    s_bg_loaded = true;
    return true;
}

bool metro_albumart_load_background_file(const char *path, long mtime)
{
    bool ok = false;

    if (!path || !path[0])
        return false;

    /* Misma caché-de-1 que load_background(), clavada a la RUTA REAL
     * de origen y no al track: así una foto de artista y una carátula
     * nunca se confunden entre sí, y volver a la misma pista con la
     * misma fuente no vuelve a decodificar. */
    if (s_bg_loaded && !strcmp(s_bg_loaded_path, path))
        return true;

    metro_master_art_lock();
    if (mtime > 0)
    {
        /* M-097: la maestra de artista (130 px, contrato v16) --
         * leída si existe, decodificada y ESCRITA si no; el fondo se
         * agranda desde ella (bilineal). Una foto de artista viene a
         * lo mucho de 128px (tope del contrato), así que agrandar
         * desde 130 no pierde nada frente a agrandar desde el JPEG. */
        char key[METRO_MASTER_ART_KEY_LEN];
        fb_data *master = metro_master_art_scratch();
        const int px = METRO_MASTER_ART_ARTIST_PX;

        metro_music_artist_image_master_key(strrchr(path, '/') ? strrchr(path, '/') + 1 : path,
                                            mtime, key, sizeof(key));
        switch (metro_master_art_probe(ARTISTS_SUBDIR, key))
        {
        case METRO_MASTER_ART_PRESENT:
            ok = metro_master_art_read(ARTISTS_SUBDIR, key, master, px);
            if (ok)
                break;
            /* FALLTHROUGH */
        case METRO_MASTER_ART_MISSING:
        {
            struct bitmap bm;
            ok = decode_file_into(path, s_scratch, sizeof(s_scratch), px, px,
                                  FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT, &bm);
            if (ok)
            {
                metro_master_art_cover((const fb_data *)s_scratch, bm.width, bm.height,
                                       master, px, px);
                metro_master_art_write(ARTISTS_SUBDIR, key, master, px);
            }
            else
                metro_master_art_write_none(ARTISTS_SUBDIR, key);
            break;
        }
        case METRO_MASTER_ART_NONE:
        default:
            ok = false;
            break;
        }
        if (ok)
            upscale_master_to_bg(master, px);
    }
    else
    {
        /* Sin FORMAT_KEEP_ASPECT, igual que load_background(): llena los
         * 320x240 y recorta, que es el punto de un fondo. */
        ok = decode_file_into(path, s_bg_scratch, sizeof(s_bg_scratch),
                              LCD_WIDTH, LCD_HEIGHT, FORMAT_NATIVE | FORMAT_RESIZE, NULL);
    }
    metro_master_art_unlock();

    if (!ok)
    {
        s_bg_loaded = false;
        s_bg_loaded_path[0] = '\0';
        return false;
    }

    strlcpy(s_bg_loaded_path, path, sizeof(s_bg_loaded_path));
    s_bg_loaded = true;
    return true;
}

const fb_data *metro_albumart_background_bitmap(void)
{
    return (const fb_data *)s_bg_scratch;
}

/* Own static mp3entry -- struct mp3entry is ~1.5-2KB (ID3V2_BUF_SIZE
 * alone is up to 1800B, lib/rbcodec/metadata/metadata.h), too big for
 * a comfortable stack frame on Rockbox's small per-thread stacks (same
 * "D-226 stack concern" class of buffer metro_music.c already avoids
 * putting on the stack). Deliberately NOT get_temp_mp3entry()
 * (playback.h): that one is playback-engine scratch memory with its
 * own locking, for a different purpose (peeking at the next track) --
 * using it here would mean contending with the audio thread for
 * something that has nothing to do with it. get_metadata() itself is
 * a standalone utility, not tied to that machinery -- tagcache.c calls
 * it the same way, for the same reason (reading tags from an arbitrary
 * file, independent of what's playing). */
static struct mp3entry s_track_id3;

bool metro_albumart_decode_track_master(const char *track_path, fb_data *out)
{
    /* Shares s_scratch (the decoder's room) with the Now Playing
     * paths above -- serialized by the master-art lock the caller
     * holds; s_tile (the NP cache of 1) is untouched, so no
     * invalidation is needed anymore (R3-F4's s_loaded = false). */
    if (!get_metadata(&s_track_id3, -1, track_path))
        return false;

    return decode_master_from_id3(&s_track_id3, out);
}

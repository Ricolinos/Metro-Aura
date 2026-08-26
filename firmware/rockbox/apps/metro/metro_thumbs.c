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

#include "file.h"
#include "dir.h"
#include "jpeg_load.h"
#include "bmp.h"
#include "string-extra.h"
#include "crc32.h"
#include <stdlib.h> /* qsort() -- metro_thumbs_gc(), M-096 */

#include "metro_thumbs.h"
#include "metro_settings.h"
#include "metro_draw.h"
#include "metro_fsutil.h"

#define THUMB_PX (METRO_TILE_SIZE * METRO_TILE_SIZE)

/* R2-F2/DD-9: 16 thumbnails (~2 grid screens) resident in RAM at once
 * -- 16 * 80*80*sizeof(fb_data) = 16 * 12800 = 204800 bytes. Plain ring
 * eviction (not true LRU): simple, and with a window this much bigger
 * than one screen (8 tiles) it takes real back-and-forth scrolling to
 * evict something still on screen. R3-F1/DD-1: this window is now
 * SHARED across every source (photos/artists/albums) -- only one grid
 * is ever on screen, so there's no reason to pay for three windows. */
#define WINDOW_N 16
#define PENDING_MAX 16

/* Cache-key stems are filename-shaped ("<name>.<mtime>") -- reuse the
 * same length budget metro_fsutil.c already uses for source names. */
#define KEY_LEN METRO_FSUTIL_NAME_LEN

struct thumb_slot {
    char key[KEY_LEN];
    bool valid;
    fb_data pixels[THUMB_PX];
};

static struct thumb_slot s_window[WINDOW_N];
static int s_window_ring = 0;

/* R3-F1/DD-1: a pending entry remembers WHICH source queued it (not
 * just a filename+mtime, R2-F2's original shape) -- metro_thumbs_tick()
 * needs (source, ctx, index) to call back into that source's own
 * decode(). The cache key is captured once, at metro_thumbs_get() time,
 * and carried along so tick() never has to recompute it (and so a
 * dedup check against the pending queue is a plain string compare). */
struct pending_entry {
    const struct metro_thumb_source *source;
    void *ctx;
    int index;
    char key[KEY_LEN];
};

static struct pending_entry s_pending[PENDING_MAX];
static int s_pending_n = 0;

/* R3-F3 correction (M-064, docs/DESVIACIONES.md R3-3): R2-F2 sized this
 * for METRO_TILE_SIZE (80px) x2 margin (M-033) and never hit trouble
 * because every /Photos/ fixture was comfortably larger than 80px.
 * Rockbox's JPEG decoder only offers power-of-two DCT scale steps
 * (1/1, 1/2, 1/4, 1/8); when the smallest step that's still >= the
 * requested size lands close to the SOURCE's own native resolution,
 * read_jpeg_file() decodes at that native resolution first and scales
 * down in software afterward -- the exact JPEG_DECODE_OVERHEAD
 * mechanism the photo VIEWER already had to account for explicitly
 * (R2-F3, metro_screen_photo_viewer.c). Artist photos are capped at
 * <=128px BY CONTRACT (CONTRATO-firmware-studio.md SS D.3) -- squarely
 * in that gap (128/2=64 < 80, so the decoder falls back to the full
 * 128x128 native size) -- confirmed live: every artist thumbnail
 * failed to decode (metro_thumbs_decode_jpeg_cover() returning false)
 * until this was widened to cover a full 128x128 decode, not just an
 * 80x80 one. Budgeted against the contract's own upper bound (128px),
 * same x2 margin rule, rather than adding a dimension probe like the
 * viewer did -- simpler, and this helper is shared with photos (whose
 * sources are typically far larger than 128px, so the wider budget
 * costs nothing there, it just also covers the rare small photo that
 * would have hit the exact same gap). */
#define SCRATCH_MAX_SRC_PX 128
#define SCRATCH_SIZE (SCRATCH_MAX_SRC_PX * SCRATCH_MAX_SRC_PX * 2 * 2)
static unsigned char s_scratch[SCRATCH_SIZE];

static struct thumb_slot *find_slot(const char *key)
{
    int i;
    for (i = 0; i < WINDOW_N; i++)
        if (s_window[i].valid && !strcmp(s_window[i].key, key))
            return &s_window[i];
    return NULL;
}

static void cache_dir_for(const struct metro_thumb_source *source, char *out, size_t outsz)
{
    metro_settings_metro_cache_dir(source->cache_subdir, out, outsz);
}

static void cache_path(const struct metro_thumb_source *source, const char *key,
                        char *out, size_t outsz)
{
    cache_dir_for(source, out, outsz);
    strlcat(out, "/", outsz);
    strlcat(out, key, outsz);
    strlcat(out, ".mth", outsz);
}

static void ensure_cache_dir(const struct metro_thumb_source *source)
{
    char dir[MAX_PATH], parent[MAX_PATH];
    char *slash;

    cache_dir_for(source, dir, sizeof(dir));
    strlcpy(parent, dir, sizeof(parent));
    slash = strrchr(parent, '/');
    if (slash)
        *slash = '\0';

    /* M-096: /.aura/thumbs/<subdir> -- on a fresh disk none of the
     * three levels exist yet (/.aura is only created by the first sync
     * marker or stamp write), so the grandparent is covered too. */
    if (!dir_exists(parent))
    {
        char grand[MAX_PATH];
        strlcpy(grand, parent, sizeof(grand));
        slash = strrchr(grand, '/');
        if (slash && slash != grand)
        {
            *slash = '\0';
            if (!dir_exists(grand))
                mkdir(grand);
        }
        mkdir(parent);
    }
    if (!dir_exists(dir))
        mkdir(dir);
}

/* Length of the key's "stable name" half: up to the last '.', or --
 * for synthetic keys with no '.' (album "a-<crc>-<mtime>", M-096) --
 * the last '-'. */
static size_t key_stem_len(const char *key)
{
    const char *sep = strrchr(key, '.');
    if (!sep)
        sep = strrchr(key, '-');
    return sep ? (size_t)(sep - key) : strlen(key);
}

/* R2-F2/DD-9, generalized R3-F1: drops any other cached .mth that
 * shares this key's stem (the part before the *last* '.', i.e. the
 * "<stable-name>" half of "<stable-name>.<mtime>") -- the mtime-in-
 * the-key scheme means a changed source item picks a new cache
 * filename, leaving the old one an orphan pointing at content nobody
 * will ever ask for by that exact key again. Cheap: one directory
 * scan, only run on an actual decode (already the slow path). Generic
 * over all three sources because all three follow the same
 * "<name>.<mtime>" key convention (metro_thumb_source.cache_key's own
 * contract). */
static void remove_stale(const struct metro_thumb_source *source, const char *key,
                          const char *keep_path)
{
    char dir[MAX_PATH];
    char prefix[KEY_LEN];
    size_t prefix_len;
    DIR *d;
    struct DIRENT *entry;

    strlcpy(prefix, key, sizeof(prefix));
    prefix_len = key_stem_len(prefix);
    prefix[prefix_len] = '\0';

    cache_dir_for(source, dir, sizeof(dir));
    d = opendir(dir);
    if (!d)
        return;

    while ((entry = readdir(d)) != NULL)
    {
        char full[MAX_PATH];

        if (strncmp(entry->d_name, prefix, prefix_len) != 0 ||
            (entry->d_name[prefix_len] != '.' && entry->d_name[prefix_len] != '-'))
            continue; /* not "<prefix>.<...>.mth" / "<prefix>-<...>.mth" for THIS stem */

        strlcpy(full, dir, sizeof(full));
        strlcat(full, "/", sizeof(full));
        strlcat(full, entry->d_name, sizeof(full));
        if (strcmp(full, keep_path) != 0)
            remove(full);
    }
    closedir(d);
}

/* Nearest-neighbour "cover" crop from a single FORMAT_KEEP_ASPECT
 * decode -- no second (unscaled) decode and no JPEG-dimension probe
 * needed. `src`/`sw`/`sh` is the KEEP_ASPECT result (one dimension
 * already == METRO_TILE_SIZE, the other <= it); this conceptually
 * upscales that result until BOTH dimensions reach METRO_TILE_SIZE,
 * then samples the centered METRO_TILE_SIZE x METRO_TILE_SIZE crop
 * straight out of `src` (never materializes the upscaled bitmap).
 * Trades a little sharpness on the cropped axis for staying a single
 * cheap decode -- acceptable at 80x80. The photo VIEWER's own "cubrir"
 * (full 320x240) needs real precision instead, hence that one reads
 * Aura's Q16.16 algorithm as reference; a thumbnail this small doesn't
 * call for the same machinery. */
static void cover_crop(const fb_data *src, int sw, int sh, fb_data *out)
{
    int scale_den = (sw < sh) ? sw : sh; /* the dimension short of METRO_TILE_SIZE */
    int upscaled_w = sw * METRO_TILE_SIZE / scale_den;
    int upscaled_h = sh * METRO_TILE_SIZE / scale_den;
    int crop_x = (upscaled_w - METRO_TILE_SIZE) / 2;
    int crop_y = (upscaled_h - METRO_TILE_SIZE) / 2;
    int ox, oy;

    for (oy = 0; oy < METRO_TILE_SIZE; oy++)
    {
        int up_y = oy + crop_y;
        int sy = up_y * scale_den / METRO_TILE_SIZE;

        if (sy < 0) sy = 0;
        if (sy >= sh) sy = sh - 1;

        for (ox = 0; ox < METRO_TILE_SIZE; ox++)
        {
            int up_x = ox + crop_x;
            int sx = up_x * scale_den / METRO_TILE_SIZE;

            if (sx < 0) sx = 0;
            if (sx >= sw) sx = sw - 1;

            out[oy * METRO_TILE_SIZE + ox] = src[sy * sw + sx];
        }
    }
}

bool metro_thumbs_decode_jpeg_cover(const char *path, fb_data *out)
{
    struct bitmap bm;
    int ret;

    bm.width = METRO_TILE_SIZE;
    bm.height = METRO_TILE_SIZE;
    bm.data = (char *)s_scratch;
#if (LCD_DEPTH > 1)
    bm.maskdata = NULL;
#endif
    ret = read_jpeg_file(path, &bm, sizeof(s_scratch),
                          FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT, NULL);
    if (ret <= 0)
        return false;

    cover_crop((const fb_data *)s_scratch, bm.width, bm.height, out);
    return true;
}

const fb_data *metro_thumbs_get(const struct metro_thumb_source *source,
                                 void *ctx, int index)
{
    struct thumb_slot *s;
    char key[KEY_LEN];
    char path[MAX_PATH];
    int fd;
    ssize_t got;
    int i;

    if (!source->cache_key(ctx, index, key, sizeof(key)))
        return NULL;

    s = find_slot(key);
    if (s)
        return s->pixels;

    /* Disk cache is a raw read (no decode) -- cheap enough to try
     * synchronously, unlike an actual decode. */
    cache_path(source, key, path, sizeof(path));
    fd = open(path, O_RDONLY);
    if (fd >= 0)
    {
        got = read(fd, s_window[s_window_ring].pixels, THUMB_PX * sizeof(fb_data));
        close(fd);
        if (got == (ssize_t)(THUMB_PX * sizeof(fb_data)))
        {
            s = &s_window[s_window_ring];
            strlcpy(s->key, key, sizeof(s->key));
            s->valid = true;
            s_window_ring = (s_window_ring + 1) % WINDOW_N;
            return s->pixels;
        }
    }

    for (i = 0; i < s_pending_n; i++)
        if (!strcmp(s_pending[i].key, key))
            return NULL; /* already queued */

    if (s_pending_n < PENDING_MAX)
    {
        s_pending[s_pending_n].source = source;
        s_pending[s_pending_n].ctx = ctx;
        s_pending[s_pending_n].index = index;
        strlcpy(s_pending[s_pending_n].key, key, sizeof(s_pending[0].key));
        s_pending_n++;
    }
    return NULL;
}

bool metro_thumbs_tick(void)
{
    struct pending_entry entry;
    char path[MAX_PATH];
    struct thumb_slot *s;
    int fd;
    int i;

    if (s_pending_n == 0)
        return false;

    entry = s_pending[0];
    for (i = 1; i < s_pending_n; i++)
        s_pending[i - 1] = s_pending[i];
    s_pending_n--;

    s = &s_window[s_window_ring];
    if (!entry.source->decode(entry.ctx, entry.index, s->pixels))
        return true; /* budget spent either way -- don't retry this tick */

    strlcpy(s->key, entry.key, sizeof(s->key));
    s->valid = true;
    s_window_ring = (s_window_ring + 1) % WINDOW_N;

    ensure_cache_dir(entry.source);
    cache_path(entry.source, entry.key, path, sizeof(path));
    remove_stale(entry.source, entry.key, path);
    fd = creat(path, 0666);
    if (fd >= 0)
    {
        write(fd, s->pixels, THUMB_PX * sizeof(fb_data));
        close(fd);
    }

    return true;
}

void metro_thumbs_reset(void)
{
    int i;
    for (i = 0; i < WINDOW_N; i++)
        s_window[i].valid = false;
    s_pending_n = 0;
}

/* --- M-096: orphan sweep ------------------------------------------- */

/* The flag lives on disk (an empty file next to the caches) rather
 * than in RAM: a sync that finishes at boot and a shutdown before the
 * user ever opens Music would otherwise lose it, and the orphans would
 * stay forever. Path via metro_settings_metro_cache_dir() -- CLAUDE.md's
 * compat-path rule. */
static void dirty_flag_path(char *out, size_t outsz)
{
    metro_settings_metro_cache_dir("albums.dirty", out, outsz);
}

void metro_thumbs_mark_dirty(void)
{
    char path[MAX_PATH];
    int fd;

    dirty_flag_path(path, sizeof(path));
    ensure_cache_dir(&(const struct metro_thumb_source){ "albums", NULL, NULL });
    fd = creat(path, 0666);
    if (fd >= 0)
        close(fd);
}

bool metro_thumbs_take_dirty(void)
{
    char path[MAX_PATH];

    dirty_flag_path(path, sizeof(path));
    if (!file_exists(path))
        return false;
    remove(path);
    return true;
}

/* Keys are remembered as crc32 of the full stem, 4 bytes each, so a
 * 2,000-album library costs 8KB of static scratch instead of 2,000
 * KEY_LEN strings. A crc collision only ever KEEPS a file (never
 * deletes a live one), which is the safe direction. */
#define GC_MAX_KEYS 2048
static uint32_t s_gc_keys[GC_MAX_KEYS];

static int cmp_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : x > y;
}

static bool gc_key_live(uint32_t h, int n)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        if (s_gc_keys[mid] == h) return true;
        if (s_gc_keys[mid] < h) lo = mid + 1; else hi = mid - 1;
    }
    return false;
}

int metro_thumbs_gc(const struct metro_thumb_source *source, void *ctx, int count)
{
    char key[KEY_LEN], dir[MAX_PATH], full[MAX_PATH];
    DIR *d;
    struct DIRENT *entry;
    int i, n = 0, removed = 0;

    for (i = 0; i < count && n < GC_MAX_KEYS; i++)
        if (source->cache_key(ctx, i, key, sizeof(key)))
            s_gc_keys[n++] = crc_32(key, strlen(key), 0xffffffff);
    if (count > GC_MAX_KEYS)
        return 0; /* can't tell live from orphan beyond the cap: keep all */
    qsort(s_gc_keys, n, sizeof(s_gc_keys[0]), cmp_u32);

    cache_dir_for(source, dir, sizeof(dir));
    d = opendir(dir);
    if (!d)
        return 0;
    while ((entry = readdir(d)) != NULL)
    {
        size_t len = strlen(entry->d_name);
        if (len <= 4 || strcmp(entry->d_name + len - 4, ".mth") != 0)
            continue;
        if (gc_key_live(crc_32(entry->d_name, len - 4, 0xffffffff), n))
            continue;
        strlcpy(full, dir, sizeof(full));
        strlcat(full, "/", sizeof(full));
        strlcat(full, entry->d_name, sizeof(full));
        remove(full);
        removed++;
    }
    closedir(d);
    return removed;
}

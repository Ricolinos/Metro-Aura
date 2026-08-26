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
/* R3-F1/DD-1 (M-062): generalized from R2-F2's photo-only
 * metro_photo_thumbs.c -- same two-layer design (RAM window + on-disk
 * cache under metro_settings_metro_cache_dir()), now parametrized by a
 * metro_thumb_source so R3-F3 (artist photos) and R3-F4 (quickplay
 * album art) each plug in their own "what's the key"/"how do I decode
 * this" without a second/third copy of the window+cache bookkeeping.
 * Only one grid is ever on screen at a time, so there's exactly ONE
 * shared RAM window/pending queue for all sources -- switching source
 * (leaving Photos for Artists, say) is a metro_thumbs_reset(), same as
 * before. See DECISIONS.md M-062 for why this shape (index-based
 * pending entries, not filename-based) instead of a thinner rename. */
#ifndef METRO_THUMBS_H
#define METRO_THUMBS_H

#include <stdbool.h>
#include <stddef.h>
#include "lcd.h"

struct metro_thumb_source {
    /* Subdirectory of metro_settings_metro_cache_dir(), e.g. "photos"/
     * "artists"/"albums" -- keeps each source's .mth files apart on
     * disk (and apart from Aura's own unrelated photocache/). */
    const char *cache_subdir;

    /* Writes this item's cache-key stem (no ".mth") into out -- by
     * convention "<stable-name>.<mtime-or-equivalent>", same scheme
     * R2-F2 used for photos (albums, M-096: "a-<crc>-<mtime>", '-'
     * separated because the stem is synthetic and has no '.'), so a
     * re-synced/changed source item invalidates cleanly:
     * metro_thumbs_tick() drops any other cached file that shares the
     * part before the *last* '.' (or, with no '.', the last '-') once
     * it writes a fresh one. Returns false if this index has nothing cacheable
     * (item vanished, index out of range) -- caller gets NULL/no-op
     * instead of a garbage key. */
    bool (*cache_key)(void *ctx, int index, char *out, size_t out_len);

    /* Decodes this item straight into a METRO_TILE_SIZE x
     * METRO_TILE_SIZE dst -- whatever that takes for this source (a
     * JPEG under /Photos/, a JPEG under the artists/ cache, resolving
     * a representative track's art via tagcache for albums). Returns
     * false on failure (missing file, decode error) -- the caller
     * keeps showing the accent-tile placeholder. */
    bool (*decode)(void *ctx, int index, fb_data *dst);
};

/* Returns the decoded METRO_TILE_SIZE x METRO_TILE_SIZE thumbnail for
 * (source, ctx, index) if it's ready -- from the RAM window, or found
 * on disk and loaded into it (a raw read, cheap enough to not need the
 * decode budget). Returns NULL if not ready yet, which also queues the
 * item for metro_thumbs_tick() (deduplicated by cache key). Caller (a
 * pivot's get_tile()) falls back to metro_draw_tile()'s
 * accent-tile-with-initial placeholder in that case. */
const fb_data *metro_thumbs_get(const struct metro_thumb_source *source,
                                 void *ctx, int index);

/* Decodes and caches (RAM + disk) at most one pending thumbnail from
 * the queue metro_thumbs_get() built up -- regardless of which
 * source(s) queued it. Returns true if it did the work for one (caller
 * should force a redraw so the freshly-ready tile replaces its
 * placeholder); false if the queue was empty (idle -- nothing to do
 * this tick). Meant to be called once per metro_main() idle-loop
 * iteration, same poll as before. */
bool metro_thumbs_tick(void);

/* Clears the RAM window and the pending queue -- call when leaving a
 * grid, or switching source within the same screen, so a later visit
 * doesn't keep serving or decoding for a subset the user isn't looking
 * at anymore. Never touches the on-disk cache -- that stays valid
 * across visits. */
void metro_thumbs_reset(void);

/* Shared decode helper: FORMAT_KEEP_ASPECT JPEG decode of `path`
 * followed by a nearest-neighbour "cover" crop to METRO_TILE_SIZE x
 * METRO_TILE_SIZE, straight into `out` -- the exact algorithm R2-F2
 * used for photo thumbnails (DECISIONS.md M-057), now shared by any
 * source whose decode() is "read one JPEG file, cover-crop it" (photos
 * and artist photos both are; quickplay album art is not -- it
 * resolves a representative track first, then decodes that). Owns its
 * own static scratch buffer -- safe because only one decode happens at
 * a time by construction (metro_thumbs_tick()'s one-per-call budget),
 * so sources using this never need their own. */
bool metro_thumbs_decode_jpeg_cover(const char *path, fb_data *out);

/* M-096 (contract v15): orphan sweep for one source. Deletes every
 * .mth in that source's cache directory whose stem is not the cache
 * key of any of `count` items of `ctx` (asks source->cache_key for
 * each). Album keys are now file-derived, so a tagcache rebuild no
 * longer invalidates them -- but albums that left the library after a
 * sync would keep their thumbnails forever without this. Runs the
 * per-item key resolution in full (tagcache lookups for albums), so
 * callers gate it on metro_thumbs_take_dirty(): the flag is raised by
 * metro_sync.c's finish_ok() (music synced), by the bootstrap rebuild
 * (metro_music.c) and by the one-shot cache relocation
 * (metro_settings.c), persisted as an empty file
 * (<thumbs dir>/albums.dirty) so it survives a reboot, and consumed on
 * the next entry into the album grid. Returns the number of files
 * removed. */
int metro_thumbs_gc(const struct metro_thumb_source *source, void *ctx, int count);
void metro_thumbs_mark_dirty(void);
bool metro_thumbs_take_dirty(void);

#endif /* METRO_THUMBS_H */

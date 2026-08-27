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
/* M-097 (contract v16): the shared master image cache under
 * /.aura/art/<albums|artists|photos>/ -- Rockbox side (files, the
 * decode lock). The format itself is metro_master_art_format.h.
 *
 * Rule of the whole feature: NEVER decode a JPEG when a master exists;
 * ALWAYS write the master when one gets decoded. Every consumer
 * (metro_thumbs.c's tile engine, metro_albumart.c's Now Playing cover
 * and background, the background builder thread) goes through probe()
 * -> read() / decode + write() so that rule lives in one place.
 *
 * Threading: metro_master_art_builder.c decodes on its own low-
 * priority thread. Rockbox threads are cooperative, but disk I/O
 * yields, and read_jpeg_file() keeps its whole decoder state in ONE
 * static (apps/recorder/jpeg_load.c: `static struct jpeg jpeg`), as do
 * metro_albumart.c/metro_thumbs.c with their scratch buffers and
 * metro_music.c with s_uniqbuf -- two decodes or two tagcache walks
 * interleaved at a yield would corrupt each other. So there is ONE
 * recursive mutex here; every JPEG decode and every metro_music.c
 * tagcache walk takes it, the builder holds it for exactly one element
 * at a time (and sleeps without it). See DECISIONS.md M-097. */
#ifndef METRO_MASTER_ART_H
#define METRO_MASTER_ART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lcd.h"
#include "metro_master_art_format.h"

void metro_master_art_init(void);

/* Recursive (same-thread re-entry is fine, Rockbox mutexes count it). */
void metro_master_art_lock(void);
void metro_master_art_unlock(void);

/* One METRO_MASTER_ART_MAX_PX^2 fb_data buffer shared by every
 * consumer -- valid only while the caller holds the lock. Big enough
 * to read any master or to hold a freshly cropped one before it is
 * written/derived. */
fb_data *metro_master_art_scratch(void);

enum metro_master_art_state {
    METRO_MASTER_ART_MISSING = 0, /* neither .art nor .none: decode it */
    METRO_MASTER_ART_PRESENT,     /* <key>.art exists */
    METRO_MASTER_ART_NONE         /* <key>.none exists: no art, don't try */
};

/* subdir is "albums"/"artists"/"photos" (metro_master_art_px_for_subdir()
 * gives the expected size). key is the stem, no extension. */
enum metro_master_art_state metro_master_art_probe(const char *subdir, const char *key);

/* Reads <subdir>/<key>.art into out (px*px fb_data), validating the
 * 'MAST' header and that it is exactly px x px. False on any mismatch
 * (the caller then treats it as MISSING and re-decodes -- a corrupt or
 * foreign-sized file gets overwritten by the next write()). */
bool metro_master_art_read(const char *subdir, const char *key, fb_data *out, int px);

/* Writes <subdir>/<key>.art (creating /.aura/art/<subdir> as needed)
 * and removes any other file sharing the key's stem (old mtime) or the
 * .none marker of this same key. */
bool metro_master_art_write(const char *subdir, const char *key, const fb_data *px, int size);

/* Writes the zero-byte <subdir>/<key>.none marker (and drops a stale
 * .art of the same key, if any). */
void metro_master_art_write_none(const char *subdir, const char *key);

/* Orphan sweep of <subdir>: removes every .art/.none whose stem's
 * crc32 (of the whole key string) is not in live_crcs[0..n) -- the
 * table must be sorted ascending (same table metro_thumbs_gc() builds
 * for its own .mth sweep). Returns the number of files removed. A crc
 * collision can only KEEP a file, never delete a live one. */
int metro_master_art_gc(const char *subdir, const uint32_t *live_crcs, int n);

#endif /* METRO_MASTER_ART_H */

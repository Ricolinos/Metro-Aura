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
/* /Photos browsing (PLAN_MAESTRO.md S1.2, Aura-Firmware's contract
 * S D.1): flat JPEG list (D-192-class contract: /Photos never gets
 * subfolders) plus the OPTIONAL category index
 * (metro_media_categories.h). Viewing is Metro's own
 * metro_screen_photo_viewer.c as of R2-F3 (DD-10) -- this module no
 * longer launches imageviewer.rock itself. */
#ifndef METRO_PHOTOS_H
#define METRO_PHOTOS_H

#include "metro_fsutil.h"
#include "metro_media_categories.h"

/* Contract cap: the firmware lists up to 500 photos. */
#define METRO_PHOTOS_MAX 500

/* The flat photo folder of library-layout-v1.md. Exposed (M-097) so
 * the master builder and the grid compose "/Photos/<file>" the same
 * way; nothing else builds that path. */
#define METRO_PHOTOS_DIR "/Photos"

typedef struct {
    char filename[METRO_FSUTIL_NAME_LEN];
    metro_photo_cat_t category;
    /* R2-F2/DD-9: source file's mtime -- metro_thumbs.c's cache
     * invalidation key (a re-synced file with the same name but new
     * content must not serve a stale cached thumbnail). */
    long mtime;
} metro_photo_item_t;

/* Re-scans /Photos fresh (natural order, .jpg/.jpeg) and reloads the
 * category index alongside it -- same "refresh on enter" pattern as
 * metro_video_list() / metro_music.c's own lists (DESVIACIONES.md
 * F6-1). Call once when the photos page is entered. */
int metro_photos_list(metro_photo_item_t *out, int max);

/* M-097 (contract v16): master art key of a photo --
 * "p-<crc32 of "/Photos/<filename>">.<mtime>" (metro_master_art_format.h). */
void metro_photos_master_key(const char *filename, long mtime, char *out, size_t outsz);

/* True for a directory entry the photo list would show: .jpg/.jpeg,
 * not hidden (metro_fsutil_is_hidden_name()). Same filter
 * metro_photos_list() applies, for callers that walk /Photos
 * themselves one entry at a time (the master builder). */
bool metro_photos_name_is_photo(const char *name);

#endif /* METRO_PHOTOS_H */

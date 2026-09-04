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
/* Directory scanning shared by metro_video.c/metro_photos.c -- both
 * browse a flat directory (D-192 of Aura-Firmware's contract: /Videos
 * and /Photos never get subfolders) filtered by extension, natural
 * order. Narrower than Aura-Firmware's aura_fsutil.h on purpose: that
 * one is about deleting caches Metro doesn't have (no disk-cached
 * thumbnails, no theme installs in v1) -- this one is just the listing
 * primitive neither metro_video.c nor metro_photos.c should duplicate. */
#ifndef METRO_FSUTIL_H
#define METRO_FSUTIL_H

#include <stdbool.h>

/* R4/FA-2 (M-075): true para lo que NINGUNA lista de Metro debe
 * mostrar jamás.
 *
 * El caso que motivó esto: macOS, al escribir en el FAT del iPod, deja
 * sidecars AppleDouble llamados `._<nombre original>` -- y **conservan
 * la extensión**. `._IMG_1234.jpg` pasaba el filtro de extensión (que
 * compara solo el sufijo) y no es un directorio, así que se listaba
 * como una foto más. Lo mismo con `._Mi Lista.m3u8` entre las
 * playlists. Se observó en vivo en el iPod del dueño: `._rockbox.ipod`,
 * `._version.txt`, `._sync-pending.json`, más `.Spotlight-V100`,
 * `.Trashes` y `.fseventsd`.
 *
 * La regla es **el punto inicial**, no el `._` específico: en FAT un
 * nombre que empieza con punto es oculto por convención, ninguna de las
 * fuentes de contenido legítimo lo genera (Aura Studio sanea nombres;
 * una copia manual tampoco), y así cubre de paso `.DS_Store`, los
 * directorios de servicio de macOS, y `.`/`..`. Un punto en cualquier
 * OTRA posición (`mi.foto.jpg`) no se ve afectado.
 *
 * Defensivo a propósito, y solo del lado del firmware (decisión del
 * dueño): el usuario puede copiar archivos a mano sin que Studio
 * intervenga, así que filtrar al leer es lo único que cubre todos los
 * caminos. `static inline` en el header para que quede cubierto por el
 * arnés de host sin arrastrar dependencias de Rockbox. */
static inline bool metro_fsutil_is_hidden_name(const char *name)
{
    return name == NULL || name[0] == '.';
}

/* PLAN_MAESTRO.md S1.2: VIDEO_NAME_LEN/PHOTO_NAME_LEN, both 96 bytes
 * (contract: filenames <= 95 bytes UTF-8 including extension, + NUL). */
#define METRO_FSUTIL_NAME_LEN 96

/* Lists `dir`'s entries whose name ends (case-insensitively) in one of
 * `exts` (an array of `n_exts` strings, each including the leading
 * dot, e.g. ".jpg") into `out` (`max` buffers of METRO_FSUTIL_NAME_LEN
 * bytes each), naturally sorted (strnatcasecmp -- "2.jpg" before
 * "10.jpg", case-insensitive) over the WHOLE matching set before
 * truncating to `max` -- the result is always the true first `max` in
 * natural order, never whatever readdir() happened to return first.
 * Does not recurse. Returns the count placed into `out` (0 if `dir`
 * doesn't exist). Matching entries beyond METRO_FSUTIL_SCAN_CEILING
 * are not considered at all (see its own comment) -- irrelevant for
 * the contract's own caps (100 videos, 500 photos). */
int metro_fsutil_list_by_ext(const char *dir, const char *const *exts, int n_exts,
                              char out[][METRO_FSUTIL_NAME_LEN], int max);

/* R2-F2/DD-9: same scan as metro_fsutil_list_by_ext(), plus each
 * entry's mtime in out_mtimes (parallel array, same indices as out) --
 * metro_thumbs.c's cache invalidation key. out_mtimes may be
 * NULL (metro_fsutil_list_by_ext() itself is exactly this call with
 * NULL). */
int metro_fsutil_list_by_ext_mtime(const char *dir, const char *const *exts, int n_exts,
                                    char out[][METRO_FSUTIL_NAME_LEN], long out_mtimes[],
                                    int max);

/* M-102 (contract v18): mtime of ONE file, or 0 if it does not exist.
 *
 * Rockbox has no stat(): the only way to a file's mtime is
 * dir_get_info() over its parent directory, which is what the scans
 * above already do. This is that same read, narrowed to a single name
 * -- used by the album art key to pick up a cover.jpg that Studio
 * rewrote without touching any track. 0 (the "absent" answer) is also
 * what a directory that cannot be opened returns: a key must never
 * depend on whether a read happened to fail. */
long metro_fsutil_file_mtime(const char *path);

#endif /* METRO_FSUTIL_H */

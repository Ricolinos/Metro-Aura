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

#include "config.h"
#include "file.h"
#include "dir.h"
#include "kernel.h"
#include "mutex.h"
#include "string-extra.h"
#include "crc32.h"

#include "metro_master_art.h"
#include "metro_settings.h"

#if LCD_PIXELFORMAT != RGB565 || (LCD_DEPTH != 16)
#error "master art files are raw RGB565 LE: fb_data must be 16-bit RGB565"
#endif

static struct mutex s_lock;
static fb_data s_scratch[METRO_MASTER_ART_MAX_PX * METRO_MASTER_ART_MAX_PX];

void metro_master_art_init(void)
{
    mutex_init(&s_lock);
}

void metro_master_art_lock(void)   { mutex_lock(&s_lock); }
void metro_master_art_unlock(void) { mutex_unlock(&s_lock); }

fb_data *metro_master_art_scratch(void)
{
    return s_scratch;
}

static void art_path(const char *subdir, const char *key, const char *ext,
                     char *out, size_t outsz)
{
    metro_settings_master_art_dir(subdir, out, outsz);
    strlcat(out, "/", outsz);
    strlcat(out, key, outsz);
    strlcat(out, ext, outsz);
}

/* /.aura may not exist on a fresh disk (M-096's ensure_cache_dir()
 * has the same three-level dance for /.aura/thumbs). */
static void ensure_dir(const char *subdir)
{
    char dir[MAX_PATH], up[MAX_PATH];
    char *slash;

    metro_settings_master_art_dir(subdir, dir, sizeof(dir));
    if (dir_exists(dir))
        return;
    strlcpy(up, dir, sizeof(up));
    slash = strrchr(up, '/');
    if (slash) *slash = '\0';
    if (!dir_exists(up))
    {
        char root[MAX_PATH];
        strlcpy(root, up, sizeof(root));
        slash = strrchr(root, '/');
        if (slash && slash != root)
        {
            *slash = '\0';
            if (!dir_exists(root))
                mkdir(root);
        }
        mkdir(up);
    }
    mkdir(dir);
}

enum metro_master_art_state metro_master_art_probe(const char *subdir, const char *key)
{
    char path[MAX_PATH];

    art_path(subdir, key, METRO_MASTER_ART_EXT, path, sizeof(path));
    if (file_exists(path))
        return METRO_MASTER_ART_PRESENT;
    art_path(subdir, key, METRO_MASTER_ART_NONE_EXT, path, sizeof(path));
    if (file_exists(path))
        return METRO_MASTER_ART_NONE;
    return METRO_MASTER_ART_MISSING;
}

bool metro_master_art_read(const char *subdir, const char *key, fb_data *out, int px)
{
    char path[MAX_PATH];
    uint8_t hdr[METRO_MASTER_ART_HEADER_SIZE];
    uint16_t w, h;
    ssize_t want = (ssize_t)px * px * sizeof(fb_data);
    int fd;
    bool ok;

    art_path(subdir, key, METRO_MASTER_ART_EXT, path, sizeof(path));
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    ok = read(fd, hdr, sizeof(hdr)) == (ssize_t)sizeof(hdr)
        && metro_master_art_header_parse(hdr, &w, &h)
        && w == px && h == px
        && read(fd, out, want) == want;
    close(fd);
    return ok;
}

/* Everything in the directory whose name starts with the key's stem
 * (up to the last '.') and is not `keep` -- an older mtime of the same
 * item, or the opposite marker (.art vs .none) of this very key. */
static void remove_siblings(const char *subdir, const char *key, const char *keep)
{
    char dir[MAX_PATH], full[MAX_PATH];
    const char *dot = strrchr(key, '.');
    size_t stem_len = dot ? (size_t)(dot - key) : strlen(key);
    DIR *d;
    struct DIRENT *e;

    metro_settings_master_art_dir(subdir, dir, sizeof(dir));
    d = opendir(dir);
    if (!d)
        return;
    while ((e = readdir(d)) != NULL)
    {
        if (strncmp(e->d_name, key, stem_len) != 0 || e->d_name[stem_len] != '.')
            continue;
        if (!strcmp(e->d_name, keep))
            continue;
        strlcpy(full, dir, sizeof(full));
        strlcat(full, "/", sizeof(full));
        strlcat(full, e->d_name, sizeof(full));
        remove(full);
    }
    closedir(d);
}

bool metro_master_art_write(const char *subdir, const char *key, const fb_data *px, int size)
{
    char path[MAX_PATH], name[MAX_PATH];
    uint8_t hdr[METRO_MASTER_ART_HEADER_SIZE];
    ssize_t bytes = (ssize_t)size * size * sizeof(fb_data);
    int fd;
    bool ok;

    ensure_dir(subdir);
    strlcpy(name, key, sizeof(name));
    strlcat(name, METRO_MASTER_ART_EXT, sizeof(name));
    remove_siblings(subdir, key, name);

    art_path(subdir, key, METRO_MASTER_ART_EXT, path, sizeof(path));
    fd = creat(path, 0666);
    if (fd < 0)
        return false;
    metro_master_art_header_pack(hdr, (uint16_t)size, (uint16_t)size);
    ok = write(fd, hdr, sizeof(hdr)) == (ssize_t)sizeof(hdr)
        && write(fd, px, bytes) == bytes;
    close(fd);
    if (!ok)
        remove(path); /* never leave a short file another family would trust */
    return ok;
}

void metro_master_art_write_none(const char *subdir, const char *key)
{
    char path[MAX_PATH], name[MAX_PATH];
    int fd;

    ensure_dir(subdir);
    strlcpy(name, key, sizeof(name));
    strlcat(name, METRO_MASTER_ART_NONE_EXT, sizeof(name));
    remove_siblings(subdir, key, name);

    art_path(subdir, key, METRO_MASTER_ART_NONE_EXT, path, sizeof(path));
    fd = creat(path, 0666);
    if (fd >= 0)
        close(fd);
}

static bool crc_live(uint32_t h, const uint32_t *keys, int n)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        if (keys[mid] == h) return true;
        if (keys[mid] < h) lo = mid + 1; else hi = mid - 1;
    }
    return false;
}

int metro_master_art_gc(const char *subdir, const uint32_t *live_crcs, int n)
{
    char dir[MAX_PATH], full[MAX_PATH];
    DIR *d;
    struct DIRENT *e;
    int removed = 0;

    metro_settings_master_art_dir(subdir, dir, sizeof(dir));
    d = opendir(dir);
    if (!d)
        return 0;
    while ((e = readdir(d)) != NULL)
    {
        size_t len = strlen(e->d_name), ext;
        if (len > 4 && !strcmp(e->d_name + len - 4, METRO_MASTER_ART_EXT))
            ext = 4;
        else if (len > 5 && !strcmp(e->d_name + len - 5, METRO_MASTER_ART_NONE_EXT))
            ext = 5;
        else
            continue;
        if (crc_live(crc_32(e->d_name, len - ext, 0xffffffff), live_crcs, n))
            continue;
        strlcpy(full, dir, sizeof(full));
        strlcat(full, "/", sizeof(full));
        strlcat(full, e->d_name, sizeof(full));
        remove(full);
        removed++;
    }
    closedir(d);
    return removed;
}

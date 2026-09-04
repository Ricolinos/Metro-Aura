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

#include "dir.h"
#include "file.h"
#include "string-extra.h"
#include "strnatcmp.h"

#include "metro_fsutil.h"

/* Generous upper bound on how many matching files a single directory
 * scan considers before sorting/truncating to the caller's `max` --
 * see metro_fsutil.h. Static, not stack (same D-226-class concern as
 * metro_music.c's own scratch buffers): 4096 * 96 bytes = 384KB,
 * trivial against 64MB RAM. */
#define METRO_FSUTIL_SCAN_CEILING 4096

static char s_scan[METRO_FSUTIL_SCAN_CEILING][METRO_FSUTIL_NAME_LEN];
/* R2-F2/DD-9: mtime alongside each name, same scan -- metro_thumbs.c
 * needs it as the thumbnail cache's invalidation key, and dir_get_info()
 * is already called per entry below for the ATTR_DIRECTORY check
 * (M-053), so this is free. */
static long s_scan_mtime[METRO_FSUTIL_SCAN_CEILING];

static bool matches_any_ext(const char *name, const char *const *exts, int n_exts)
{
    size_t len = strlen(name);
    int i;

    for (i = 0; i < n_exts; i++)
    {
        size_t elen = strlen(exts[i]);
        if (len > elen && !strcasecmp(name + len - elen, exts[i]))
            return true;
    }
    return false;
}

static void sort_natural(char names[][METRO_FSUTIL_NAME_LEN], long mtimes[], int n)
{
    int a, b;

    for (a = 1; a < n; a++)
    {
        char key[METRO_FSUTIL_NAME_LEN];
        long key_mtime = mtimes[a];
        strlcpy(key, names[a], sizeof(key));
        b = a - 1;
        while (b >= 0 && strnatcasecmp(names[b], key) > 0)
        {
            strlcpy(names[b + 1], names[b], METRO_FSUTIL_NAME_LEN);
            mtimes[b + 1] = mtimes[b];
            b--;
        }
        strlcpy(names[b + 1], key, METRO_FSUTIL_NAME_LEN);
        mtimes[b + 1] = key_mtime;
    }
}

int metro_fsutil_list_by_ext(const char *dir, const char *const *exts, int n_exts,
                              char out[][METRO_FSUTIL_NAME_LEN], int max)
{
    return metro_fsutil_list_by_ext_mtime(dir, exts, n_exts, out, NULL, max);
}

int metro_fsutil_list_by_ext_mtime(const char *dir, const char *const *exts, int n_exts,
                                    char out[][METRO_FSUTIL_NAME_LEN], long out_mtimes[],
                                    int max)
{
    DIR *d;
    struct DIRENT *entry;
    int n = 0, i;

    d = opendir(dir);
    if (!d)
        return 0;

    while (n < METRO_FSUTIL_SCAN_CEILING && (entry = readdir(d)) != NULL)
    {
        struct dirinfo info;

        if (metro_fsutil_is_hidden_name(entry->d_name))
            continue; /* R4/FA-2: AppleDouble y compañía */
        if (!matches_any_ext(entry->d_name, exts, n_exts))
            continue;

        /* R2-F1/DD-3 (M-053): a directory can carry a name that
         * matches one of our extensions (e.g. "Folder.jpg/" -- some
         * desktop tools create these) -- same dir_get_info()/
         * ATTR_DIRECTORY check apps/filetree.c uses to skip
         * directories in its own extension-filtered scan. */
        info = dir_get_info(d, entry);
        if (info.attribute & ATTR_DIRECTORY)
            continue;

        strlcpy(s_scan[n], entry->d_name, METRO_FSUTIL_NAME_LEN);
        s_scan_mtime[n] = (long)info.mtime;
        n++;
    }
    closedir(d);

    sort_natural(s_scan, s_scan_mtime, n);

    if (n > max)
        n = max;
    for (i = 0; i < n; i++)
    {
        strlcpy(out[i], s_scan[i], METRO_FSUTIL_NAME_LEN);
        if (out_mtimes)
            out_mtimes[i] = s_scan_mtime[i];
    }

    return n;
}

long metro_fsutil_file_mtime(const char *path)
{
    char dir[MAX_PATH];
    const char *name;
    char *slash;
    DIR *d;
    struct DIRENT *entry;
    long mtime = 0;

    if (!path || !path[0])
        return 0;
    strlcpy(dir, path, sizeof(dir));
    slash = strrchr(dir, '/');
    if (!slash)
        return 0;
    *slash = '\0';
    name = path + (slash - dir) + 1;
    if (!name[0])
        return 0;
    /* A file directly at the root: the parent is "/", not "". */
    d = opendir(dir[0] ? dir : "/");
    if (!d)
        return 0;
    while ((entry = readdir(d)) != NULL)
    {
        if (strcasecmp(entry->d_name, name))
            continue;
        mtime = (long)dir_get_info(d, entry).mtime;
        break;
    }
    closedir(d);
    return mtime;
}

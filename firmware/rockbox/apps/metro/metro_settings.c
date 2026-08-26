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
#include <stdlib.h>

#include "file.h"
#include "dir.h"
#include "misc.h"
#include "rbpaths.h"
#include "rtc.h"
#include "string-extra.h"
#include "timefuncs.h"
#include "system.h"          /* system_reboot() -- M-090 */
#include "settings.h"        /* settings_save() -- M-090 */
#include "ata_idle_notify.h" /* call_storage_idle_notifys() -- M-090 */
#include "tagcache.h"        /* tagcache_shutdown() -- M-090 */

#include "metro_settings.h"
#include "metro_sync.h" /* marcador de sync al cambiar de firmware -- M-090 */
#include "metro_firmware_families.h" /* tabla de hermanos -- M-093 */

#define METRO_DIR      ROCKBOX_DIR "/aura"
#define METRO_CFG_PATH METRO_DIR "/aura.cfg"

metro_settings_t metro_settings;

static const metro_settings_t defaults = {
    .theme = METRO_THEME_DEFAULT,
    .accent = METRO_ACCENT_DEFAULT,
    .language = METRO_LANG_ES,
    .animations = METRO_ANIM_DEFAULT,
    .graphics = METRO_GFX_DEFAULT,
    .tz_local_quarters = 0,
    .first_boot_done = false,
    .screen_lock = false,
    .screen_lock_pin = "",
};

static int clamp_enum(int v, int count)
{
    return (v >= 0 && v < count) ? v : 0;
}

void metro_settings_load(void)
{
    int fd;
    char line[64];
    bool existed;

    metro_settings = defaults;

    fd = open(METRO_CFG_PATH, O_RDONLY);
    existed = fd >= 0;
    if (existed)
    {
        while (read_line(fd, line, sizeof(line)) > 0)
        {
            char *name, *value;
            int v;

            if (!settings_parseline(line, &name, &value))
                continue;
            v = atoi(value);

            if (!strcmp(name, "theme"))
                metro_settings.theme = (enum metro_theme_kind)clamp_enum(v, 2);
            else if (!strcmp(name, "accent"))
                metro_settings.accent = (enum metro_accent)clamp_enum(v, METRO_ACCENT_COUNT);
            else if (!strcmp(name, "language"))
                metro_settings.language = (enum metro_language)clamp_enum(v, METRO_LANG_COUNT);
            else if (!strcmp(name, "animations"))
                metro_settings.animations = (enum metro_anim_level)clamp_enum(v, METRO_ANIM_COUNT);
            else if (!strcmp(name, "graphics"))
                metro_settings.graphics = (enum metro_gfx_level)clamp_enum(v, METRO_GFX_COUNT);
            else if (!strcmp(name, "tz_local_quarters"))
                metro_settings.tz_local_quarters = v;
            else if (!strcmp(name, "first_boot_done"))
                metro_settings.first_boot_done = (v != 0);
            else if (!strcmp(name, "screen_lock"))
                metro_settings.screen_lock = (v != 0);
            else if (!strcmp(name, "screen_lock_pin"))
            {
                /* Cadena, no atoi() -- los ceros a la izquierda son
                 * parte de la clave. metro_screen_lock.c valida que
                 * sean 4 dígitos y falla ABIERTO si no lo son. */
                strlcpy(metro_settings.screen_lock_pin, value,
                         sizeof(metro_settings.screen_lock_pin));
            }
            /* firmware_family/sync_marker_supported: write-only, Aura
             * Studio reads these off the mounted disk -- never read back
             * here. rtc_sync_*: transient, only
             * metro_settings_apply_pending_clock() reads them, straight
             * from disk, not through this struct. */
        }
        close(fd);
    }

    metro_settings.first_boot_done = true;
    if (!existed)
        metro_settings_save();
}

void metro_settings_save(void)
{
    int fd;

    if (!dir_exists(METRO_DIR))
        mkdir(METRO_DIR);

    fd = creat(METRO_CFG_PATH, 0666);
    if (fd < 0)
        return;

    fdprintf(fd, "firmware_family: metro\n");
    fdprintf(fd, "sync_marker_supported: 1\n");
    fdprintf(fd, "theme: %d\n", (int)metro_settings.theme);
    fdprintf(fd, "accent: %d\n", (int)metro_settings.accent);
    fdprintf(fd, "language: %d\n", (int)metro_settings.language);
    fdprintf(fd, "animations: %d\n", metro_settings.animations);
    fdprintf(fd, "graphics: %d\n", metro_settings.graphics);
    fdprintf(fd, "tz_local_quarters: %d\n", metro_settings.tz_local_quarters);
    fdprintf(fd, "first_boot_done: %d\n", metro_settings.first_boot_done ? 1 : 0);

    /* R3-F7/DD-8 (M-068): las dos claves del candado se escriben SOLO
     * cuando hay candado. Así, un aparato sin candado no las tiene en
     * absoluto (en vez de un `screen_lock: 0` permanente), y sobre todo:
     * la salida de emergencia documentada -- "conecta por USB y borra
     * estas dos líneas" -- deja un archivo que no las vuelve a hacer
     * crecer solo en el siguiente guardado. */
    if (metro_settings.screen_lock)
    {
        fdprintf(fd, "screen_lock: 1\n");
        fdprintf(fd, "screen_lock_pin: %s\n", metro_settings.screen_lock_pin);
    }

    close(fd);
}

void metro_settings_apply_pending_clock(void)
{
    int fd;
    char line[64];
    struct tm tm;
    bool have_year = false, have_month = false, have_day = false;
    bool have_hour = false, have_min = false, have_sec = false;

    memset(&tm, 0, sizeof(tm));

    fd = open(METRO_CFG_PATH, O_RDONLY);
    if (fd < 0)
        return;

    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char *name, *value;
        int v;

        if (!settings_parseline(line, &name, &value))
            continue;
        v = atoi(value);

        if (!strcmp(name, "rtc_sync_year"))       { tm.tm_year = v - 1900; have_year  = true; }
        else if (!strcmp(name, "rtc_sync_month")) { tm.tm_mon  = v - 1;    have_month = true; }
        else if (!strcmp(name, "rtc_sync_day"))   { tm.tm_mday = v;        have_day   = true; }
        else if (!strcmp(name, "rtc_sync_hour"))  { tm.tm_hour = v;        have_hour  = true; }
        else if (!strcmp(name, "rtc_sync_min"))   { tm.tm_min  = v;        have_min   = true; }
        else if (!strcmp(name, "rtc_sync_sec"))   { tm.tm_sec  = v;        have_sec   = true; }
        else if (!strcmp(name, "tz_local_quarters"))
            metro_settings.tz_local_quarters = v;
    }
    close(fd);

    if (have_year && have_month && have_day && have_hour && have_min && have_sec)
    {
#if CONFIG_RTC
        rtc_write_datetime(&tm);
#endif
        /* Rewrites aura.cfg entirely from metro_settings -- the
         * rtc_sync_* keys just read aren't part of that struct, so
         * they disappear on their own, nothing to delete by hand. */
        metro_settings_save();
    }
}

void metro_ensure_media_dirs(void)
{
    static const char *const dirs[] = { "/Music", "/Videos", "/Photos", "/Playlists" };
    unsigned i;

    for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++)
    {
        if (!dir_exists(dirs[i]))
            mkdir(dirs[i]);
    }
}

void metro_settings_metro_cache_dir(const char *subdir, char *out, size_t outsz)
{
    snprintf(out, outsz, "%s/metrocache/%s", METRO_DIR, subdir);
}

/* R3-F3/DD-6 (M-064): Studio's own index + photo cache -- distinct
 * from metro_settings_metro_cache_dir("artists", ...) above, which is
 * Metro's OWN derived 80x80 tile cache
 * (.../aura/metrocache/artists/). These two point at
 * .../aura/artist_images.cfg and .../aura/artists/ respectively --
 * Studio writes both, Metro only ever reads them. */
void metro_settings_artist_images_cfg_path(char *out, size_t outsz)
{
    strlcpy(out, METRO_DIR "/artist_images.cfg", outsz);
}

void metro_settings_artists_dir(char *out, size_t outsz)
{
    strlcpy(out, METRO_DIR "/artists", outsz);
}

/* R3-F5/DD-7 (M-066): Studio writes this, Metro only ever reads it --
 * same one-way relationship as artist_images.cfg above. */
void metro_settings_ratings_cfg_path(char *out, size_t outsz)
{
    strlcpy(out, METRO_DIR "/ratings.cfg", outsz);
}

/* --- R5 (M-090, contrato v10): cambio de firmware por renombre -------
 * M-093: generalizado a cualquier hermano de metro_firmware_families.h. */

#define METRO_FW_ACTIVE_DIR    ROCKBOX_DIR        /* "/.rockbox" */
#define METRO_FW_ROOT_BINARY   "/rockbox.ipod"
#define METRO_FW_TREE_BINARY   ROCKBOX_DIR "/rockbox.ipod"

bool metro_firmware_sibling_installed(int i)
{
    const struct metro_fw_family *sibling = metro_fw_sibling(i);

    return sibling != NULL && dir_exists(sibling->dormant_dir);
}

/* /rockbox.ipod := /.rockbox/rockbox.ipod, a trozos, con buffer estatico
 * (nunca en la pila de 8 KB). No es fatal si falla: el bootloader
 * prefiere el del arbol y este es solo el respaldo. */
static void refresh_root_binary(void)
{
    static char buf[16 * 1024];
    int in, out;
    ssize_t n;

    in = open(METRO_FW_TREE_BINARY, O_RDONLY);
    if (in < 0)
        return;
    out = creat(METRO_FW_ROOT_BINARY, 0666);
    if (out < 0)
    {
        close(in);
        return;
    }
    while ((n = read(in, buf, sizeof(buf))) > 0)
        if (write(out, buf, (size_t)n) != n)
            break;
    close(out);
    close(in);
}

bool metro_firmware_switch_to(int i)
{
    const struct metro_fw_family *sibling = metro_fw_sibling(i);

    if (sibling == NULL || !metro_firmware_sibling_installed(i))
        return false;
    if (dir_exists(METRO_FW_OWN_DORMANT))
        return false; /* no adivinar: Studio garantiza que no pase */

    /* 1. todo lo de Metro al disco, AHORA */
    metro_settings_save();
    settings_save();
    tagcache_shutdown();
    call_storage_idle_notifys(true);

    /* 2. saliente primero */
    if (rename(METRO_FW_ACTIVE_DIR, METRO_FW_OWN_DORMANT) < 0)
        return false;

    /* 3. entrante */
    if (rename(sibling->dormant_dir, METRO_FW_ACTIVE_DIR) < 0)
    {
        /* deshacer el paso 2: seguimos siendo Metro */
        rename(METRO_FW_OWN_DORMANT, METRO_FW_ACTIVE_DIR);
        return false;
    }

    /* 4 y 5 -- el marcador SOLO si la biblioteca cambio desde que el
     * hermano construyo su base (M-091, contrato v12): sin sync de por
     * medio el cambio es instantaneo, sin "optimizando" de 5 minutos. */
    refresh_root_binary();
    if (metro_sync_switch_needs_rebuild(METRO_FW_OWN_DORMANT))
        metro_sync_write_music_pending_marker();

    /* 6: en seco. Nada de lo de arriba queda pendiente de escribir. */
    system_reboot();
    return true; /* no se alcanza */
}

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
/* Reads/writes /.rockbox/aura/aura.cfg (settings_parseline()/read_line(),
 * regenerates the whole file on every save -- PLAN_MAESTRO.md S1.2, M-017).
 * Aura Studio's AuraDeviceProbe reads this file to decide whether the
 * device has ever booted Metro; the file existing with the right keys IS
 * the contract, regardless of what those values are (E.2: it must exist
 * from the very first boot). */
#ifndef METRO_SETTINGS_H
#define METRO_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include "metro_theme.h"
#include "metro_lang.h"

/* F11: FX level matrix (PLAN_MAESTRO.md M-015/S3) -- animations is
 * "how it moves" (metro_transitions.c's frame count), graphics is
 * "what gets drawn" (currently just whether FADE does a real per-pixel
 * blend or falls back to a slide, metro_fb.c's own doc comment).
 * Canon = maximum fidelity (ALL/FULL); lower levels are subtractions
 * on the same code, never a different code path. */
enum metro_anim_level {
    METRO_ANIM_OFF = 0,
    METRO_ANIM_MINIMAL,
    METRO_ANIM_ALL,
    METRO_ANIM_COUNT
};
#define METRO_ANIM_DEFAULT METRO_ANIM_ALL

enum metro_gfx_level {
    METRO_GFX_LITE = 0,
    METRO_GFX_FULL,
    METRO_GFX_COUNT
};
#define METRO_GFX_DEFAULT METRO_GFX_FULL

typedef struct {
    enum metro_theme_kind theme;
    enum metro_accent accent;
    enum metro_language language;
    enum metro_anim_level animations;
    enum metro_gfx_level graphics;
    int tz_local_quarters; /* quarter-hours from UTC, D.4 of the contract */
    bool first_boot_done;
    /* R3-F7/DD-8 (M-068): candado de interfaz. La clave se guarda como
     * CADENA de 4 dígitos, no como entero -- con un entero, "0000" y
     * "clave ausente" son el mismo valor 0, que es exactamente la
     * trampa por la que un aura.cfg a medio escribir dejaría el aparato
     * bloqueado con una clave que nadie configuró (Aura-Firmware la
     * tiene). Texto plano a propósito: es un candado de interfaz, no de
     * datos -- ver metro_screen_lock.h. Cadena vacía = sin clave. */
    bool screen_lock;
    char screen_lock_pin[5];
} metro_settings_t;

extern metro_settings_t metro_settings;

/* Loads from disk into metro_settings, falling back to defaults for
 * any key that's missing or the file doesn't exist at all -- in which
 * case this also creates it (E.2, first-boot guarantee). Does NOT
 * apply theme/accent/language anywhere; the caller (metro_main.c)
 * does that right after, in one place. */
void metro_settings_load(void);

/* Regenerates aura.cfg entirely from metro_settings -- never edits
 * the file in place. Call after changing any field. */
void metro_settings_save(void);

/* D.4 of the contract: reads the six transient rtc_sync_* keys (and
 * the persistent tz_local_quarters) straight from disk -- NOT from
 * metro_settings, which may be stale relative to a marker Aura Studio
 * just wrote. If all six are present, sets the real RTC and calls
 * metro_settings_save() (which drops the transient keys on its own,
 * they were never part of metro_settings_t). No-op otherwise. Call at
 * the same two moments as the sync marker: boot and after returning
 * from the USB screen. */
void metro_settings_apply_pending_clock(void);

/* R2-F1/DD-4 (M-054): creates /Music, /Videos, /Photos, /Playlists
 * (mkdir(), no-op if a path already exists) -- library-layout-v1.md's
 * four top-level media folders, so a device that never went through
 * an Aura Studio sync (music copied over USB by hand, or a completely
 * fresh install) still has somewhere for Metro's own screens/plugins
 * to read and write from on first boot. Call at the same two moments
 * as metro_settings_apply_pending_clock(): boot and after returning
 * from the USB screen (metro_main.c's metro_disk_handoff()) -- a USB
 * session is exactly when a folder could have been deleted or a fresh
 * disk mounted. */
void metro_ensure_media_dirs(void);

/* --- Contract v15 (M-095/M-096): shared, family-independent state ---
 * Both live at the disk ROOT under /.aura, next to the sync marker and
 * the library stamp, NOT inside any firmware tree: a firmware switch
 * (v10, two renames) must not carry the tagcache database or the
 * thumbnails along with the tree that built them. The three families
 * (Aura, Metro, moonlit) share byte-identical apps/tagcache.c (same
 * TAGCACHE_MAGIC) and the same 80x80 raw .mth thumbnail format, so one
 * copy of each serves all of them. metro_settings.h owns these path
 * literals (CLAUDE.md's compat-path rule); nothing else spells them. */
#define AURA_SHARED_DB_DIR     "/.aura/tagcache"
#define AURA_SHARED_THUMBS_DIR "/.aura/thumbs"
/* M-097 (contract v16): the shared MASTER image cache -- one decoded
 * RGB565 square per album/artist/photo, written once by whichever
 * family is active and read by all three (metro_master_art_format.h). */
#define AURA_SHARED_ART_DIR    "/.aura/art"

/* M-102 (contract v18): the FORMAT VERSION of everything derived from
 * a source image. A cache entry carries no version of its own, so a
 * tile derived by a version of the firmware with a bug in its cropping
 * survives the fix forever -- the key only changes when the SOURCE
 * changes. This file is how a firmware says "what is on this disk was
 * derived by rules older than mine": on boot each family reads it and,
 * if it is missing or lower than its own, throws away every derived
 * cache on the disk and writes its number. Studio never touches it
 * (same rule as the rest of /.aura/art).
 *
 * Version 2 = contract v18 (square fill+center-crop everywhere, album
 * key with the sibling cover.jpg mtime). "Missing" means version 1,
 * i.e. anything written before v18. */
#define AURA_SHARED_ART_FORMAT_VERSION 2

/* M-095: points global_settings.tagcache_db_path at AURA_SHARED_DB_DIR
 * and migrates a per-tree database (ROCKBOX_DIR/database_*.tcd, the
 * pre-v15 location) into it by rename() -- same FAT partition, no
 * copy, no rebuild. MUST run after settings_load() (which would
 * otherwise restore whatever "database path" the on-disk config.cfg
 * carries) and before tagcache_init() (which copies the setting into
 * tc_stat.db_path once and never re-reads it) -- i.e. from
 * metro_apply_hygiene(), which apps/main.c's init() already calls at
 * exactly that point (M-019). See DECISIONS.md M-095. */
void metro_force_shared_db_path(void);

/* M-096: one-shot rename of the pre-v15 per-tree thumbnail cache
 * (ROCKBOX_DIR/aura/metrocache/) to AURA_SHARED_THUMBS_DIR when the
 * shared one doesn't exist yet; if it does, the per-tree copy is
 * deleted (it's derived data, regenerable on demand). Same call site
 * as metro_force_shared_db_path(). */
void metro_settings_migrate_shared_thumbs(void);

/* R2-F2/DD-9 (M-057), generalized R3-F1/DD-1, relocated M-096:
 * AURA_SHARED_THUMBS_DIR/<subdir>/ -- the on-disk thumbnail cache, one
 * subdirectory per source (photos/artists/albums), raw 80x80 fb_data
 * .mth files shared by the three families (contract v15). The only
 * function allowed to build this path (CLAUDE.md's compat-path rule)
 * -- metro_thumbs.c calls this instead of composing the path itself.
 * Writes into `out` (does not create the directory -- caller's job,
 * mkdir() if missing, before writing inside it). */
void metro_settings_metro_cache_dir(const char *subdir, char *out, size_t outsz);

/* M-097: AURA_SHARED_ART_DIR/<subdir>/ (albums/artists/photos) -- the
 * shared master cache directory, same rule as above: only this
 * function spells the path, metro_master_art.c asks for it. */
void metro_settings_master_art_dir(const char *subdir, char *out, size_t outsz);

/* M-102: AURA_SHARED_ART_DIR/format.txt -- same rule, only this module
 * spells the path. */
void metro_settings_master_art_format_path(char *out, size_t outsz);

/* M-102: deletes EVERY derived image cache on the disk -- the shared
 * master cache (everything under /.aura/art, keeping that directory
 * itself so the version file can be written next to it) and Metro's
 * own L2 tiles under /.aura/thumbs. Both are derived data: nothing
 * here is the user's, everything regenerates from the JPEGs Studio
 * put on the disk.
 * Returns how many files it removed. Only metro_master_art.c calls it,
 * and only when the format version on disk is older than this
 * firmware's. */
int metro_settings_purge_derived_caches(void);

/* R3-F3/DD-6 (M-064): .../aura/artist_images.cfg (Studio's index) and
 * .../aura/artists/ (Studio's own source photo cache, the directory
 * artist_images.cfg's filenames resolve into) -- CONTRATO-firmware-studio.md
 * §D.3. Distinct from metro_settings_metro_cache_dir("artists", ...)
 * above, Metro's OWN derived 80x80 tile cache. */
void metro_settings_artist_images_cfg_path(char *out, size_t outsz);
void metro_settings_artists_dir(char *out, size_t outsz);

/* R3-F5/DD-7 (M-066): .../aura/ratings.cfg -- Studio's one-way ratings
 * export (`<path absoluta>: <rating 0-10>` por línea), reimportado por
 * metro_sync.c cada vez que el import de música termina bien. */
void metro_settings_ratings_cfg_path(char *out, size_t outsz);

/* R5 (M-090, contrato v10 -- varios firmwares instalados a la vez;
 * M-093: tres familias, tabla en metro_firmware_families.h).
 *
 * El arbol activo es siempre /.rockbox (lo unico que el bootloader
 * arranca); cada familia hermana duerme, completa y con sus ajustes,
 * como /.firmware-<familia> (Aura: /.firmware-aura, moonlit.aura:
 * /.firmware-moonlit). Cambiar de firmware son dos renombres mas
 * reiniciar. Las rutas viven aqui (y en la tabla de familias) porque
 * son del contrato (regla del CLAUDE.md). `i` es siempre un indice de
 * metro_fw_sibling(); fuera de rango ambas devuelven false.
 *
 * metro_firmware_sibling_installed(i): hay un arbol dormido de esa
 * familia que despertar -- su fila del submenu es inerte si no. */
bool metro_firmware_sibling_installed(int i);

/* Ejecuta el cambio al hermano i (contrato v10, en este orden y sin
 * nada en medio):
 *   1. guarda todo lo de Metro (aura.cfg, config.cfg, cola de tagcache)
 *      y fuerza el vaciado a disco -- despues del renombre /.rockbox es
 *      el arbol del HERMANO y cualquier escritura tardia caeria alli;
 *   2. /.rockbox -> /.firmware-metro (saliente primero: el peor corte
 *      deja un dormido entero; Studio repara al conectar);
 *   3. /.firmware-<hermano> -> /.rockbox;
 *   4. copia /.rockbox/rockbox.ipod (ya el del hermano) sobre
 *      /rockbox.ipod, el respaldo del bootloader, que debe ser siempre
 *      el del activo;
 *   5. deja /.aura/sync-pending.json con music=true SOLO si la
 *      biblioteca cambio desde el sello del arbol saliente (M-091);
 *   6. reinicia EN SECO (system_reboot) -- nunca por el apagado normal,
 *      que volveria a guardar los ajustes de Metro... en el otro arbol.
 * Solo vuelve si fallo antes de tocar nada, o si el paso 3 fallo y el
 * 2 se pudo deshacer (devuelve false; el firmware sigue siendo Metro).
 * Si ya existe /.firmware-metro (no deberia: Studio garantiza "nunca
 * dos de la misma familia") aborta sin tocar nada en vez de borrarlo. */
bool metro_firmware_switch_to(int i);

#endif /* METRO_SETTINGS_H */

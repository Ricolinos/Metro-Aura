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
#ifndef METRO_FIRMWARE_FAMILIES_H
#define METRO_FIRMWARE_FAMILIES_H

#include "metro_lang.h"

/* M-093: the table of SIBLING firmware families -- every family that
 * can sleep next to Metro under its own /.firmware-<family> tree
 * (contract v10). Pure data, no I/O: metro_settings.c asks the disk
 * whether a sibling is actually installed and performs the switch;
 * metro_screen_settings.c builds the "switch system" submenu from it,
 * one row per sibling. Metro's own dormant tree (METRO_FW_OWN_DORMANT)
 * is deliberately NOT in the table: you never switch to yourself. */

struct metro_fw_family
{
    const char *dormant_dir;   /* "/.firmware-<family>" */
    enum metro_lang_id name;   /* LANG_FAMILY_* -- visible name */
};

#define METRO_FW_OWN_DORMANT "/.firmware-metro"

/* Number of sibling families (currently 2: Aura, moonlit.aura). */
int metro_fw_sibling_count(void);

/* Sibling i in table order, NULL when i is out of range. */
const struct metro_fw_family *metro_fw_sibling(int i);

#endif /* METRO_FIRMWARE_FAMILIES_H */

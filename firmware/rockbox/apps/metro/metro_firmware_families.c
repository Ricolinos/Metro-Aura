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
#include <stddef.h>

#include "metro_firmware_families.h"

/* Order = order of the rows in Ajustes > General > "cambiar sistema".
 * New family: one line here plus its LANG_FAMILY_* string (appended at
 * the END of the catalogue, M-009). Nothing else has to change. */
static const struct metro_fw_family siblings[] = {
    { "/.firmware-aura",    LANG_FAMILY_AURA    },
    { "/.firmware-moonlit", LANG_FAMILY_MOONLIT },
};

#define SIBLING_COUNT (int)(sizeof(siblings) / sizeof(siblings[0]))

int metro_fw_sibling_count(void)
{
    return SIBLING_COUNT;
}

const struct metro_fw_family *metro_fw_sibling(int i)
{
    if (i < 0 || i >= SIBLING_COUNT)
        return NULL;
    return &siblings[i];
}

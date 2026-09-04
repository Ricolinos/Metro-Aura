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
#include "font.h"
#include "rbpaths.h"
#include "fs_defines.h" /* MAX_PATH */
#include "debug.h"

#include "metro_fonts.h"

/* Glyph budget passed to font_load_ex(): 0x20-0x17F is 352 codepoints
 * (gen_fonts.sh); 400 gives headroom without forcing the glyph-cache
 * path (see font_load_ex() in firmware/font.c -- glyphs>0 sizes the
 * buffer to fit exactly that many glyphs, so as long as it's >= the
 * font's real glyph count the whole file loads at once, uncached). */
#define METRO_FONT_GLYPH_BUDGET 400

/* M-113/M-114: las fuentes cirílicas miden 81 glifos reales exactos
 * (rango denso 1025-1105, alfabeto ruso completo, gen_fonts.sh); 96 da
 * el mismo tipo de margen que los 400 de arriba dan sobre 351-352. */
#define METRO_CYRILLIC_GLYPH_BUDGET 96

struct metro_font_spec {
    const char *filename; /* under FONT_DIR */
    const char *role_name; /* for DEBUGF only */
    const char *cyrillic_filename; /* M-113/M-114: NULL si el rol no tiene */
};

static const struct metro_font_spec font_specs[MFONT_COUNT] = {
    /* M-113: display midio 52,6% del presupuesto cirilico de los 5
     * roles en un solo archivo -- se dejo sin fuente propia a
     * proposito, ver metro_font_cyrillic_id() mas abajo (cae a
     * MFONT_TITLE, no a font_load_ex() fallando). */
    [MFONT_DISPLAY]  = { "metro-display-48.fnt",  "display",  NULL },
    [MFONT_TITLE]    = { "metro-title-28.fnt",    "title",
                          "metro-title-28-cyrillic.fnt" },
    [MFONT_LIST]     = { "metro-list-20.fnt",     "list",
                          "metro-list-20-cyrillic.fnt" },
    [MFONT_LIST_SEL] = { "metro-listsel-20.fnt",  "list_sel",
                          "metro-listsel-20-cyrillic.fnt" },
    [MFONT_CAPTION]  = { "metro-caption-14.fnt",  "caption",
                          "metro-caption-14-cyrillic.fnt" },
};

static int font_ids[MFONT_COUNT];
static int cyrillic_font_ids[MFONT_COUNT]; /* M-113/M-114 */

/* M-114 (porte de moonlit D-081, leido read-only): DEBUGF con el texto
 * literal "failed to load" en cualquier fallo de carga -- no es solo
 * diagnostico. En el simulador, DEBUGF SI imprime (a diferencia del
 * target real), y firmware/tools/sim_shot.sh ahora falla la captura
 * (grep de esa frase exacta sobre la salida del proceso) si aparece --
 * sin esto, una fuente que no entra en MAXUSERFONTS (font.h) cae en
 * silencio al id primario (metro_font_cyrillic_id() de mas abajo,
 * "nunca deja al llamador con un id invalido") y el hueco es invisible
 * sin medir a mano. Ver DECISIONS.md M-114. */
static int load_one(const char *filename, const char *role_name,
                    const char *what, int glyph_budget)
{
    char path[MAX_PATH];
    int id;

    snprintf(path, sizeof(path), "%s/%s", FONT_DIR, filename);
    id = font_load_ex(path, 0, glyph_budget);
    if (id < 0)
    {
        DEBUGF("metro_fonts: %s %s (%s) failed to load\n",
               role_name, what, path);
        return -1;
    }
    DEBUGF("metro_fonts: %s %s (%s) loaded as font id %d\n",
           role_name, what, path, id);
    return id;
}

void metro_fonts_init(void)
{
    int role;

    for (role = 0; role < MFONT_COUNT; role++)
    {
        int id = load_one(font_specs[role].filename, font_specs[role].role_name,
                          "primary", METRO_FONT_GLYPH_BUDGET);

        font_ids[role] = (id >= 0) ? id : FONT_SYSFIXED;

        /* M-113/M-114: sin fuente cirilica propia (MFONT_DISPLAY), o si
         * la carga falla, el tramo CYRILLIC cae al id primario del
         * MISMO rol por ahora -- el reencamine a MFONT_TITLE para
         * display pasa en metro_font_cyrillic_id(), abajo, despues de
         * que los 5 roles ya cargaron (display puede procesarse antes
         * que title en este bucle, segun el orden del enum). */
        cyrillic_font_ids[role] = font_ids[role];
        if (font_specs[role].cyrillic_filename)
        {
            int cyr_id = load_one(font_specs[role].cyrillic_filename,
                                  font_specs[role].role_name, "cyrillic",
                                  METRO_CYRILLIC_GLYPH_BUDGET);
            if (cyr_id >= 0)
                cyrillic_font_ids[role] = cyr_id;
        }
    }
}

int metro_font_id(enum metro_font_role role)
{
    if ((unsigned)role >= MFONT_COUNT)
        return FONT_SYSFIXED;
    return font_ids[role];
}

bool metro_font_has_cyrillic(enum metro_font_role role)
{
    if ((unsigned)role >= MFONT_COUNT)
        return false;
    /* M-113: display cae a title (mas abajo), pero SI cuenta como
     * "tiene cirilico" para metro_textseg_build() -- el atajo de un
     * solo tramo transliterado no debe aplicar a display, que dibuja
     * nombres de pivote cirilicos de punta a punta en ruso
     * ("настройки"). Este es justo el bug que D-081 de moonlit
     * encontro y documento para su propio MFONT_DISPLAY (con punct, no
     * cirilico, pero misma trampa): el atajo debe mirar CUALQUIER
     * fuente aparte que el rol tenga, no solo si este archivo .fnt en
     * particular existe. */
    return true;
}

int metro_font_cyrillic_id(enum metro_font_role role)
{
    if ((unsigned)role >= MFONT_COUNT)
        return FONT_SYSFIXED;
    if (role == MFONT_DISPLAY)
        return cyrillic_font_ids[MFONT_TITLE];
    return cyrillic_font_ids[role];
}

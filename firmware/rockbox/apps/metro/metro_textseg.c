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
/* M-114: ver metro_textseg.h. */
#include <stdint.h>
#include <string.h>

#include "metro_textseg.h"

/* Decodifica UNA secuencia UTF-8 en `s`. Devuelve cuántos bytes ocupa
 * (1..4) y deja el codepoint en *out_cp. Una secuencia mal formada
 * devuelve 1 byte y el codepoint 0 -- el llamador la copia tal cual,
 * no la sanea. */
static int decode_utf8(const char *s, uint32_t *out_cp)
{
    unsigned char b0 = (unsigned char)s[0];
    int n, i;
    uint32_t cp;

    if (b0 < 0x80)
    {
        *out_cp = b0;
        return 1;
    }
    if ((b0 & 0xE0) == 0xC0) { n = 2; cp = b0 & 0x1F; }
    else if ((b0 & 0xF0) == 0xE0) { n = 3; cp = b0 & 0x0F; }
    else if ((b0 & 0xF8) == 0xF0) { n = 4; cp = b0 & 0x07; }
    else { *out_cp = 0; return 1; }

    for (i = 1; i < n; i++)
    {
        unsigned char bi = (unsigned char)s[i];

        if ((bi & 0xC0) != 0x80)
        {
            *out_cp = 0;
            return 1; /* truncada o invalida: un byte, tal cual */
        }
        cp = (cp << 6) | (bi & 0x3F);
    }
    *out_cp = cp;
    return n;
}

int metro_textseg_build(const char *in, bool has_cyrillic_font,
                        char *out_buf, size_t out_buf_sz,
                        struct metro_textseg *segs, int max_segs)
{
    size_t o = 0, run_start = 0;
    int n = 0;
    enum metro_textseg_kind cur_kind = METRO_TEXTSEG_PRIMARY;
    bool have_run = false;

    if (!out_buf || out_buf_sz == 0 || max_segs <= 0)
        return 0;
    if (!in || !in[0])
        return 0;

    /* Sin fuente cirílica para este rol (hoy solo MFONT_DISPLAY,
     * M-113): un único tramo PRIMARY con la cadena entera -- el
     * comportamiento de antes de esta fase, sin excepciones. Nada que
     * clasificar; solo copiar, truncando en frontera de carácter UTF-8
     * si no cupiera entera (nunca a mitad de secuencia, que le
     * entregaría a lcd_putsxy() un byte guía suelto sin continuación). */
    if (!has_cyrillic_font)
    {
        size_t o2 = 0;
        const char *p = in;

        while (*p)
        {
            uint32_t cp;
            int len = decode_utf8(p, &cp);

            if (o2 + (size_t)len >= out_buf_sz)
                break;
            memcpy(out_buf + o2, p, (size_t)len);
            o2 += (size_t)len;
            p += len;
        }
        out_buf[o2] = '\0';
        segs[0].kind = METRO_TEXTSEG_PRIMARY;
        segs[0].text = out_buf;
        return 1;
    }

    while (*in)
    {
        uint32_t cp;
        int len = decode_utf8(in, &cp);
        enum metro_textseg_kind kind;

        if (cp >= METRO_TEXTSEG_CYRILLIC_START && cp <= METRO_TEXTSEG_CYRILLIC_LIMIT)
        {
            /* Sin transliterar -- no hay ASCII razonable para una
             * letra rusa. */
            kind = METRO_TEXTSEG_CYRILLIC;
        }
        else
        {
            /* Rango primario (32-383) o cualquier codepoint sin
             * fuente en absoluto: mismo tramo, bytes tal cual -- el
             * defaultchar del rol primario ('?') resuelve lo que
             * ninguna fuente cubre. */
            kind = METRO_TEXTSEG_PRIMARY;
        }

        if (kind != cur_kind && have_run)
        {
            if (o >= out_buf_sz || n >= max_segs)
                break; /* sin espacio para cerrar el tramo -- truncar aqui */
            out_buf[o++] = '\0';
            segs[n].kind = cur_kind;
            segs[n].text = out_buf + run_start;
            n++;
            have_run = false;
        }
        if (!have_run)
        {
            if (n >= max_segs)
                break; /* no habria donde guardar este tramo al cerrarlo */
            run_start = o;
            cur_kind = kind;
            have_run = true;
        }

        if (o + (size_t)len >= out_buf_sz)
            break; /* no cabe: lo ya escrito se cierra abajo */
        memcpy(out_buf + o, in, (size_t)len);
        o += (size_t)len;

        in += len;
    }

    if (have_run && o < out_buf_sz && n < max_segs)
    {
        out_buf[o++] = '\0';
        segs[n].kind = cur_kind;
        segs[n].text = out_buf + run_start;
        n++;
    }

    return n;
}

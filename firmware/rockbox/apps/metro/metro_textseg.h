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
/* M-114 (ronda "ajustes 2", Fase 5, plan maestro SS D.3): dibujo por
 * tramos. Selawik no trae ni un glifo cirílico (M-111) -- las cuatro
 * letras rusas de una cadena se dibujan con OTRO archivo .fnt (Inter,
 * M-113), así que un mismo `lcd_putsxy()` no puede pintar los dos: hay
 * que partir la cadena en tramos, uno por tipo de fuente, y dibujarlos
 * uno tras otro avanzando la x por el ancho medido de cada uno.
 *
 * Porte de moonlit_textseg.c (moonlit.aura D-074/D-081, leído
 * read-only como referencia -- ver DECISIONS.md M-114), RECORTADO a lo
 * que Metro necesita: Metro no tiene fuente de puntuación aparte (el
 * único hueco real, el carácter de puntos suspensivos U+2026, ya se
 * cerró en M-111 cambiando la cadena fuente a "..." en vez de agregar
 * una fuente), así que no hay clase PUNCT ni parámetro
 * `has_punct_font`, y tampoco hay transliteración (`moonlit_translit.c`
 * no se portó -- un codepoint sin fuente cae directo al `defaultchar`
 * del rol primario, '?', igual que siempre en Metro).
 *
 * Módulo puro (sin una sola dependencia de Rockbox) para que el arnés
 * de host lo enlace solo -- mismo patrón que metro_marquee_cycle.c.
 * Todo lo que se puede equivocar sin que se note vive aquí: qué
 * codepoint cae en qué tramo, que los tramos contiguos del mismo tipo
 * se funden en uno solo (menos llamadas a dibujar), y que nunca se
 * corte a mitad de una secuencia UTF-8 ni se desborde el buffer del
 * llamador. */
#ifndef METRO_TEXTSEG_H
#define METRO_TEXTSEG_H

#include <stdbool.h>
#include <stddef.h>

enum metro_textseg_kind {
    METRO_TEXTSEG_PRIMARY = 0, /* rol normal -- Selawik, rango 32-383 (M-010) */
    METRO_TEXTSEG_CYRILLIC,    /* fuente cirílica del rol (M-113/M-114, ruso) */
};

/* Un tramo, ya escrito y NUL-terminado dentro del buffer de salida del
 * llamador -- `text` apunta ahí, listo para pasar tal cual a
 * lcd_putsxy(). Válido solo mientras ese buffer no se reutilice. */
struct metro_textseg {
    enum metro_textseg_kind kind;
    const char *text;
};

/* Rango denso de la fuente cirílica (alfabeto ruso completo, ver
 * gen_fonts.sh/CYRILLIC_ROLES): Ё(0x401), А-я(0x410-0x44F), ё(0x451).
 * Público para que el test host pueda recorrerlo sin duplicar los
 * límites -- mismos valores que moonlit_textseg.h
 * (MOONLIT_TEXTSEG_CYRILLIC_START/_LIMIT), no una coincidencia: es el
 * mismo alfabeto ruso, medido igual en las dos familias (M-113/D-081).*/
#define METRO_TEXTSEG_CYRILLIC_START 1025
#define METRO_TEXTSEG_CYRILLIC_LIMIT 1105

/* Parte `in` en como mucho `max_segs` tramos dentro de `out_buf`
 * (`out_buf_sz` bytes). Devuelve cuántos escribió -- 0 si `in` es NULL
 * o vacío, o si no cupo ni un byte.
 *
 * `has_cyrillic_font` en false (el rol no tiene fuente cirílica propia
 * -- hoy solo MFONT_DISPLAY, M-113) hace que el resultado sea SIEMPRE
 * un único tramo PRIMARY con `in` entero, byte a byte -- el
 * comportamiento de antes de esta fase, sin excepciones. Con
 * `has_cyrillic_font` en true, cada codepoint se clasifica:
 *   - 1025-1105 (el rango de arriba): tramo CYRILLIC, bytes tal cual
 *     (sin transliterar -- no hay ASCII razonable para una letra rusa).
 *   - cualquier otro (incluido el rango primario 32-383, y cualquier
 *     codepoint sin fuente en absoluto): tramo PRIMARY, bytes tal
 *     cual -- el `defaultchar` del rol primario ('?') resuelve lo que
 *     ninguna fuente cubre.
 *
 * Tramos contiguos del mismo tipo se funden en uno: nunca hay dos
 * tramos PRIMARY seguidos en el resultado. Si `in` tiene más
 * alternancias de las que caben en `max_segs`, o más bytes de los que
 * caben en `out_buf_sz`, el resto se descarta -- trunca sin desbordar
 * ninguno de los dos. */
int metro_textseg_build(const char *in, bool has_cyrillic_font,
                        char *out_buf, size_t out_buf_sz,
                        struct metro_textseg *segs, int max_segs);

#endif /* METRO_TEXTSEG_H */

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
/* M-106: el reloj de la marquesina, aparte del dibujo.
 *
 * Modulo puro, sin una sola dependencia de Rockbox, para que
 * apps/metro/test/test_marquee.c lo enlace solo -- mismo patron que
 * metro_master_art_format.c respecto de la cache maestra (M-097). Aqui
 * esta todo lo que se puede equivocar sin que se note a simple vista:
 * que el tramo quieto sea de verdad quieto, que el barrido llegue
 * EXACTAMENTE a `span_px` (si se queda corto o se pasa, el bucle tiene
 * costura), y que un tiempo grande no desborde.
 *
 * PORTADO de moonlit.aura (D-067, solo lectura) sin cambios de logica:
 * es deliberado que las dos familias compartan byte a byte la parte
 * que un test de host puede fijar. */

int metro_marquee_offset_px(long elapsed_ms, int span_px,
                              int static_ms, int scroll_ms)
{
    long cycle, t;

    if (span_px <= 0 || scroll_ms <= 0 || static_ms < 0)
        return 0;

    cycle = (long)static_ms + scroll_ms;
    t = elapsed_ms % cycle;
    if (t < 0)
        t += cycle; /* defensivo: un reloj que retrocede no invierte el barrido */
    if (t < static_ms)
        return 0;

    t -= static_ms;
    /* Lineal a proposito (plan maestro SS G): una marquesina con easing se
     * lee peor, porque el ojo sigue el texto y no el movimiento. */
    return (int)(((long)span_px * t) / scroll_ms);
}


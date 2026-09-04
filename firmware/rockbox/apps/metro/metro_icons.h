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
/* R4/FA-1 (M-077): iconografía de Fluent System Icons (Microsoft, MIT)
 * como tabla de mapas de bits monocromos, generada offline por
 * firmware/tools/gen_icons.py y COMMITEADA -- mismo patrón que
 * metro_turnstile_table.c.
 *
 * Por qué no de otra forma, en corto (el largo está en el generador):
 * una FUENTE de iconos no cabe en el pipeline de fuentes de Metro (su
 * rango es 0x20-0x17F, sin zona de uso privado), y un `.bmp` por icono
 * obligaría a leer disco para dibujarlos, cosa que `CLAUDE.md` prohíbe
 * dentro de un bucle de animación -- y los iconos de modo se dibujan en
 * cada cuadro de Now Playing.
 *
 * **Monocromo a propósito**: el color lo elige quien dibuja. Es lo que
 * permite respetar la regla de cero RGB fuera de `metro_palette.h` y,
 * de paso, que el mismo glifo sirva en acento, secundario o fg según el
 * estado que represente.
 *
 * El orden del enum es el mismo que la lista ICONS del generador. Si se
 * agrega un icono, va AL FINAL de ambos.
 */
#ifndef METRO_ICONS_H
#define METRO_ICONS_H

/* Coincide con METRO_WIDGETS_ICON_SIZE (metro_widgets.h) -- los glifos
 * de Fluent que se usan son las variantes de 16px, dibujadas para ese
 * tamaño, no un 24px reducido. */
#define METRO_ICON_SIZE 16

enum metro_icon_id {
    METRO_ICON_PLAY = 0,
    METRO_ICON_PAUSE,
    METRO_ICON_SHUFFLE,
    METRO_ICON_REPEAT_ALL,
    METRO_ICON_SPEAKER,
    /* R5-F3 (M-083) */
    METRO_ICON_STAR,
    METRO_ICON_PREVIOUS,
    METRO_ICON_NEXT,
    /* R5 (M-088): pantalla USB */
    METRO_ICON_SYNC,
    /* M-104: candado de la barra de estado, cuando el interruptor Hold
     * esta puesto. Fluent "lock closed" 16 filled. */
    METRO_ICON_LOCK,
    METRO_ICON_COUNT
};

/* Una máscara por fila, bit 15 = píxel de la izquierda. 32 bytes por
 * icono. */
struct metro_icon {
    unsigned short rows[METRO_ICON_SIZE];
};

extern const struct metro_icon metro_icons[METRO_ICON_COUNT];

#endif /* METRO_ICONS_H */

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
/* M-106: marquesina de texto largo. Especificacion del plan maestro
 * SS G (referencia: aura_patterns.c:32-49 + aura_marquee.c:29-70, leidos
 * de ../Aura-Firmware, solo lectura): si el texto cabe, estatico; si no,
 * METRO_MARQUEE_STATIC_MS quieto, luego METRO_MARQUEE_SCROLL_MS
 * desplazando de derecha a izquierda de forma LINEAL, con
 * METRO_MARQUEE_LOOP_GAP_PX de hueco entre copias, en bucle. Se dibujan
 * DOS copias del texto para que el bucle no tenga costura: cuando la
 * primera termina de salir, la segunda esta exactamente donde arranco
 * la primera.
 *
 * PORTADO de moonlit.aura (D-067, commit f38f723b, leido en solo
 * lectura), no reescrito: el reloj del ciclo es el mismo modulo puro,
 * con las mismas constantes, para que las dos familias compartan el
 * codigo que se puede equivocar sin que se note. Lo unico que cambia
 * son los nombres y el conjunto de ranuras (moonlit tiene Marea y una
 * lista de "Acerca de" propia; Metro dibuja "Acerca de" por el camino
 * generico de filas).
 *
 * Puerta de energia (regla del CLAUDE.md): solo pide cuadros mientras
 * HAY un texto visible que desborda, `lcd_active()` es cierto y
 * `metro_settings.animations != METRO_ANIM_OFF`. Con las animaciones
 * apagadas se corta a la derecha como siempre -- ni un tick de mas.
 *
 * El calculo del desplazamiento es PURO y esta probado en host
 * (apps/metro/test/test_marquee.c); lo unico que toca el LCD es
 * metro_marquee_draw(). */
#ifndef METRO_MARQUEE_H
#define METRO_MARQUEE_H

#include <stdbool.h>

#include "metro_fonts.h"

/* Constantes del maestro SS G, identicas en las tres familias. */
#define METRO_MARQUEE_STATIC_MS   2000
#define METRO_MARQUEE_SCROLL_MS   5000
#define METRO_MARQUEE_LOOP_GAP_PX 24

/* Un sitio de la UI con marquesina. El estado (cuando arranco el ciclo,
 * que texto era) vive en este modulo indexado por ranura, para que las
 * pantallas no tengan que arrastrarlo. Una ranura por SITIO, no por
 * fila: solo el texto CON FOCO desplaza, y solo hay uno por sitio. */
enum metro_marquee_slot {
    METRO_MARQUEE_ROW = 0,   /* titulo de la fila seleccionada de una lista */
    METRO_MARQUEE_TILE,      /* rotulo del tile seleccionado */
    METRO_MARQUEE_NP_TITLE,
    METRO_MARQUEE_NP_ARTIST,
    METRO_MARQUEE_NP_ALBUM,
    METRO_MARQUEE_COUNT
};

/* Desplazamiento en pixeles dentro del ciclo. PURA. `span_px` es el
 * ancho de un ciclo completo (ancho del texto + hueco): al final del
 * tramo de scroll el desplazamiento vale exactamente `span_px`, que es
 * la misma imagen que 0 gracias a la segunda copia. Un `span_px` <= 0
 * o duraciones no positivas devuelven 0 (nada que desplazar). */
int metro_marquee_offset_px(long elapsed_ms, int span_px,
                            int static_ms, int scroll_ms);

/* Dibuja `text` en la banda [clip_x, clip_x+clip_w) a la altura `y`.
 * Si cabe, o si las animaciones estan apagadas o el LCD dormido, cae en
 * metro_draw_text_cut_right() -- exactamente el comportamiento de
 * antes. Si no cabe y puede animar, desplaza.
 *
 * Devuelve true si esta desplazando (el llamador debe seguir pidiendo
 * cuadros). El reloj de cada ranura se reinicia solo cuando cambia el
 * texto: pasar de una fila a otra empieza de nuevo por el tramo quieto,
 * que es lo que se espera al mover la seleccion. */
bool metro_marquee_draw(enum metro_marquee_slot slot,
                        enum metro_font_role role, int clip_x, int clip_w,
                        int y, const char *text, unsigned color);

/* true si ALGUNA ranura quedo desplazando en el ultimo dibujo -- la
 * puerta que metro_main.c consulta para bajar su espera a HZ/20 y
 * repintar. Se apaga sola: cada ranura marca su estado en cada
 * metro_marquee_draw(), y una pantalla que deja de dibujar una ranura
 * la deja marcada como quieta via metro_marquee_reset(). */
bool metro_marquee_wants_ticks(void);

/* Olvida el estado de todas las ranuras (cambio de pantalla, de tema o
 * de idioma). Sin esto, entrar a otra lista heredaria el reloj a mitad
 * de ciclo de la anterior. */
void metro_marquee_reset(void);

#endif /* METRO_MARQUEE_H */

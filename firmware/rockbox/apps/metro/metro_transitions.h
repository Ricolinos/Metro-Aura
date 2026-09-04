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
/* F11: the transition engine (PLAN_MAESTRO.md S3.2/S3.3) -- metro_main.c
 * is the only caller. It diffs metro_nav_t state before/after an
 * action to decide WHICH of these to call (depth up = push, depth
 * down = pop, pivot changed = twist, entering/leaving Now Playing =
 * fade); this module doesn't know about pages, pivots or Now Playing
 * at all, only "slide toward a signed direction" and "fade to a new
 * frame", each given a draw_to() callback that renders the destination
 * screen (one of Metro's normal *_show() functions). */
#ifndef METRO_TRANSITIONS_H
#define METRO_TRANSITIONS_H

#include <stdbool.h>
#include "metro_fb.h"

typedef metro_fb_draw_fn metro_transitions_draw_fn;

/* SLIDE (pivot twist, PLAN_MAESTRO.md S3.3): direction > 0 -- draw_to
 * enters from the right (pivot-next). direction < 0: draw_to enters
 * from the left (pivot-prev). Falls straight through to draw_to()
 * with no animation at all when animations=off, !lcd_active(), or
 * after auto-degradation (M-015) has dropped the effective level to
 * off. Never uses turnstile -- that substitution is specific to
 * PUSH/POP (metro_transitions_push() below), the plan's own catalog
 * table keeps SLIDE and PUSH/POP as separate rows. */
void metro_transitions_slide(metro_transitions_draw_fn draw_to, int direction);

/* M-106: el mismo deslizamiento, pero RAPIDO -- la mitad de cuadros.
 *
 * Existe por el visor de fotos, donde el plan maestro fija un tope de
 * 150 ms y el deslizamiento normal dura 240 ms bajo `animations=all`
 * (8 cuadros x 3 ticks). No es un capricho de cifra: en el visor cada
 * cambio de foto ya paga un decode JPEG completo antes de poder
 * animar, asi que la animacion es lo unico que se puede acortar sin
 * perder nada -- y una pantalla ENTERA de foto deslizandose lee mucho
 * mas rapido que una lista de texto, donde el ojo sigue palabras.
 * 4 cuadros bajo `all` (120 ms) y 2 bajo `minimal` (60 ms); con las
 * animaciones apagadas, igual que el normal, no anima en absoluto. */
void metro_transitions_slide_fast(metro_transitions_draw_fn draw_to, int direction);

/* PUSH/POP (deepening into a page / going back, PLAN_MAESTRO.md S3.3):
 * same direction convention as metro_transitions_slide() (> 0 push,
 * < 0 pop). Under animations=all AND graphics=full, substitutes
 * metro_transitions_slide()'s plain slide for a turnstile rotation
 * (F12, present_turnstile) -- any other level/graphics combination
 * falls back to the exact same slide metro_transitions_slide() uses. */
void metro_transitions_push(metro_transitions_draw_fn draw_to, int direction);

/* F12: true when the EFFECTIVE animations level (metro_settings.animations
 * minus M-015's session auto-degradation, same value
 * metro_transitions_slide()/_push()/_fade() already act on) is ALL --
 * metro_screen_list.c's own FEATHER cascade has no framebuffer work
 * of its own to gate through this module, but still needs to respect
 * the same "off"/"minimal" -> no animation rule and the same
 * degradation state instead of re-reading metro_settings.animations
 * directly and duplicating the degradation logic. */
bool metro_transitions_effective_all(void);

/* FADE: entering/leaving Now Playing, returning from a plugin
 * (PLAN_MAESTRO.md S3.3). Only actually cross-fades (metro_fb.c's
 * present_fade, per-pixel blend) when animations=all AND
 * graphics=full; animations=minimal or graphics=lite both fall back
 * to metro_transitions_slide(draw_to, 1) -- present_fade is reserved
 * to graphics=full (metro_fb.h), and `minimal` "usa SLIDE" for every
 * transition per the plan's own table, this one included. */
void metro_transitions_fade(metro_transitions_draw_fn draw_to);

/* R3-F8/DD-9 (M-069): CONTINUUM -- arma el "texto volador" para el
 * PRÓXIMO metro_transitions_push(), y solo para ese (se consume ahí,
 * arme o no arme la animación). `text` es el título de la fila que el
 * usuario acaba de elegir y `from_y` la y en pantalla donde esa fila
 * estaba dibujada; el destino es fijo (la ceja de la página nueva, ver
 * metro_transitions.c). El llamador (metro_screen_list.c) es quien
 * decide SI hay continuidad real que mostrar -- esta función no
 * compara nada, solo guarda. `text` se copia; no hace falta que
 * sobreviva a la llamada. */
#define METRO_CONTINUUM_TITLE_MAX 64
void metro_transitions_arm_continuum(const char *text, int from_y);

#endif /* METRO_TRANSITIONS_H */

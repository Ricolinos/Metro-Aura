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
/* R2-F3 (PLAN-metro-r2-maestro.md DD-10): Metro's own photo viewer,
 * replacing imageviewer.rock (which stays in the tree, untouched --
 * apps/plugins/imageviewer.make still builds it -- Metro just stops
 * launching it from Fotos). Same sentinel-page pattern as Now Playing
 * (metro_screen_nowplaying.h) -- a real, otherwise-empty metro_page is
 * pushed onto the nav stack so depth/pop bookkeeping (metro_nav.c)
 * stays generic; metro_main.c routes drawing/input to this module
 * instead of the generic list screen while it's on top.
 *
 * DESVIACIONES.md R2-1: what this gives up versus imageviewer.rock
 * (zoom, pan, slideshow, PNG/GIF/BMP support) -- same trade Aura-Firmware
 * already made for its own photo viewer (C.3), for the same reason:
 * Metro-styled fit/cover beats a correct-but-foreign-looking plugin
 * screen with a completely different, undiscoverable button scheme
 * (BUTTON_SELECT to quit on this keypad, not MENU -- see DECISIONS.md
 * M-058 for the exact bug this replaces). */
#ifndef METRO_SCREEN_PHOTO_VIEWER_H
#define METRO_SCREEN_PHOTO_VIEWER_H

#include <stdbool.h>
#include "metro_photos.h"

/* Pushes the viewer sentinel, starting on items[start_index]. LEFT/RIGHT
 * (wheel) browse the SAME array from then on -- `items`/`count` must
 * stay valid and unchanged for as long as the viewer stays on the nav
 * stack (same lifetime rule metro_screen_hub.c's photo_pivot_ctx
 * already honors for its own get_row()/on_select() -- this points
 * straight at that same static array, no copy). Returns false without
 * pushing anything if the nav stack is already full or `count` is 0. */
bool metro_screen_photo_viewer_push(const metro_photo_item_t *items, int count,
                                     int start_index);

/* True while the viewer sentinel is the current page -- metro_main.c
 * uses this to route drawing/input here instead of the generic list
 * screen, exactly like metro_screen_nowplaying_is_current(). */
bool metro_screen_photo_viewer_is_current(void);

void metro_screen_photo_viewer_show(void);

/* M-106: direccion del deslizamiento que dejo pendiente el ultimo
 * cambio de foto (+1 siguiente, -1 anterior, 0 ninguno) -- se CONSUME
 * al leerla.
 *
 * La transicion no se dispara desde aqui a proposito: metro_main.c es
 * el unico que elige transiciones, diffeando el nav antes y despues de
 * la accion (F11/M-070), y un cambio de foto no mueve ni profundidad ni
 * pivot, asi que no hay nada que diffear. Dejarla anunciada es el mismo
 * mecanismo que ya usan CONTINUUM (metro_transitions_arm_continuum())
 * y FEATHER: la pantalla dice QUE paso, el bucle decide COMO se ve. */
int metro_screen_photo_viewer_take_slide(void);
void metro_screen_photo_viewer_handle(int action, int steps);

#endif /* METRO_SCREEN_PHOTO_VIEWER_H */

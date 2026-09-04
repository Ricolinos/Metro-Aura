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
#ifndef METRO_SCREEN_ADJUST_H
#define METRO_SCREEN_ADJUST_H

#include "metro_lang.h"

/* M-103 (matriz de Ajustes homologada, plan maestro SS C): pantalla
 * modal de UN valor con barra.
 *
 * Por que existe: brillo y retroiluminacion se ciclaban con SELECT
 * sobre cuatro y seis valores fijos. Con cuatro pasos de brillo el
 * usuario no puede afinar, y con seis de retroiluminacion llegar al
 * que quiere puede costar cinco pulsaciones sin ver nunca el rango
 * completo. Aura ya resuelve esto con un deslizador; Metro no tiene
 * gesto de arrastre en la rueda, asi que la forma equivalente es una
 * pantalla propia donde la rueda ES el control: cada paso se aplica en
 * vivo y MENU vuelve.
 *
 * La pantalla no sabe QUE esta ajustando. El llamador le pasa cuantos
 * pasos hay, en cual empieza, como se rotula cada uno y que hacer
 * cuando cambia -- asi el mismo codigo sirve para brillo (10 pasos
 * lineales) y para retroiluminacion (6 valores no lineales, incluido
 * "nunca"), que es lo que hace que las dos filas se sientan iguales. */

struct metro_adjust_spec {
    enum metro_lang_id title;   /* rotulo grande de la pantalla */
    int   steps;                /* cuantos pasos tiene el control (>= 2) */

    /* Etiqueta del paso `step` (0..steps-1). Puede devolver un buffer
     * estatico propio: se dibuja de inmediato. */
    const char *(*label)(void *ctx, int step);

    /* Se llama en CADA cambio de paso, no al salir: el valor se aplica
     * en vivo para que el usuario vea (o escuche) lo que esta
     * eligiendo. Guardar a disco es cosa del llamador, al volver. */
    void (*apply)(void *ctx, int step);

    void *ctx;
};

/* Corre la pantalla hasta que el usuario sale con MENU. Devuelve el
 * paso final (ya aplicado). `start` se acota al rango. */
int metro_screen_adjust_run(const struct metro_adjust_spec *spec, int start);

#endif /* METRO_SCREEN_ADJUST_H */

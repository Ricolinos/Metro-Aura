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
#ifndef METRO_SCREEN_TEXT_H
#define METRO_SCREEN_TEXT_H

/* M-103: pantalla modal de TEXTO desplazable -- parrafos con ajuste de
 * linea, la rueda desplaza, MENU vuelve.
 *
 * Existe por "avisos legales" (GPL v2 SS3 exige que el aviso de licencia
 * este a la vista del usuario, no solo en el repositorio), y es la
 * unica pantalla de Metro que muestra texto corrido: la lista generica
 * (metro_screen_list.c) dibuja FILAS de una linea con recorte a la
 * derecha, que para un parrafo de licencia significa perder el texto.
 *
 * El texto se pasa entero, con '\n' donde el autor quiera un salto
 * duro; el resto se ajusta al ancho util cortando en espacios. No lee
 * disco: la cadena viene del catalogo bilingue (metro_lang.c), como
 * todo lo demas de la UI. */
void metro_screen_text_show(const char *title, const char *body);

#endif /* METRO_SCREEN_TEXT_H */

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
/* M-110 (ronda "ajustes 2", plan maestro SS A, contrato v19): formato y
 * parseo de /.aura/settings.cfg -- los ajustes que Aura, Metro y
 * moonlit.aura leen y escriben por igual (brillo, retroiluminacion,
 * apagado, clicker, limite de volumen, replaygain, idioma, tema claro/
 * oscuro y el candado). Modulo PURO a proposito (host-tested en
 * test/test_shared_settings.c, sin ningun header de Rockbox): NO abre
 * ni escribe el archivo -- eso es metro_settings.c, el unico que tiene
 * permiso de construir rutas bajo /.aura/ (regla del CLAUDE.md) y el
 * unico que conoce los rangos REALES de brillo/retroiluminacion, que
 * son constantes del target (MIN/MAX_BRIGHTNESS_SETTING) que este
 * modulo no puede ver.
 *
 * División del trabajo (A.2 del maestro):
 *   - metro_shared_settings_parse_field(): UNA línea ya partida en
 *     nombre/valor (el llamador la partió, con settings_parseline() en
 *     el target o a mano en el test) actualiza el campo reconocido.
 *     Válida lo que se puede validar sin el target (un entero
 *     razonable, uno de los valores fijos del contrato); todo lo que
 *     depende del target (si 999 es un brillo válido en ESTE panel)
 *     se revalida otra vez en metro_settings.c al aplicar.
 *   - metro_shared_settings_format_field(): la clave `index` (0..12,
 *     orden de la tabla A.1) como línea de texto lista para escribir.
 *   - El resto (leer/escribir el archivo, aplicar al estado vivo,
 *     capturar el estado vivo para reescribir) vive en metro_settings.c. */
#ifndef METRO_SHARED_SETTINGS_H
#define METRO_SHARED_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

#include "metro_settings.h" /* enum metro_lock_require -- mismos nombres */
#include "metro_theme.h"    /* enum metro_theme_kind -- dark/light = appearance */

#define METRO_SHARED_SETTINGS_HEADER "# aura-shared-settings v1"
#define METRO_SHARED_SETTINGS_KEY_COUNT 13
#define METRO_SHARED_LANG_CODE_LEN 3 /* 2 letras + NUL */

/* Sanidad NO especifica del target -- solo para descartar un valor
 * absurdo (el "brightness: 999" del vector A.3) en la capa pura. El
 * rango real (1..MAX_BRIGHTNESS_SETTING del panel) se revisa otra vez
 * en metro_settings.c, que sí conoce el target. */
#define METRO_SHARED_BRIGHTNESS_SANE_MAX 255
#define METRO_SHARED_BACKLIGHT_SANE_MAX  (24 * 3600) /* 24h en segundos */
#define METRO_SHARED_POWEROFF_SANE_MAX   (24 * 60)   /* 24h en minutos */
#define METRO_SHARED_VOLUME_LIMIT_SANE_MIN -200
#define METRO_SHARED_VOLUME_LIMIT_SANE_MAX  200

enum metro_shared_updated_by {
    METRO_SHARED_BY_AURA = 0,
    METRO_SHARED_BY_METRO,
    METRO_SHARED_BY_MOONLIT,
    METRO_SHARED_BY_UNKNOWN
};

enum metro_shared_replaygain {
    METRO_SHARED_RG_OFF = 0,
    METRO_SHARED_RG_TRACK,
    METRO_SHARED_RG_ALBUM
};

typedef struct {
    int rev;
    enum metro_shared_updated_by updated_by;

    bool screen_lock_enabled;
    char screen_lock_pin[5]; /* 4 digitos ASCII + NUL, o vacio */
    enum metro_lock_require screen_lock_require;

    /* Enteros sin validar contra el target -- ver la nota de arriba de
     * METRO_SHARED_BRIGHTNESS_SANE_MAX. */
    int brightness;
    int backlight_timeout; /* segundos; -1 = nunca */
    int idle_poweroff;     /* minutos; 0 = nunca */
    bool keyclick;
    int volume_limit; /* dB, nativo de Rockbox */

    enum metro_shared_replaygain replaygain;
    char language[METRO_SHARED_LANG_CODE_LEN]; /* "es"/"en"/"fr"/"de"/"ru"/"it" */
    enum metro_theme_kind appearance; /* dark/light */
} metro_shared_settings_t;

/* Deja SOLO los campos que este modulo puede fijar sin depender del
 * target en un valor razonable (rev=0 -- la primera escritura real
 * queda en 1 -- updated_by=metro, sin candado, replaygain apagado,
 * "es", oscuro). brightness/backlight_timeout/idle_poweroff/
 * volume_limit quedan en 0 A PROPOSITO: sus defaults de verdad son
 * constantes del target que este modulo no conoce -- metro_settings.c
 * nunca lee estos ceros, siempre captura el estado vivo (ya puesto en
 * su default real) antes de escribir. */
void metro_shared_settings_defaults(metro_shared_settings_t *s);

/* true si `line` es exactamente la cabecera obligatoria (A.1) -- el
 * llamador debe rechazar el archivo ENTERO si la primera linea no
 * pasa esto (A.2.5), antes de parsear ninguna otra. */
bool metro_shared_settings_is_header(const char *line);

/* `name`/`value` ya vienen partidos (settings_parseline() en el
 * target). Si `name` es una de las 13 claves de A.1, actualiza el
 * campo correspondiente EN *s cuando `value` es valido para esa clave
 * y devuelve true (reconocida) -- incluso si el valor no era valido y
 * se ignoro (A.2.2: "una clave a la vez", el resto del archivo sigue
 * vivo). Devuelve false si `name` no es ninguna de las 13: el llamador
 * debe guardar esa linea cruda para reescribirla tal cual (A.2.2,
 * `clave_futura` del vector A.3). */
bool metro_shared_settings_parse_field(metro_shared_settings_t *s,
                                        const char *name, const char *value);

/* Formatea la clave `index` (0..METRO_SHARED_SETTINGS_KEY_COUNT-1,
 * mismo orden que la tabla A.1) como "clave: valor\n" en `buf`.
 * Devuelve la cantidad de bytes escritos (0 si no cupo, o si esta
 * clave no debe escribirse ahora -- las dos del candado cuando
 * screen_lock_enabled es falso, mismo criterio que metro_settings_save()
 * ya usa para aura.cfg). El llamador itera 0..COUNT-1 y hace fdprintf()
 * de cada resultado no vacio. */
size_t metro_shared_settings_format_field(const metro_shared_settings_t *s,
                                           int index, char *buf, size_t bufsz);

#endif /* METRO_SHARED_SETTINGS_H */

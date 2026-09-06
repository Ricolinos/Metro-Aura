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
#ifndef METRO_LANG_H
#define METRO_LANG_H

#include <stdbool.h>
#include <stddef.h>

/* Own string table, no Rockbox .lang system -- same mechanism as
 * Aura-Firmware's aura_lang.c (D-013, INVESTIGACION.md A.9). Spanish
 * by default (DECISIONS.md M-009); append-only as new screens land. */

/* M-111 (ronda "ajustes 2", Fase 3, plan maestro SS D): fr/de/ru/it se
 * suman a es/en. Orden fijo, nunca alfabetico -- es el orden en que el
 * selector de Ajustes > idioma los ofrece (§D.2 del maestro), y el
 * mismo orden en las tres familias por convencion del contrato. */
enum metro_language {
    METRO_LANG_ES = 0,
    METRO_LANG_EN,
    METRO_LANG_FR,
    METRO_LANG_DE,
    METRO_LANG_RU,
    METRO_LANG_IT,
    METRO_LANG_COUNT
};

enum metro_lang_id {
    LANG_HUB_MUSIC = 0,
    LANG_HUB_VIDEOS,
    LANG_HUB_PHOTOS,
    LANG_HUB_SETTINGS,

    LANG_PIVOT_ARTISTS,
    LANG_PIVOT_ALBUMS,
    LANG_PIVOT_SONGS,
    LANG_PIVOT_GENRES,
    LANG_PIVOT_PLAYLISTS,

    LANG_PIVOT_ALL,
    LANG_PIVOT_MOVIES,
    LANG_PIVOT_SERIES,
    LANG_PIVOT_CLIPS,

    LANG_PIVOT_PHOTOS,
    LANG_PIVOT_IMAGES,
    LANG_PIVOT_AI,

    LANG_PIVOT_GENERAL,
    LANG_PIVOT_DISPLAY,
    LANG_PIVOT_ABOUT,

    LANG_SETTING_LANGUAGE,
    LANG_SETTING_THEME,
    LANG_SETTING_ACCENT,
    LANG_SETTING_RESET,
    LANG_VALUE_DARK,
    LANG_VALUE_LIGHT,

    LANG_ACCENT_BLUE,
    LANG_ACCENT_BROWN,
    LANG_ACCENT_GREEN,
    LANG_ACCENT_LIME,
    LANG_ACCENT_MAGENTA,
    LANG_ACCENT_MANGO,
    LANG_ACCENT_PINK,
    LANG_ACCENT_PURPLE,
    LANG_ACCENT_RED,
    LANG_ACCENT_TEAL,

    LANG_ABOUT_BASED_ON_ROCKBOX,

    LANG_DIALOG_RESET_TITLE,
    LANG_DIALOG_YES,
    LANG_DIALOG_NO,

    LANG_MUSIC_DB_UPDATING,
    LANG_HUB_NOWPLAYING,
    LANG_UNKNOWN_ARTIST,
    LANG_UNKNOWN_ALBUM,
    LANG_UNKNOWN_GENRE,
    LANG_UNKNOWN_TITLE,

    LANG_NP_OPTIONS_TITLE,
    LANG_NP_SHUFFLE,
    LANG_NP_REPEAT,
    LANG_VALUE_ON,
    LANG_VALUE_OFF,
    LANG_REPEAT_ALL,
    LANG_REPEAT_ONE,

    LANG_SYNC_ERROR_VERSION,
    LANG_SYNC_ERROR_ATTEMPTS,
    LANG_SYNC_DISMISS_HINT,
    /* M-100: image phase of the preparation (shared master cache
     * /.aura/art). Photos carries no total: its walk is streaming. */
    LANG_SYNC_ART_ALBUMS,       /* "preparando carátulas %d/%d" */
    LANG_SYNC_ART_ARTISTS,      /* "preparando fotos de artistas %d/%d" */
    LANG_SYNC_ART_PHOTOS,       /* "preparando imágenes %d" */
    /* M-123: la fase de base de datos, que hasta ahora no se nombraba
     * -- la pantalla solo decía "actualizando biblioteca" y el usuario
     * no sabía en qué iba. La variante _BUSY existe porque tagcache no
     * siempre publica un total: mientras recorre el disco por primera
     * vez `total_entries` es 0 y no hay porcentaje que enseñar. */
    LANG_SYNC_DB_SCAN,          /* "leyendo la música %d" -- carpetas ya vistas */
    LANG_SYNC_DB_INDEX,         /* "indexando %d/%d" -- paso de commit */
    LANG_SYNC_DB_BUSY,          /* "leyendo la música" -- todavía sin cifra */
    LANG_SYNC_POSTPONE_HINT,    /* "menú para seguir en segundo plano" */

    LANG_SETTING_LIBRARY,
    LANG_SETTING_BRIGHTNESS,
    LANG_SETTING_BACKLIGHT,
    LANG_VALUE_NEVER,
    LANG_DIALOG_LIBRARY_TITLE,
    LANG_DIALOG_LIBRARY_DETAIL, /* M-100: advertencia de duración */
    LANG_LIBRARY_UPDATING,

    LANG_ABOUT_DEVICE_DEFAULT,
    LANG_ABOUT_SONGS,
    LANG_ABOUT_NOT_SYNCED,
    LANG_ABOUT_PLAYLISTS,
    LANG_ABOUT_MOVIES,
    LANG_ABOUT_SERIES,
    LANG_ABOUT_CLIPS,
    LANG_ABOUT_IMAGES,
    LANG_ABOUT_PHOTOS_TAKEN,
    LANG_ABOUT_AI,

    LANG_USB_CONNECTED,
    LANG_SHUTTING_DOWN,

    LANG_EMPTY_LIST,

    LANG_SETTING_ANIMATIONS,
    LANG_SETTING_GRAPHICS,
    LANG_ANIM_ALL,
    LANG_ANIM_MINIMAL,
    LANG_ANIM_OFF,
    LANG_GFX_FULL,
    LANG_GFX_LITE,

    LANG_PHOTO_LOADING,
    LANG_PHOTO_UNSUPPORTED,

    LANG_NP_LYRICS,
    LANG_VALUE_UNAVAILABLE,

    LANG_PIVOT_QUICKPLAY,
    LANG_QUICKPLAY_EMPTY,

    LANG_NP_RATING,

    LANG_SETTING_LOCK,
    LANG_LOCK_TITLE_LOCKED,
    LANG_LOCK_TITLE_SET,
    LANG_LOCK_TITLE_CONFIRM,
    LANG_LOCK_HINT_UNLOCK,
    LANG_LOCK_HINT_WRONG,
    LANG_LOCK_HINT_SET,
    LANG_LOCK_HINT_CONFIRM,
    LANG_LOCK_HINT_MISMATCH,
    LANG_DIALOG_LOCK_OFF_TITLE,

    LANG_SETTING_SLEEP,
    LANG_SETTING_EQ,
    LANG_EQ_FLAT,
    LANG_EQ_BASS,
    LANG_EQ_VOCAL,
    LANG_EQ_BRIGHT,

    /* R5-F3 (M-083) */
    LANG_SETTING_VOLUME_LIMIT,

    /* R5 (M-087): fila final cuando una lista llego a su tope */
    LANG_LIST_TRUNCATED,

    /* R5 (M-090): cambio de firmware */
    LANG_VALUE_NOT_INSTALLED,

    /* M-093: submenu "cambiar sistema", una fila por familia hermana
     * (metro_firmware_families.c). LANG_FAMILY_* son los nombres
     * visibles; el dialogo lleva %s para el nombre. */
    LANG_SETTING_SWITCH_SYSTEM,
    LANG_FAMILY_AURA,
    LANG_FAMILY_METRO,
    LANG_FAMILY_MOONLIT,
    LANG_DIALOG_SWITCH_FMT,

    /* M-101: fila oculta de diagnostico en "acerca de" -- marca de agua
     * de la pila del hilo principal (plan maestro de la ronda,
     * seccion E.4). Se revela con SELECT sostenido sobre la fila de
     * version; en el simulador esta siempre visible. */
    LANG_ABOUT_STACK,
    LANG_ABOUT_STACK_NA,

    /* M-103 (ronda homologacion, matriz de Ajustes del plan maestro
     * SS C): filas homologadas con Aura y moonlit. */
    LANG_SETTING_POWEROFF,      /* apagado automatico */
    LANG_SETTING_KEYCLICK,      /* clicker */
    LANG_SETTING_LEGAL,         /* avisos legales */
    LANG_SETTING_REPLAYGAIN,    /* ajuste de volumen (replaygain) */
    LANG_VALUE_REPLAYGAIN_TRACK,
    LANG_VALUE_REPLAYGAIN_ALBUM,

    /* M-104 (plan maestro SS D): bloqueo por codigo con el interruptor
     * Hold. Textos fijados por el maestro para las tres familias. */
    LANG_LOCK_ENABLE,        /* activar */
    LANG_LOCK_CHANGE,        /* cambiar codigo */
    LANG_LOCK_REQUIRE,       /* pedir codigo */
    LANG_LOCK_REQUIRE_HOLD,  /* al bloquear */
    LANG_LOCK_REQUIRE_1MIN,  /* tras 1 minuto */
    LANG_LOCK_REQUIRE_5MIN,  /* tras 5 minutos */
    LANG_LOCK_REQUIRE_BOOT,  /* solo al encender */
    LANG_LOCK_REMOVE,        /* quitar bloqueo */
    LANG_LOCK_RESTING,       /* "bloqueado" -- pantalla en reposo */
    LANG_LEGAL_BODY,            /* el texto completo, con saltos de linea */

    LANG_COUNT
};

void metro_lang_set(enum metro_language lang);
enum metro_language metro_lang_get(void);
const char *metro_lang_str(enum metro_lang_id id);

/* M-110 (ronda "ajustes 2", contrato v19 SS A.1): el codigo de dos
 * letras que /.aura/settings.cfg usa para `language`, unico lugar
 * fuera de este modulo donde ese codigo viaja. Devuelve false para un
 * codigo que el contrato reconoce pero que este firmware TODAVIA no
 * implementa (fr/de/ru/it -- Fase 3 de esta ronda); metro_settings.c
 * ya trata eso igual que cualquier clave conocida con un valor que no
 * puede aplicar (deja el idioma como estaba). */
bool metro_lang_from_code(const char *code, enum metro_language *out);

/* El codigo de dos letras de `lang` -- lo inverso de arriba, para
 * cuando Metro es quien escribe el archivo compartido. */
const char *metro_lang_code(enum metro_language lang);

/* M-111: nombre nativo de `lang` para el selector (Ajustes > idioma,
 * plan maestro SS D.2: "Español · English · Français · Deutsch ·
 * Русский · Italiano") -- SIEMPRE en su propio idioma, nunca pasa por
 * metro_lang_str()/current_lang. Fuera de rango devuelve "". */
const char *metro_lang_native_name(enum metro_language lang);

/* R4/FA-5a (M-076): copia el PRIMER CARÁCTER de `s` -- no el primer
 * BYTE -- a `out`, en mayúscula si es una letra.
 *
 * Existe porque varios sitios dibujaban la inicial de una etiqueta con
 * `label[0]`, un solo byte. Para cualquier texto que empiece con una
 * letra acentuada eso parte la secuencia UTF-8 a la mitad y le entrega
 * a `lcd_putsxy()` un byte guía suelto sin continuación: glifo basura.
 *
 * **Es un bug preexistente, no uno que introduzca la acentuación del
 * catálogo**: una biblioteca real en español con un artista "Ángela" o
 * un álbum "Éxitos" ya lo disparaba -- solo que ninguna cadena
 * compilada del firmware empezaba con acento, así que no se había
 * visto. Acentuar `LANG_UNKNOWN_ALBUM` lo volvió alcanzable también
 * desde el propio firmware.
 *
 * Mayúsculas: ASCII `a-z` y las letras acentuadas de Latin-1 en UTF-8
 * (`á`→`Á`, `ñ`→`Ñ`), que la fuente sí trae (rango 0x20-0x17F,
 * gen_fonts.sh). `out` necesita al menos 5 bytes (4 de UTF-8 + NUL).
 * Una cadena vacía o NULL deja `out` vacío -- los llamadores ya tratan
 * eso como "sin inicial". */
void metro_lang_initial(const char *s, char *out, size_t outsz);

/* R4 (M-079): comparación para ORDENAR etiquetas de biblioteca, con
 * acentos plegados. Devuelve <0, 0 o >0 como strcmp().
 *
 * El problema que resuelve: la comparación anterior recorría BYTES y
 * solo pasaba a mayúscula el ASCII, así que cualquier inicial acentuada
 * (`Á` = 0xC3 0x81) caía **después de la Z**. Una artista "Ángela" se
 * iba al final de la lista, detrás de "Zoé" -- en una biblioteca en
 * español eso no es un caso raro.
 *
 * Reglas, en orden de aplicación:
 *   - Mayúsculas/minúsculas no cambian DÓNDE cae una etiqueta:
 *     "abba" y "ABBA" aterrizan las dos entre "Zzz" y "Beto". Lo que
 *     sí hacen es desempatar entre ellas (ver la última regla), así
 *     que la función no devuelve 0 para ese par -- devolverlo dejaría
 *     su orden relativo a merced del algoritmo de ordenamiento.
 *   - Las vocales acentuadas pliegan a su vocal base: `á`==`a`,
 *     `ü`==`u`. Es lo que espera cualquier hispanohablante al buscar
 *     en una lista.
 *   - `ñ` NO pliega a `n`: es letra propia y va **entre la N y la O**,
 *     como manda la RAE. Por eso las claves son enteros y no bytes --
 *     entre 'N' y 'O' no cabe nada.
 *   - Si todo empata en ese nivel, se desempata por bytes crudos. Es
 *     lo que hace el resultado DETERMINISTA: "Ángela"/"Angela" y
 *     "abba"/"ABBA" son pares que el plegado vuelve indistinguibles, y
 *     sin desempate su orden relativo quedaría a merced del algoritmo
 *     de ordenamiento. Solo devuelve 0 para cadenas byte a byte
 *     idénticas. */
int metro_lang_collate(const char *a, const char *b);

/* R5-F3 (M-083): copia `s` a `out` en MAYÚSCULAS, con las mismas reglas
 * de metro_lang_initial() aplicadas a cada carácter (ASCII a-z y las
 * acentuadas de Latin-1 en UTF-8; cualquier otra secuencia se copia tal
 * cual). Para la línea de artista del reproductor, que la maqueta del
 * dueño lleva en versalitas. Trunca en frontera de carácter UTF-8, nunca
 * a mitad de secuencia. */
void metro_lang_upper(const char *s, char *out, size_t outsz);

#endif /* METRO_LANG_H */

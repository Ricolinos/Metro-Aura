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
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "settings.h"
#include "backlight.h"
#include "powermgmt.h" /* R3-F6/DD-10: set_sleeptimer_duration()/get_sleep_timer() */
#include "ata_idle_notify.h" /* M-103: call_storage_idle_notifys() */
#include "eq.h" /* R3-F6/DD-10: dsp_eq_enable()/dsp_set_eq_coefs() */
#include "dsp_misc.h" /* M-103: dsp_replaygain_set_settings() */

#include "metro_screen_settings.h"
#include "metro_screen_about.h"
#include "metro_screen_lock.h"
#include "metro_lang.h"
#include "metro_theme.h"
#include "metro_widgets.h"
#include "metro_settings.h"
#include "metro_sync.h"
#include "metro_main.h"
#include "metro_music.h"  /* R5-F3: límite de volumen */
#include "metro_volume.h"
#include "metro_firmware_families.h" /* M-093: "cambiar sistema" */
#include "metro_screen_list.h"
#include "metro_screen_adjust.h" /* M-103: brillo/retroiluminación */

/* --- general: language, library update, reset settings ---------------- */

static const enum metro_lang_id anim_names[METRO_ANIM_COUNT] = {
    LANG_ANIM_OFF, LANG_ANIM_MINIMAL, LANG_ANIM_ALL,
};
static const enum metro_lang_id gfx_names[METRO_GFX_COUNT] = {
    LANG_GFX_LITE, LANG_GFX_FULL,
};

/* R3-F6/DD-10: cicla desactivado -> 15 -> 30 -> 60 -> 90 min llamando
 * set_sleeptimer_duration() directo (firmware/powermgmt.c) -- sin
 * pantalla propia, sin tocar apps/settings.h (Aura no implementa esta
 * feature en absoluto, INVESTIGACION-metro-r3.md F.1). El índice de
 * qué paso sigue se guarda aparte de get_sleep_timer() (que cuenta
 * hacia abajo en tiempo real, no sirve como "posición en la lista" a
 * ciclar) -- puramente de sesión, un temporizador de sueño reinicia en
 * cada boot en el propio Rockbox stock también, no hay nada que
 * persistir. */
static const int sleep_steps_min[] = { 0, 15, 30, 60, 90 };
#define SLEEP_STEPS_N (int)(sizeof(sleep_steps_min) / sizeof(sleep_steps_min[0]))
static int s_sleep_step = 0;

static void cycle_sleep(void)
{
    s_sleep_step = (s_sleep_step + 1) % SLEEP_STEPS_N;
    set_sleeptimer_duration(sleep_steps_min[s_sleep_step]);
}

static const char *sleep_subtitle(void)
{
    static char buf[16];
    int remaining_min = get_sleep_timer() / 60;

    if (remaining_min <= 0)
        return metro_lang_str(LANG_VALUE_OFF);

    snprintf(buf, sizeof(buf), "%d min", remaining_min);
    return buf;
}

/* R3-F6/DD-10: tabla propia de Metro, no la de settings.h (Aura sí
 * necesita ese extern de 4 líneas porque reusa los defaults de stock;
 * Metro no los reusa -- INVESTIGACION-metro-r3.md F.2, cero cambios a
 * `apps/settings.h`). Forma de banda (tipo/corte/Q) igual para los 4
 * presets, el mismo layout de 10 bandas que el menú EQ de stock usa
 * por default (`apps/settings_list.c`) -- valores genéricos de
 * ingeniería de audio, no código de Aura -- solo la GANANCIA por banda
 * cambia de preset a preset. Ganancia en décimas de dB (`eq_menu.h`:
 * EQ_GAIN_MIN/MAX = -240/240, o sea ±24.0 dB). */
enum metro_eq_preset {
    METRO_EQ_FLAT = 0,
    METRO_EQ_BASS,
    METRO_EQ_VOCAL,
    METRO_EQ_BRIGHT,
    METRO_EQ_PRESET_COUNT
};

static const enum metro_lang_id eq_preset_names[METRO_EQ_PRESET_COUNT] = {
    LANG_EQ_FLAT, LANG_EQ_BASS, LANG_EQ_VOCAL, LANG_EQ_BRIGHT,
};

struct eq_band_shape { enum eq_filter_type type; int cutoff; int q; };
static const struct eq_band_shape eq_shapes[EQ_NUM_BANDS] = {
    { EQ_FILTER_LOW_SHELF,     32,  7 },
    { EQ_FILTER_PEAK,          64, 10 },
    { EQ_FILTER_PEAK,         125, 10 },
    { EQ_FILTER_PEAK,         250, 10 },
    { EQ_FILTER_PEAK,         500, 10 },
    { EQ_FILTER_PEAK,        1000, 10 },
    { EQ_FILTER_PEAK,        2000, 10 },
    { EQ_FILTER_PEAK,        4000, 10 },
    { EQ_FILTER_PEAK,        8000, 10 },
    { EQ_FILTER_HIGH_SHELF, 16000,  7 },
};

static const int eq_preset_gains[METRO_EQ_PRESET_COUNT][EQ_NUM_BANDS] = {
    /* plano */     {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 },
    /* graves */    {  60,  40,  20,   0,   0,   0,   0,   0,   0,   0 },
    /* voz */       { -20, -10,   0,   0,  20,  40,  30,   0,   0,   0 },
    /* brillante */ {   0,   0,   0,   0,   0,   0,   0,  30,  40,  50 },
};

static int s_eq_preset = METRO_EQ_FLAT;

static void apply_eq_preset(enum metro_eq_preset preset)
{
    int i;

    /* Plano == sin procesamiento -- apagar el DSP entero en vez de
     * aplicar 10 bandas en 0 es más barato y más correcto (bypass
     * real, no "todas las bandas midiendo cero"). */
    dsp_eq_enable(preset != METRO_EQ_FLAT);

    for (i = 0; i < EQ_NUM_BANDS; i++)
    {
        struct eq_band_setting band;

        band.type   = eq_shapes[i].type;
        band.cutoff = eq_shapes[i].cutoff;
        band.q      = eq_shapes[i].q;
        band.gain   = eq_preset_gains[preset][i];
        dsp_set_eq_coefs(i, &band);
    }
}

static void cycle_eq(void)
{
    s_eq_preset = (s_eq_preset + 1) % METRO_EQ_PRESET_COUNT;
    apply_eq_preset((enum metro_eq_preset)s_eq_preset);
}

/* R5-F3 (M-083): límite de volumen en la escala 00..15 de Metro
 * (metro_volume.h), nunca en dB. Cinco presets que se ciclan con SELECT,
 * igual que brillo/retroiluminación -- 16 valores uno a uno serían
 * quince pulsaciones en el peor caso, y un límite por debajo de 06 no
 * tiene uso real. Persiste en global_settings.volume_limit (Rockbox). */
static const int volume_limit_steps[] = { 15, 12, 10, 8, 6 };
#define VOLUME_LIMIT_STEPS_N (int)(sizeof(volume_limit_steps) / sizeof(volume_limit_steps[0]))

static const char *volume_limit_subtitle(void)
{
    static char buf[4];
    snprintf(buf, sizeof(buf), "%02d", metro_music_volume_limit_level());
    return buf;
}

static void cycle_volume_limit(void)
{
    int cur = metro_music_volume_limit_level();
    int i, next = volume_limit_steps[0];

    /* Siguiente preset ESTRICTAMENTE menor que el actual; si no hay
     * (estamos en el más bajo, o en un valor raro por debajo), vuelve
     * al máximo. */
    for (i = 0; i < VOLUME_LIMIT_STEPS_N; i++)
        if (volume_limit_steps[i] < cur)
        {
            next = volume_limit_steps[i];
            break;
        }
    metro_music_set_volume_limit_level(next);
    call_storage_idle_notifys(true); /* M-103: ver settings_save_now() */
}

/* M-103: guardar un ajuste de Rockbox Y dejarlo en disco AHORA.
 *
 * `settings_save()` no escribe: registra un callback de "disco ocioso"
 * (`apps/settings.c:738`) que corre cuando el hilo de almacenamiento
 * decide que la unidad puede dormir -- y `call_storage_idle_notifys()`
 * se auto-bloquea 30 s entre corridas (`firmware/ata_idle_notify.c:58`).
 * En un apagado limpio el flush llega igual, por `system_flush()`
 * (`apps/misc.c:341`), asi que no era un bug; pero significa que entre
 * "el usuario eligio esto" y "esta en el disco" pueden pasar minutos, y
 * un iPod se queda sin bateria o se reinicia a mano (MENU+SELECT) sin
 * apagado limpio con toda naturalidad.
 *
 * Los ajustes PROPIOS de Metro (`metro_settings_save()` -> `aura.cfg`)
 * se escriben en el acto desde siempre. Que los de Rockbox sean mas
 * fragiles que los propios no tiene defensa desde el lado del usuario,
 * asi que las filas de esta pantalla fuerzan el flush. Cuesta una
 * escritura de config.cfg por pulsacion en una fila de ajustes, del
 * mismo orden que el aura.cfg que ya se escribia. */
static void settings_save_now(void)
{
    settings_save();
    call_storage_idle_notifys(true);
}

/* M-103 (matriz de Ajustes homologada, plan maestro SS C): apagado
 * automatico. Los mismos cuatro valores que Aura -- nunca / 10 / 20 /
 * 60 min -- sobre `global_settings.poweroff` (minutos, 0 = apagado;
 * apps/settings_list.c:1291) y `set_poweroff_timeout()`, que es lo que
 * arma el temporizador de verdad. A diferencia del temporizador de
 * sueno (que es de sesion y reinicia en cada arranque, R3-F6/DD-10),
 * este SI persiste: va en config.cfg via settings_save(). */
static const int poweroff_steps_min[] = { 0, 10, 20, 60 };
#define POWEROFF_STEPS_N (int)(sizeof(poweroff_steps_min) / sizeof(poweroff_steps_min[0]))

static const char *poweroff_subtitle(void)
{
    static char buf[16];

    if (global_settings.poweroff <= 0)
        return metro_lang_str(LANG_VALUE_NEVER);
    snprintf(buf, sizeof(buf), "%d min", global_settings.poweroff);
    return buf;
}

static void cycle_poweroff(void)
{
    int i, next = poweroff_steps_min[0];

    for (i = 0; i < POWEROFF_STEPS_N; i++)
        if (poweroff_steps_min[i] > global_settings.poweroff)
        {
            next = poweroff_steps_min[i];
            break;
        }
    global_settings.poweroff = next;
    set_poweroff_timeout(next);
    settings_save_now();
}

/* M-103 (P2 de la fase, matriz SS C "Ajuste de volumen"): replaygain.
 * Tres valores, los mismos que ofrece Aura -- apagado / pista / album
 * -- sobre `global_settings.replaygain_settings.type`
 * (lib/rbcodec/dsp/dsp_misc.h: 0 = pista, 1 = album, 2 = pista si hay
 * aleatorio, 4 = apagado). El valor 2 NO se expone: "depende de si el
 * aleatorio esta puesto" es una regla que el usuario no puede deducir
 * de una fila de dos palabras, y las tres familias tienen que ofrecer
 * lo mismo.
 *
 * `dsp_replaygain_set_settings()` aplica en vivo; el struct entero es
 * lo que persiste en config.cfg. */
static const int replaygain_steps[] = { 4, 0, 1 }; /* apagado, pista, album */
#define REPLAYGAIN_STEPS_N (int)(sizeof(replaygain_steps) / sizeof(replaygain_steps[0]))

static const enum metro_lang_id replaygain_names[REPLAYGAIN_STEPS_N] = {
    LANG_VALUE_OFF, LANG_VALUE_REPLAYGAIN_TRACK, LANG_VALUE_REPLAYGAIN_ALBUM,
};

static int replaygain_index(void)
{
    int i;

    for (i = 0; i < REPLAYGAIN_STEPS_N; i++)
        if (replaygain_steps[i] == global_settings.replaygain_settings.type)
            return i;
    return 0; /* incluye el valor 2, que esta fila no ofrece */
}

static void cycle_replaygain(void)
{
    int next = (replaygain_index() + 1) % REPLAYGAIN_STEPS_N;

    global_settings.replaygain_settings.type = replaygain_steps[next];
    dsp_replaygain_set_settings(&global_settings.replaygain_settings);
    settings_save_now();
}

/* M-103: clicker. UN solo interruptor, como el iPod original y como
 * Aura (que llego a la misma conclusion en su D-196/D-201): en el 6G
 * `keyclick` es el beep por el DAC -- inaudible sin audifonos, que es
 * como se usa el aparato la mayor parte del tiempo -- y
 * `keyclick_hardware` es el piezo, que es el clic que la gente
 * reconoce. Exponer dos filas para eso seria pedirle al usuario que
 * entienda una diferencia de implementacion. Se prenden y se apagan
 * juntos.
 *
 * No hace falta tocar nada mas para que suene: `get_custom_action()`
 * -- por donde pasa TODA la entrada de Metro (metro_input.c) -- ya
 * llama `keyclick_click()` al final de `get_action_worker()`
 * (apps/action.c:1005). Lo unico que faltaba era que el ajuste pudiera
 * estar prendido; ver el retiro del forzado en metro_apply_hygiene()
 * (metro_main.c, M-103). El valor 2 ("moderate" en la escala 0..3 de
 * Rockbox) es el medio de la escala: un clic de interfaz no deberia
 * ser lo mas fuerte que el aparato sabe hacer. */
#define METRO_KEYCLICK_ON_LEVEL 2

static void toggle_keyclick(void)
{
    bool on = (global_settings.keyclick != 0);

    global_settings.keyclick = on ? 0 : METRO_KEYCLICK_ON_LEVEL;
#ifdef HAVE_HARDWARE_CLICK
    global_settings.keyclick_hardware = !on;
#endif
    settings_save_now();
}

/* --- M-104: Ajustes > bloqueo (plan maestro SS D.6) --------------------
 * Sub-pagina con el mismo patron que "cambiar sistema": una pagina
 * local de una sola pivot, empujada con metro_screen_list_push().
 *
 * Antes esto era UNA fila que hacia dos cosas segun el estado (con
 * candado: quitarlo; sin candado: configurarlo). Con el sondeo del Hold
 * hay tres cosas mas que decidir -- cambiar el codigo, cuando se vuelve
 * a pedir, y quitarlo -- y meterlas en una fila que cambia de
 * significado seria adivinanza. */
static const enum metro_lang_id lock_require_names[METRO_LOCK_REQUIRE_COUNT] = {
    LANG_LOCK_REQUIRE_HOLD, LANG_LOCK_REQUIRE_1MIN,
    LANG_LOCK_REQUIRE_5MIN, LANG_LOCK_REQUIRE_BOOT,
};

static bool lock_is_set(void)
{
    return metro_screen_lock_state() != METRO_LOCK_NONE;
}

static int lock_count(void *ctx)
{
    (void)ctx;
    /* Sin bloqueo configurado solo se ofrece "activar": "cambiar
     * codigo", "pedir codigo" y "quitar bloqueo" no significan nada
     * todavia, y una fila inerte es peor que una fila ausente cuando lo
     * que falta es el paso previo. */
    return lock_is_set() ? 3 : 1;
}

static void lock_get_row(void *ctx, int index, struct metro_row *out)
{
    (void)ctx;
    out->subtitle = NULL;
    out->kind = METRO_ROW_ACTION;

    if (!lock_is_set())
    {
        out->title = metro_lang_str(LANG_LOCK_ENABLE);
        return;
    }

    switch (index)
    {
        case 0:
            out->title = metro_lang_str(LANG_LOCK_CHANGE);
            break;
        case 1:
            out->title = metro_lang_str(LANG_LOCK_REQUIRE);
            out->subtitle = metro_lang_str(
                lock_require_names[metro_settings.screen_lock_require]);
            out->kind = METRO_ROW_SETTING;
            break;
        default:
            out->title = metro_lang_str(LANG_LOCK_REMOVE);
            break;
    }
}

static void lock_on_select(void *ctx, int index)
{
    (void)ctx;

    if (!lock_is_set())
    {
        metro_screen_lock_setup();
        return;
    }

    switch (index)
    {
        case 0:
            /* Sin pedir el codigo actual, mismo criterio que "quitar":
             * para llegar aqui el aparato ya esta desbloqueado. */
            metro_screen_lock_setup();
            break;
        case 1:
            metro_settings.screen_lock_require =
                (enum metro_lock_require)((metro_settings.screen_lock_require + 1)
                                           % METRO_LOCK_REQUIRE_COUNT);
            metro_settings_save();
            break;
        default:
            if (metro_widgets_confirm(metro_lang_str(LANG_SETTING_LOCK),
                                       metro_lang_str(LANG_DIALOG_LOCK_OFF_TITLE)))
                metro_screen_lock_clear();
            break;
    }
}

static const struct metro_pivot lock_pivots[] = {
    { .name = LANG_SETTING_LOCK, .count = lock_count,
      .get_row = lock_get_row, .on_select = lock_on_select },
};
static const struct metro_page lock_page = {
    LANG_SETTING_LOCK, lock_pivots, 1, NULL
};

/* --- "cambiar sistema" (M-093, contrato v10 con tres familias) --------
 * Una fila por familia hermana de metro_firmware_families.h. Sin arbol
 * dormido la fila lleva "no instalado" y no hace nada; con el, pide
 * confirmacion y hace el cambio (metro_firmware_switch_to solo vuelve
 * si no pudo: el aparato sigue siendo Metro y la lista se redibuja). */
static int switch_count(void *ctx)
{
    (void)ctx;
    return metro_fw_sibling_count();
}

static void switch_get_row(void *ctx, int index, struct metro_row *out)
{
    const struct metro_fw_family *sibling = metro_fw_sibling(index);
    (void)ctx;

    out->title = metro_lang_str(sibling->name);
    out->subtitle = metro_firmware_sibling_installed(index)
                        ? NULL : metro_lang_str(LANG_VALUE_NOT_INSTALLED);
    out->kind = METRO_ROW_ACTION;
}

static void switch_on_select(void *ctx, int index)
{
    const struct metro_fw_family *sibling = metro_fw_sibling(index);
    char question[96];
    (void)ctx;

    if (sibling == NULL || !metro_firmware_sibling_installed(index))
        return;

    snprintf(question, sizeof(question),
             metro_lang_str(LANG_DIALOG_SWITCH_FMT),
             metro_lang_str(sibling->name));
    if (metro_widgets_confirm(metro_lang_str(LANG_HUB_SETTINGS), question))
        metro_firmware_switch_to(index);
}

/* M-103: inicializadores DESIGNADOS. Los posicionales dejaban un
 * -Wmissing-field-initializers por cada campo que struct metro_pivot
 * fue ganando (tile_cols, get_tile, empty_message, on_select_hold), y
 * ese ruido esconde el warning del día que sí importe. Designados, los
 * campos que no se nombran quedan en 0/NULL por el estándar, que es
 * justo el default que cada uno documenta. */
static const struct metro_pivot switch_pivots[] = {
    { .name = LANG_SETTING_SWITCH_SYSTEM, .count = switch_count,
      .get_row = switch_get_row, .on_select = switch_on_select },
};
static const struct metro_page switch_page = {
    LANG_SETTING_SWITCH_SYSTEM, switch_pivots, 1, NULL
};

static int general_count(void *ctx)
{
    (void)ctx;
    return 13; /* M-103: +apagado automático, +ajuste de volumen, +clicker */
}

static void general_get_row(void *ctx, int index, struct metro_row *out)
{
    (void)ctx;

    switch (index)
    {
        case 0:
            out->title = metro_lang_str(LANG_SETTING_LANGUAGE);
            out->subtitle = metro_lang_str(metro_lang_get() == METRO_LANG_ES
                                                ? LANG_VALUE_SPANISH
                                                : LANG_VALUE_ENGLISH);
            out->kind = METRO_ROW_SETTING;
            break;
        case 1:
            out->title = metro_lang_str(LANG_SETTING_ANIMATIONS);
            out->subtitle = metro_lang_str(anim_names[metro_settings.animations]);
            out->kind = METRO_ROW_SETTING;
            break;
        case 2:
            out->title = metro_lang_str(LANG_SETTING_GRAPHICS);
            out->subtitle = metro_lang_str(gfx_names[metro_settings.graphics]);
            out->kind = METRO_ROW_SETTING;
            break;
        case 3:
            out->title = metro_lang_str(LANG_SETTING_SLEEP);
            out->subtitle = sleep_subtitle();
            out->kind = METRO_ROW_SETTING;
            break;
        case 4:
            /* M-103: junto al temporizador de sueño -- son las dos
             * filas de energía, y así se leen como un par. */
            out->title = metro_lang_str(LANG_SETTING_POWEROFF);
            out->subtitle = poweroff_subtitle();
            out->kind = METRO_ROW_SETTING;
            break;
        case 5:
            out->title = metro_lang_str(LANG_SETTING_EQ);
            out->subtitle = metro_lang_str(eq_preset_names[s_eq_preset]);
            out->kind = METRO_ROW_SETTING;
            break;
        case 6:
            out->title = metro_lang_str(LANG_SETTING_VOLUME_LIMIT);
            out->subtitle = volume_limit_subtitle();
            out->kind = METRO_ROW_SETTING;
            break;
        case 7:
            out->title = metro_lang_str(LANG_SETTING_REPLAYGAIN);
            out->subtitle = metro_lang_str(replaygain_names[replaygain_index()]);
            out->kind = METRO_ROW_SETTING;
            break;
        case 8:
            /* M-103: cierra el bloque de reproducción (EQ, límite de
             * volumen, ajuste de volumen, clicker), igual que en Aura. */
            out->title = metro_lang_str(LANG_SETTING_KEYCLICK);
            out->subtitle = metro_lang_str(global_settings.keyclick
                                                ? LANG_VALUE_ON : LANG_VALUE_OFF);
            out->kind = METRO_ROW_SETTING;
            break;
        case 9:
            /* R3-F7/DD-8 (M-068): estado real del candado, no la
             * preferencia guardada -- ARMED y ACTIVE se ven igual desde
             * aquí (para llegar a esta fila el aparato ya está
             * desbloqueado), así que basta con "activado"/"desactivado".
             * M-104: la fila pasa de ACTION a NAV -- ahora abre la
             * sub-página con las cuatro opciones del plan maestro §D.6.
             * El subtítulo se queda: es lo que hace que se sepa si hay
             * bloqueo sin tener que entrar. */
            out->title = metro_lang_str(LANG_SETTING_LOCK);
            out->subtitle = metro_lang_str(
                metro_screen_lock_state() == METRO_LOCK_NONE ? LANG_VALUE_OFF
                                                              : LANG_VALUE_ON);
            out->kind = METRO_ROW_NAV;
            break;
        case 10:
            out->title = metro_lang_str(LANG_SETTING_LIBRARY);
            out->subtitle = NULL;
            out->kind = METRO_ROW_ACTION;
            break;
        case 11:
            /* M-093 (era M-090 "cambiar a Aura"): submenu con una fila
             * por familia hermana -- se ve siempre, para que se sepa que
             * existe la opcion; cada fila dice sola si esta instalada. */
            out->title = metro_lang_str(LANG_SETTING_SWITCH_SYSTEM);
            out->subtitle = NULL;
            out->kind = METRO_ROW_NAV;
            break;
        default:
            out->title = metro_lang_str(LANG_SETTING_RESET);
            out->subtitle = NULL;
            out->kind = METRO_ROW_ACTION;
            break;
    }
}

static void general_on_select(void *ctx, int index)
{
    (void)ctx;

    switch (index)
    {
        case 0:
            metro_lang_set(metro_lang_get() == METRO_LANG_ES
                                ? METRO_LANG_EN : METRO_LANG_ES);
            metro_settings.language = metro_lang_get();
            metro_settings_save();
            break;

        case 1:
            metro_settings.animations =
                (enum metro_anim_level)((metro_settings.animations + 1) % METRO_ANIM_COUNT);
            metro_settings_save();
            break;

        case 2:
            metro_settings.graphics =
                (enum metro_gfx_level)((metro_settings.graphics + 1) % METRO_GFX_COUNT);
            metro_settings_save();
            break;

        case 3:
            cycle_sleep();
            break;

        case 4:
            cycle_poweroff(); /* M-103 */
            break;

        case 5:
            cycle_eq();
            break;

        case 6:
            cycle_volume_limit();
            break;

        case 7:
            cycle_replaygain(); /* M-103 */
            break;

        case 8:
            toggle_keyclick(); /* M-103 */
            break;

        case 9:
            metro_screen_list_push(&lock_page); /* M-104 */
            break;

        case 10:
            /* M-100: la advertencia de duración va como detalle, no
             * dentro de la pregunta -- draw_question() está topada en
             * dos líneas y la cortaba a la mitad. */
            if (metro_widgets_confirm_detail(metro_lang_str(LANG_HUB_SETTINGS),
                                              metro_lang_str(LANG_DIALOG_LIBRARY_TITLE),
                                              metro_lang_str(LANG_DIALOG_LIBRARY_DETAIL)))
            {
                metro_sync_request_manual();
                metro_run_sync_screen_if_needed();
            }
            break;

        case 11:
            metro_screen_list_push(&switch_page);
            break;

        default:
            if (metro_widgets_confirm(metro_lang_str(LANG_HUB_SETTINGS),
                                       metro_lang_str(LANG_DIALOG_RESET_TITLE)))
            {
                metro_settings.theme = METRO_THEME_DEFAULT;
                metro_settings.accent = METRO_ACCENT_DEFAULT;
                metro_settings.language = METRO_LANG_ES;
                metro_settings.animations = METRO_ANIM_DEFAULT;
                metro_settings.graphics = METRO_GFX_DEFAULT;
                metro_settings_save();

                metro_theme_set(metro_settings.theme);
                metro_accent_set(metro_settings.accent);
                metro_lang_set(metro_settings.language);

                /* R3-F6/DD-10: sueño/EQ son de sesión, no parte de
                 * metro_settings -- pero "restablecer ajustes" debería
                 * verse consistente con el resto de esta fila. */
                s_sleep_step = 0;
                set_sleeptimer_duration(sleep_steps_min[s_sleep_step]);
                s_eq_preset = METRO_EQ_FLAT;
                apply_eq_preset((enum metro_eq_preset)s_eq_preset);
                metro_music_set_volume_limit_level(METRO_VOLUME_MAX_LEVEL);

                /* M-103: apagado automático y clicker SÍ persisten
                 * (config.cfg), así que "restablecer ajustes" tiene que
                 * devolverlos a su default o quedarían fuera de una
                 * fila que promete restablecer todo. El default de
                 * apagado automático es el de Rockbox (10 min,
                 * apps/settings_list.c:1291); el del clicker es
                 * apagado, que es el que Metro traía desde M-008. */
                global_settings.poweroff = 10;
                set_poweroff_timeout(global_settings.poweroff);
                global_settings.keyclick = 0;
#ifdef HAVE_HARDWARE_CLICK
                global_settings.keyclick_hardware = false;
#endif
                settings_save_now();
            }
            break;
    }
}

/* --- display: theme, accent, brightness, backlight --------------------- */

static const enum metro_lang_id accent_names[METRO_ACCENT_COUNT] = {
    LANG_ACCENT_BLUE, LANG_ACCENT_BROWN, LANG_ACCENT_GREEN, LANG_ACCENT_LIME,
    LANG_ACCENT_MAGENTA, LANG_ACCENT_MANGO, LANG_ACCENT_PINK,
    LANG_ACCENT_PURPLE, LANG_ACCENT_RED, LANG_ACCENT_TEAL,
};

/* M-103 (plan maestro SS C): brillo y retroiluminación dejan de ciclar
 * con SELECT y abren una pantalla propia con barra
 * (metro_screen_adjust.h). Antes eran cuatro y seis valores que había
 * que recorrer a ciegas, sin ver nunca el rango; ahora la rueda ES el
 * control, cada paso se aplica en vivo y MENU vuelve. Es el
 * equivalente al deslizador de Aura con el gesto que sí existe en una
 * rueda de clic (no hay arrastre).
 *
 * Brillo: 10 pasos LINEALES sobre el rango real del panel
 * (MIN_BRIGHTNESS_SETTING..MAX_BRIGHTNESS_SETTING = 1..63), tal como
 * pide el plan. Retroiluminación: se conservan los SEIS valores que ya
 * tenía -- son tiempos, no una magnitud continua, y "nunca" no cae en
 * ninguna rejilla lineal -- pero se manejan con la misma pantalla, que
 * es lo que hace que las dos filas se sientan iguales. */
#define BRIGHTNESS_STEPS_N 10

static int brightness_value(int step)
{
    /* step 0 -> MIN, step N-1 -> MAX, repartido parejo y redondeando
     * al entero más cercano para que no se pierda ningún nivel útil
     * del panel por truncamiento. */
    int span = MAX_BRIGHTNESS_SETTING - MIN_BRIGHTNESS_SETTING;

    return MIN_BRIGHTNESS_SETTING +
           (step * span * 2 + (BRIGHTNESS_STEPS_N - 1)) / ((BRIGHTNESS_STEPS_N - 1) * 2);
}

static int brightness_step_of(int value)
{
    int i, best = 0, best_d = -1;

    /* El valor guardado puede no caer exactamente en la rejilla (un
     * aura.cfg de antes de M-103, o el default de Rockbox): se entra
     * por el paso más cercano, nunca por el 0. */
    for (i = 0; i < BRIGHTNESS_STEPS_N; i++)
    {
        int d = brightness_value(i) - value;

        if (d < 0)
            d = -d;
        if (best_d < 0 || d < best_d)
        {
            best_d = d;
            best = i;
        }
    }
    return best;
}

static const char *brightness_label(void *ctx, int step)
{
    static char buf[8];

    (void)ctx;
    /* El porcentaje es la POSICIÓN en el control (10 %..100 %), el mismo
     * número que dibuja la barra. Antes decía el crudo del panel
     * (`brightness / 0x3f`), que no coincidía con la barra por un paso
     * -- "55 %" bajo una barra llena al 60 %. Ninguno de los dos es
     * "brillo percibido" (la respuesta del panel no es lineal), así que
     * entre un número que miente igual y contradice a la barra, y uno
     * que miente igual pero concuerda, gana el segundo. */
    snprintf(buf, sizeof(buf), "%d%%", (step + 1) * 100 / BRIGHTNESS_STEPS_N);
    return buf;
}

static void brightness_apply(void *ctx, int step)
{
    (void)ctx;
    global_settings.brightness = brightness_value(step);
    backlight_set_brightness(global_settings.brightness);
}

static const int backlight_steps[] = { 5, 10, 15, 30, 60, -1 }; /* -1 = never */
#define BACKLIGHT_STEPS_N (int)(sizeof(backlight_steps) / sizeof(backlight_steps[0]))

static const char *backlight_label(void *ctx, int step)
{
    static char buf[12];

    (void)ctx;
    if (backlight_steps[step] < 0)
        return metro_lang_str(LANG_VALUE_NEVER);
    snprintf(buf, sizeof(buf), "%ds", backlight_steps[step]);
    return buf;
}

static void backlight_apply(void *ctx, int step)
{
    (void)ctx;
    global_settings.backlight_timeout = backlight_steps[step];
    backlight_set_timeout(global_settings.backlight_timeout);
}

static int backlight_step_of(int value)
{
    int i;

    for (i = 0; i < BACKLIGHT_STEPS_N; i++)
        if (backlight_steps[i] == value)
            return i;
    return 0;
}

static int display_count(void *ctx)
{
    (void)ctx;
    return 4;
}

static void display_get_row(void *ctx, int index, struct metro_row *out)
{
    /* One buffer per row that formats a value -- rows are drawn
     * straight after get_row today, but a shared buffer would silently
     * show the last-formatted value on both rows the moment a caller
     * keeps two struct metro_row around (R5-F1 audit). */
    static char brightness_buf[16];
    static char backlight_buf[16];

    (void)ctx;
    out->kind = METRO_ROW_SETTING;

    switch (index)
    {
        case 0:
            out->title = metro_lang_str(LANG_SETTING_THEME);
            out->subtitle = metro_lang_str(metro_theme_get() == METRO_THEME_DARK
                                                ? LANG_VALUE_DARK : LANG_VALUE_LIGHT);
            break;
        case 1:
            out->title = metro_lang_str(LANG_SETTING_ACCENT);
            out->subtitle = metro_lang_str(accent_names[metro_accent_get()]);
            break;
        case 2:
            out->title = metro_lang_str(LANG_SETTING_BRIGHTNESS);
            snprintf(brightness_buf, sizeof(brightness_buf), "%d%%",
                     global_settings.brightness * 100 / MAX_BRIGHTNESS_SETTING);
            out->subtitle = brightness_buf;
            break;
        default:
            out->title = metro_lang_str(LANG_SETTING_BACKLIGHT);
            if (global_settings.backlight_timeout < 0)
                out->subtitle = metro_lang_str(LANG_VALUE_NEVER);
            else
            {
                snprintf(backlight_buf, sizeof(backlight_buf), "%ds",
                         global_settings.backlight_timeout);
                out->subtitle = backlight_buf;
            }
            break;
    }
}

static void display_on_select(void *ctx, int index)
{
    (void)ctx;

    switch (index)
    {
        case 0:
            metro_theme_set(metro_theme_get() == METRO_THEME_DARK
                                 ? METRO_THEME_LIGHT : METRO_THEME_DARK);
            metro_settings.theme = metro_theme_get();
            metro_settings_save();
            break;

        case 1:
        {
            int next = (metro_accent_get() + 1) % METRO_ACCENT_COUNT;
            metro_accent_set((enum metro_accent)next);
            metro_settings.accent = metro_accent_get();
            metro_settings_save();
            break;
        }

        case 2:
        {
            /* M-103: pantalla propia con barra; el valor ya quedó
             * aplicado en vivo, aquí solo se persiste al volver -- un
             * settings_save() por paso de rueda sería una escritura a
             * disco por clic. */
            static const struct metro_adjust_spec spec = {
                LANG_SETTING_BRIGHTNESS, BRIGHTNESS_STEPS_N,
                brightness_label, brightness_apply, NULL
            };

            metro_screen_adjust_run(&spec,
                                     brightness_step_of(global_settings.brightness));
            settings_save_now();
            break;
        }

        default:
        {
            static const struct metro_adjust_spec spec = {
                LANG_SETTING_BACKLIGHT, BACKLIGHT_STEPS_N,
                backlight_label, backlight_apply, NULL
            };

            metro_screen_adjust_run(&spec,
                                     backlight_step_of(global_settings.backlight_timeout));
            settings_save_now();
            break;
        }
    }
}

/* metro_screen_about_pivot (metro_screen_about.c) owns the 3rd pivot's
 * provider -- About outgrew a single static row (F8: device name,
 * sync counts) -- it's an extern const from another translation unit,
 * so it can't sit in a static initializer here; all_pivots[] is filled
 * in once, at first use. */
static struct metro_pivot all_pivots[3];
static const struct metro_page settings_page = {
    LANG_HUB_SETTINGS, all_pivots, 3, NULL
};

const struct metro_page *metro_screen_settings_page(void)
{
    static bool built = false;

    if (!built)
    {
        all_pivots[0] = (struct metro_pivot){
            .name = LANG_PIVOT_GENERAL, .count = general_count,
            .get_row = general_get_row, .on_select = general_on_select };
        all_pivots[1] = (struct metro_pivot){
            .name = LANG_PIVOT_DISPLAY, .count = display_count,
            .get_row = display_get_row, .on_select = display_on_select };
        all_pivots[2] = metro_screen_about_pivot;
        built = true;
    }
    return &settings_page;
}

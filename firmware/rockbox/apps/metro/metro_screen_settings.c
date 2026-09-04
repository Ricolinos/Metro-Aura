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
#include "eq.h" /* R3-F6/DD-10: dsp_eq_enable()/dsp_set_eq_coefs() */

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
}

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

static const struct metro_pivot switch_pivots[] = {
    { LANG_SETTING_SWITCH_SYSTEM, switch_count, switch_get_row, switch_on_select,
      NULL, 0, NULL, 0 },
};
static const struct metro_page switch_page = {
    LANG_SETTING_SWITCH_SYSTEM, switch_pivots, 1, NULL
};

static int general_count(void *ctx)
{
    (void)ctx;
    return 10;
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
            out->title = metro_lang_str(LANG_SETTING_EQ);
            out->subtitle = metro_lang_str(eq_preset_names[s_eq_preset]);
            out->kind = METRO_ROW_SETTING;
            break;
        case 5:
            out->title = metro_lang_str(LANG_SETTING_VOLUME_LIMIT);
            out->subtitle = volume_limit_subtitle();
            out->kind = METRO_ROW_SETTING;
            break;
        case 6:
            /* R3-F7/DD-8 (M-068): estado real del candado, no la
             * preferencia guardada -- ARMED y ACTIVE se ven igual desde
             * aquí (para llegar a esta fila el aparato ya está
             * desbloqueado), así que basta con "activado"/"desactivado". */
            out->title = metro_lang_str(LANG_SETTING_LOCK);
            out->subtitle = metro_lang_str(
                metro_screen_lock_state() == METRO_LOCK_NONE ? LANG_VALUE_OFF
                                                              : LANG_VALUE_ON);
            out->kind = METRO_ROW_SETTING;
            break;
        case 7:
            out->title = metro_lang_str(LANG_SETTING_LIBRARY);
            out->subtitle = NULL;
            out->kind = METRO_ROW_ACTION;
            break;
        case 8:
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
            cycle_eq();
            break;

        case 5:
            cycle_volume_limit();
            break;

        case 6:
            /* R3-F7/DD-8 (M-068): con candado -> confirmar y quitarlo;
             * sin candado -> configurar uno nuevo (dos capturas). Quitar
             * pide confirmación (destruye la clave guardada), poner no
             * la necesita: la propia doble captura ya es la confirmación,
             * y MENU cancela en cualquier momento. */
            if (metro_screen_lock_state() != METRO_LOCK_NONE)
            {
                if (metro_widgets_confirm(metro_lang_str(LANG_HUB_SETTINGS),
                                           metro_lang_str(LANG_DIALOG_LOCK_OFF_TITLE)))
                    metro_screen_lock_clear();
            }
            else
                metro_screen_lock_setup();
            break;

        case 7:
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

        case 8:
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

/* A handful of presets rather than the raw 1..MAX_BRIGHTNESS_SETTING
 * range or every possible timeout value -- same "cycle through a
 * short list via SELECT" pattern as theme/accent/repeat, simpler than
 * a slider Metro's input model doesn't have anyway (no drag gesture on
 * a clickwheel). */
static const int brightness_steps[] = { 16, 32, 48, MAX_BRIGHTNESS_SETTING };
#define BRIGHTNESS_STEPS_N (int)(sizeof(brightness_steps) / sizeof(brightness_steps[0]))

static const int backlight_steps[] = { 5, 10, 15, 30, 60, -1 }; /* -1 = never */
#define BACKLIGHT_STEPS_N (int)(sizeof(backlight_steps) / sizeof(backlight_steps[0]))

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
            int i, next = brightness_steps[0];
            for (i = 0; i < BRIGHTNESS_STEPS_N; i++)
                if (brightness_steps[i] > global_settings.brightness)
                {
                    next = brightness_steps[i];
                    break;
                }
            global_settings.brightness = next;
            backlight_set_brightness(next);
            settings_save();
            break;
        }

        default:
        {
            int i, next = backlight_steps[0];
            for (i = 0; i < BACKLIGHT_STEPS_N; i++)
                if (backlight_steps[i] > global_settings.backlight_timeout)
                {
                    next = backlight_steps[i];
                    break;
                }
            global_settings.backlight_timeout = next;
            backlight_set_timeout(next);
            settings_save();
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
            LANG_PIVOT_GENERAL, general_count, general_get_row, general_on_select, NULL };
        all_pivots[1] = (struct metro_pivot){
            LANG_PIVOT_DISPLAY, display_count, display_get_row, display_on_select, NULL };
        all_pivots[2] = metro_screen_about_pivot;
        built = true;
    }
    return &settings_page;
}

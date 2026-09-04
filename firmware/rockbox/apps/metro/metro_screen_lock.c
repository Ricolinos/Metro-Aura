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
#include <string.h>
#include <stdio.h>

#include "string-extra.h" /* strlcpy: en el target no viene por <string.h> */

#include "lcd.h"
#include "misc.h"
#include "button.h"  /* M-104: button_hold() */
#include "backlight.h" /* M-104: lcd_active() */
#include "kernel.h"  /* M-104: current_tick */
#include "queue.h"   /* M-104: SYS_EVENT en el bucle de reposo */

#include "metro_screen_lock.h"
#include "metro_screen_usb.h"
#include "metro_settings.h"
#include "metro_input.h"
#include "metro_keymap.h"
#include "metro_draw.h"
#include "metro_fonts.h"
#include "metro_theme.h"
#include "metro_lang.h"
#include "metro_widgets.h" /* M-104: metro_widgets_draw_glyph() */
#include "metro_glyphs.h"

/* Geometría de las 4 casillas: ancho*4 + hueco*3 = 212, centrado en los
 * 320 px de ancho del panel. Alto/ancho elegidos para que un dígito en
 * MFONT_TITLE (28 px) respire adentro sin tocar el borde. */
#define LOCK_BOX_W   44
#define LOCK_BOX_H   56
#define LOCK_BOX_GAP 12
#define LOCK_BOXES_W (METRO_LOCK_PIN_LEN * LOCK_BOX_W + \
                      (METRO_LOCK_PIN_LEN - 1) * LOCK_BOX_GAP)
#define LOCK_BOXES_X ((LCD_WIDTH - LOCK_BOXES_W) / 2)
#define LOCK_BOXES_Y 116
#define LOCK_TITLE_Y 48
#define LOCK_HINT_Y  196
/* Punto que representa un dígito ya confirmado (los que siguen en
 * blanco no dibujan nada adentro). */
#define LOCK_DOT_SIZE 10

/* M-104: pantalla de bloqueo en reposo -- un candado grande centrado y
 * el rotulo debajo. Mismo glifo antialiaseado de 40px y misma altura
 * que la pantalla USB (M-089): es el tamano que el dueno ya aprobo para
 * "un simbolo solo en una pantalla vacia", y usar otro haria que dos
 * pantallas del mismo caracter se vieran distintas sin motivo. */
#define LOCK_RESTING_GLYPH_Y  86
#define LOCK_RESTING_LABEL_Y  148

/* Modo de la pantalla: la misma entrada de 4 dígitos sirve a los tres
 * flujos, solo cambian el rótulo y qué se hace al completar. */
enum lock_mode {
    LOCK_MODE_UNLOCK = 0, /* desbloquear (candado activo) */
    LOCK_MODE_SET,        /* primera captura al configurar */
    LOCK_MODE_CONFIRM,    /* segunda captura al configurar */
};

static enum metro_lock_state s_state = METRO_LOCK_NONE;

/* Dígitos en captura. s_focus es cuál se está marcando ahora (0..3);
 * los anteriores ya están confirmados, los siguientes en blanco. */
static int s_digits[METRO_LOCK_PIN_LEN];
static int s_focus;

static void reset_entry(void)
{
    int i;

    for (i = 0; i < METRO_LOCK_PIN_LEN; i++)
        s_digits[i] = 0;
    s_focus = 0;
}

/* Falla ABIERTO a propósito (ver la cabecera del .h): cualquier cosa que
 * no sean exactamente 4 dígitos -- clave truncada, con letras, vacía, o
 * un aura.cfg con `screen_lock: 1` pero sin línea de `screen_lock_pin`
 * -- cuenta como "sin candado". Es justo la trampa que Aura-Firmware sí
 * tiene: allá el PIN se empaca como entero, así que una clave ausente y
 * la clave "0000" son indistinguibles y el aparato queda bloqueado con
 * 0000 sin que nadie la haya configurado. Guardar la clave como cadena
 * (y validarla aquí) hace que ese caso no exista. */
static bool pin_is_valid(const char *pin)
{
    int i;

    if (pin == NULL || strlen(pin) != METRO_LOCK_PIN_LEN)
        return false;

    for (i = 0; i < METRO_LOCK_PIN_LEN; i++)
    {
        if (pin[i] < '0' || pin[i] > '9')
            return false;
    }
    return true;
}

static void entry_to_string(char *out)
{
    int i;

    for (i = 0; i < METRO_LOCK_PIN_LEN; i++)
        out[i] = (char)('0' + s_digits[i]);
    out[METRO_LOCK_PIN_LEN] = '\0';
}

void metro_screen_lock_init(void)
{
    /* Rearmar en CADA arranque es deliberado, no un descuido: si el
     * candado no volviera a activarse solo, apagar y prender el aparato
     * lo saltaría por completo. Y hacerlo aquí -- una vez, sin condición
     * -- cierra de golpe todos los caminos de apagado (SYS_POWEROFF,
     * batería crítica, el hold de PLAY del driver) sin tener que
     * interceptar cada uno ni arriesgar que un camino nuevo se olvide de
     * rearmar. Es la única parte del diseño de Aura-Firmware que se
     * copia tal cual, porque su razonamiento es correcto. */
    s_state = (metro_settings.screen_lock && pin_is_valid(metro_settings.screen_lock_pin))
                  ? METRO_LOCK_ACTIVE
                  : METRO_LOCK_NONE;
}

enum metro_lock_state metro_screen_lock_state(void)
{
    return s_state;
}

static void draw_entry(enum lock_mode mode, bool mismatch)
{
    enum metro_lang_id title_id;
    enum metro_lang_id hint_id;
    int i, w, h;

    switch (mode)
    {
        case LOCK_MODE_SET:     title_id = LANG_LOCK_TITLE_SET;
                                hint_id  = LANG_LOCK_HINT_SET;     break;
        case LOCK_MODE_CONFIRM: title_id = LANG_LOCK_TITLE_CONFIRM;
                                hint_id  = mismatch ? LANG_LOCK_HINT_MISMATCH
                                                    : LANG_LOCK_HINT_CONFIRM; break;
        default:                title_id = LANG_LOCK_TITLE_LOCKED;
                                hint_id  = mismatch ? LANG_LOCK_HINT_WRONG
                                                    : LANG_LOCK_HINT_UNLOCK;  break;
    }

    metro_draw_clear();
    /* Ceja vacía a propósito: el reloj y la batería del encabezado sí se
     * dibujan (metro_draw_header() los pone siempre) -- son justo lo que
     * uno quiere poder mirar en un aparato bloqueado -- pero la ceja
     * nombraría una página a la que no se puede llegar. */
    metro_draw_header("");

    metro_draw_text(MFONT_DISPLAY, 12, LOCK_TITLE_Y, metro_lang_str(title_id),
                     metro_color_fg());

    for (i = 0; i < METRO_LOCK_PIN_LEN; i++)
    {
        int x = LOCK_BOXES_X + i * (LOCK_BOX_W + LOCK_BOX_GAP);
        bool focused = (i == s_focus);

        lcd_set_foreground(focused ? metro_color_accent() : metro_color_tertiary());
        lcd_drawrect(x, LOCK_BOXES_Y, LOCK_BOX_W, LOCK_BOX_H);

        if (focused)
        {
            /* El dígito que se está marcando se ve en claro -- hay que
             * poder leerlo para girar la rueda hasta el que se quiere. */
            char buf[2];

            buf[0] = (char)('0' + s_digits[i]);
            buf[1] = '\0';
            lcd_setfont(metro_font_id(MFONT_TITLE));
            lcd_getstringsize((const unsigned char *)buf, &w, &h);
            metro_draw_text(MFONT_TITLE, x + (LOCK_BOX_W - w) / 2,
                             LOCK_BOXES_Y + (LOCK_BOX_H - h) / 2, buf,
                             metro_color_accent());
        }
        else if (i < s_focus)
        {
            /* Ya confirmado: punto, no el dígito. Enmascarar solo lo ya
             * capturado es lo que hace que mirar por encima del hombro
             * no regale la clave entera, sin quitarle al dueño la
             * legibilidad del dígito que está girando. */
            lcd_set_foreground(metro_color_fg());
            lcd_fillrect(x + (LOCK_BOX_W - LOCK_DOT_SIZE) / 2,
                          LOCK_BOXES_Y + (LOCK_BOX_H - LOCK_DOT_SIZE) / 2,
                          LOCK_DOT_SIZE, LOCK_DOT_SIZE);
        }
    }

    metro_draw_text(MFONT_CAPTION, 12, LOCK_HINT_Y, metro_lang_str(hint_id),
                     metro_color_secondary());
    lcd_update();
}

/* Corre la captura de 4 dígitos. Devuelve true si se completaron los
 * cuatro (la clave queda en `out`), false si el usuario canceló con MENU
 * -- imposible en LOCK_MODE_UNLOCK, donde MENU solo borra el dígito
 * anterior y nunca sale (no existe un "cancelar" legítimo ahí: la única
 * salida es acertar, o borrar la clave del aura.cfg por USB). */
static bool run_entry(enum lock_mode mode, char *out, bool mismatch)
{
    reset_entry();
    draw_entry(mode, mismatch);

    while (1)
    {
        int steps = 1;
        /* Con timeout, no bloqueando indefinidamente: el encabezado trae
         * reloj y batería, así que la pantalla tiene que refrescarse
         * sola aunque nadie toque un botón. */
        int action = metro_input_next(MCTX_LOCK, HZ / 2, &steps);

        if (action & SYS_EVENT)
        {
            /* DD-8: el USB se atiende NORMAL estando bloqueado -- Metro
             * no difiere el montaje (eso es el parche a apps/misc.c que
             * esta fase evita, ver docs/DESVIACIONES.md R3-6). Es una
             * pérdida de privacidad consciente y documentada... y a la
             * vez lo que mantiene viva la salida de emergencia: quien
             * olvidó su clave puede montar el disco y borrarla. */
            if (action == SYS_USB_CONNECTED)
                metro_screen_usb_show();

            if (default_event_handler(action) == SYS_USB_CONNECTED &&
                mode == LOCK_MODE_UNLOCK)
            {
                /* Releer del disco lo que la sesión USB pudo haber
                 * cambiado: si el dueño borró las dos líneas del
                 * aura.cfg, la salida de emergencia surte efecto AQUÍ
                 * MISMO, sin exigirle además adivinar que hay que
                 * reiniciar. (Y de paso evita el reverso: que un guardado
                 * posterior regenere el aura.cfg desde la copia vieja en
                 * RAM y resucite la clave recién borrada.) */
                metro_settings_load();
                if (!metro_settings.screen_lock ||
                    !pin_is_valid(metro_settings.screen_lock_pin))
                {
                    s_state = METRO_LOCK_NONE;
                    return false;
                }
            }
            draw_entry(mode, mismatch);
            continue;
        }

        switch (action)
        {
            case MACT_NEXT:
                s_digits[s_focus] = (s_digits[s_focus] + 1) % 10;
                break;

            case MACT_PREV:
                s_digits[s_focus] = (s_digits[s_focus] + 9) % 10;
                break;

            case MACT_SELECT:
                if (s_focus < METRO_LOCK_PIN_LEN - 1)
                {
                    s_focus++;
                    break;
                }
                entry_to_string(out);
                return true;

            case MACT_BACK:
                /* Retroceder un dígito -- Aura-Firmware no tiene esto y
                 * equivocarse a media clave obliga allá a completar los
                 * cuatro y fallar a propósito. En el primer dígito, MENU
                 * cancela al configurar, y no hace nada al desbloquear. */
                if (s_focus > 0)
                {
                    s_focus--;
                    s_digits[s_focus] = 0;
                }
                else if (mode != LOCK_MODE_UNLOCK)
                    return false;
                break;

            case MACT_NONE:
                /* Timeout: redibuja para que avance el reloj. */
                break;

            default:
                break;
        }

        draw_entry(mode, mismatch);
    }
}

void metro_screen_lock_run_if_active(void)
{
    char entered[METRO_LOCK_PIN_LEN + 1];
    bool wrong = false;

    if (s_state != METRO_LOCK_ACTIVE)
        return;

    while (1)
    {
        if (!run_entry(LOCK_MODE_UNLOCK, entered, wrong))
            return; /* solo ocurre si el USB quitó la clave (arriba) */

        if (!strcmp(entered, metro_settings.screen_lock_pin))
        {
            s_state = METRO_LOCK_ARMED;
            return;
        }

        /* Sin límite de intentos ni retardo, a propósito y dicho en la
         * documentación: es un candado de interfaz. Reiniciar la captura
         * (con el rótulo cambiado) ES la señal de que falló -- mismo
         * lenguaje sin diálogos de error que el resto de Metro. */
        wrong = true;
    }
}

bool metro_screen_lock_setup(void)
{
    char first[METRO_LOCK_PIN_LEN + 1];
    char second[METRO_LOCK_PIN_LEN + 1];
    bool mismatch = false;

    while (1)
    {
        if (!run_entry(LOCK_MODE_SET, first, false))
            return false; /* MENU en el primer dígito: cancelado */

        if (!run_entry(LOCK_MODE_CONFIRM, second, mismatch))
            return false;

        if (!strcmp(first, second))
            break;

        mismatch = true; /* vuelve a pedirla desde cero */
    }

    metro_settings.screen_lock = true;
    strlcpy(metro_settings.screen_lock_pin, first,
             sizeof(metro_settings.screen_lock_pin));
    metro_settings_save();
    metro_settings_shared_write(); /* M-110: screen_lock_* es compartido (SS A) */

    /* ARMED, no ACTIVE: configurar la clave no debe bloquear el aparato
     * en ese mismo instante -- el dueño acaba de entrar a Ajustes, la
     * quiere para el próximo arranque. (Aura-Firmware llegó a este mismo
     * diseño después de reportes del dueño en las dos direcciones: con
     * un solo booleano, configurar la clave o no servía de nada o
     * bloqueaba de inmediato.) */
    s_state = METRO_LOCK_ARMED;
    return true;
}

/* --- M-104: sondeo del interruptor Hold ------------------------------ */

static bool s_hold_prev = false;
static long s_hold_since = 0;

/* Pantalla de bloqueo EN REPOSO: candado grande, reloj y bateria, sin
 * entrada de codigo. No es la de desbloquear -- mientras el Hold esta
 * puesto no hay nada que teclear, y mostrar cuatro casillas invitaria a
 * intentarlo. Es lo que el dueno ve al sacar el aparato del bolsillo:
 * la hora y cuanta bateria queda, que es exactamente para lo que uno lo
 * saca sin quitar el Hold. */
static void draw_resting(void)
{
    int w, h;
    const char *label = metro_lang_str(LANG_LOCK_RESTING);

    metro_draw_clear();
    /* Ceja vacia, igual que la pantalla de codigo: el reloj y la
     * bateria SI se dibujan (metro_draw_header() los pone siempre), y
     * el icono de candado tambien -- button_hold() es cierto aqui por
     * construccion. */
    metro_draw_header("");

    metro_widgets_draw_glyph(&metro_glyph_lock_large,
                              (LCD_WIDTH - metro_glyph_lock_large.width) / 2,
                              LOCK_RESTING_GLYPH_Y, metro_color_fg());

    lcd_setfont(metro_font_id(MFONT_TITLE));
    lcd_getstringsize((const unsigned char *)label, &w, &h);
    metro_draw_text(MFONT_TITLE, (LCD_WIDTH - w) / 2, LOCK_RESTING_LABEL_Y,
                     label, metro_color_secondary());
    lcd_update();
}

/* Corre mientras el Hold siga puesto. La retroiluminacion sigue su
 * temporizador normal a proposito (no se toca `backlight_*`): el
 * aparato esta guardado, no en uso.
 *
 * Sigue atendiendo SYS_EVENT -- USB y apagado tienen que funcionar con
 * el Hold puesto, y la salida de emergencia del candado (borrar las
 * claves del aura.cfg por USB) depende de que asi sea. */
static void run_resting(void)
{
    long last_draw;

    draw_resting();
    last_draw = current_tick;

    while (button_hold())
    {
        int action = metro_input_next(MCTX_LOCK, HZ / 10, NULL);

        if (action & SYS_EVENT)
        {
            if (action == SYS_USB_CONNECTED)
            {
                metro_screen_usb_show();
                default_event_handler(action);
                /* La sesion USB pudo haber quitado la clave del
                 * aura.cfg (salida de emergencia): se relee, igual que
                 * hace el bucle de la pantalla de codigo. */
                metro_settings_load();
                if (!metro_settings.screen_lock ||
                    !pin_is_valid(metro_settings.screen_lock_pin))
                    s_state = METRO_LOCK_NONE;
            }
            else
                default_event_handler(action);
            draw_resting();
            last_draw = current_tick;
            continue;
        }

        /* El sondeo del Hold corre a HZ/10 porque el flanco tiene que
         * notarse rapido, pero REDIBUJAR a 10 Hz una pantalla cuyo unico
         * contenido variable es el reloj (minutos) y la bateria seria
         * gastar bateria en no mostrar nada nuevo. Una vez por segundo
         * alcanza, y solo con la pantalla encendida: con la
         * retroiluminacion apagada -- que es lo normal en un aparato
         * guardado con el Hold puesto -- no se dibuja nada.
         * (`lcd_active()`, la misma puerta que el CLAUDE.md exige para
         * cualquier animacion.) */
        if (lcd_active() && current_tick - last_draw >= HZ)
        {
            draw_resting();
            last_draw = current_tick;
        }
    }
}

/* M-104 (addendum): la UNIDAD de los umbrales "tras 1 minuto" y "tras 5
 * minutos". En el aparato es un minuto, y punto.
 *
 * En el SIMULADOR son DOS SEGUNDOS. Los dos umbrales son la unica parte
 * de esta maquina de estados que no se podia ejercitar de punta a
 * punta: comprobarlos de verdad exigiria dejar el simulador corriendo
 * cinco minutos con el Hold puesto por cada uno de los cuatro casos
 * (soltar antes y despues, para cada umbral). Escalando la unidad, los
 * cuatro se capturan en segundos y lo que se prueba es exactamente el
 * mismo codigo: la comparacion, el origen `s_hold_since` y el flanco
 * que lo fija son identicos, solo cambia la constante. La aritmetica de
 * hardware queda intacta.
 *
 * DOS segundos y no uno: el inyector headless separa dos tokens
 * consecutivos por METRO_INJECT_WAIT_TICKS = 1 s
 * (uisimulator/common/sim_tasks.c), asi que con la unidad en 1 s el
 * caso "soltar ANTES del umbral" era inexpresable -- dos HOLD seguidos
 * ya duran exactamente el umbral. Con 2 s, "HOLD,HOLD" son 1 s (antes)
 * y "HOLD,WAIT,WAIT,HOLD" son 3 s (despues), y la razon 1:5 entre los
 * dos umbrales se conserva. */
#ifdef SIMULATOR
#define METRO_LOCK_HOLD_UNIT (2L * HZ)
#else
#define METRO_LOCK_HOLD_UNIT (60L * HZ)
#endif

static void apply_require_on_release(void)
{
    long held;

    if (s_state != METRO_LOCK_ARMED)
        return; /* sin clave, o ya bloqueado */

    held = current_tick - s_hold_since;

    switch (metro_settings.screen_lock_require)
    {
        case METRO_LOCK_REQUIRE_HOLD:
            s_state = METRO_LOCK_ACTIVE;
            break;
        case METRO_LOCK_REQUIRE_1MIN:
            if (held >= 1L * METRO_LOCK_HOLD_UNIT)
                s_state = METRO_LOCK_ACTIVE;
            break;
        case METRO_LOCK_REQUIRE_5MIN:
            if (held >= 5L * METRO_LOCK_HOLD_UNIT)
                s_state = METRO_LOCK_ACTIVE;
            break;
        case METRO_LOCK_REQUIRE_BOOT:
        default:
            break; /* el Hold no bloquea nunca */
    }
}

bool metro_screen_lock_poll_hold(void)
{
    bool hold = button_hold();

    if (hold == s_hold_prev)
        return false;

    s_hold_prev = hold;

    if (hold)
    {
        s_hold_since = current_tick;
        if (s_state != METRO_LOCK_NONE)
        {
            run_resting();
            /* run_resting() solo vuelve con el Hold ya quitado: el
             * flanco de bajada se resuelve aqui mismo, para que no
             * quede a merced de que la siguiente vuelta del bucle lo
             * vea antes que cualquier otra cosa. */
            s_hold_prev = false;
            apply_require_on_release();
        }
        return true;
    }

    apply_require_on_release();
    return true;
}

void metro_screen_lock_clear(void)
{
    metro_settings.screen_lock = false;
    metro_settings.screen_lock_pin[0] = '\0';
    metro_settings_save();
    metro_settings_shared_write(); /* M-110: screen_lock_* es compartido (SS A) */
    s_state = METRO_LOCK_NONE;
}

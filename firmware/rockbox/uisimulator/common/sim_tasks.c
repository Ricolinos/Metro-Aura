/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2009 by Jens Arnold
 * Copyright (C) 2011 by Thomas Martitz
 *
 * Rockbox simulator specific tasks
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

#include "config.h"
#include "kernel.h"
#include "screendump.h"
#include "thread.h"
#include "debug.h"
#include "usb.h"
#include "mv.h"
#include "ata_idle_notify.h"
#include "sim_tasks.h" /* Metro (M-039): sim_trigger_usb() prototype for
                           the USB_INSERT injection token below */

#ifdef WIN32
#include <windows.h>
#endif

/* Aura: automatizacion de entrada + screendump para evidencia headless
 * (sin permisos de Grabacion de Pantalla ni de Accesibilidad de macOS).
 * Activado por variables de entorno:
 *   METRO_SIM_AUTODUMP_TICKS=<n>  -> ticks a esperar antes de volcar
 *                                   la pantalla (desde el boot, o desde
 *                                   que termina de inyectar botones si
 *                                   METRO_SIM_BUTTONS esta presente)
 *   METRO_SIM_AUTODUMP_QUIT=1     -> sale del proceso tras el dump
 *   METRO_SIM_BUTTONS=SELECT,MENU,SCROLL_FWD,... -> inyecta esta secuencia
 *                                   de botones (uno de: SELECT, MENU,
 *                                   SCROLL_FWD, SCROLL_BACK, PLAY, LEFT,
 *                                   RIGHT) directamente en la cola de
 *                                   botones antes de tomar el dump.
 *                                   WAIT pausa ~1s sin postear boton;
 *                                   USB_INSERT llama sim_trigger_usb(true)
 *                                   (M-039, F9) en vez de un boton
 * Ver firmware/tools/sim_screenshot.sh */
#include <stdlib.h>
#include <string.h>
#include "string-extra.h"
#include "button.h"

static bool autodump_pending = false;
static bool autodump_quit = false;
static long autodump_tick = 0;
static long autodump_settle_ticks = 0;

/* Ampliado 2026-08-13 (de 32/256 a 128/2048): la profundidad real del
 * arbol de menus (Ajustes tiene 23 filas, Fecha y hora vive detras de
 * Ajustes->fila18->submenu->fila2) hacia rutinariamente que la
 * secuencia de botones necesaria para llegar a una pantalla profunda
 * desde el arranque excediera el limite viejo y se truncara en
 * silencio (varias verificaciones de esta sesion quedaron documentadas
 * como "solo por compilacion" exactamente por esto). Herramienta de
 * pruebas, solo compila en el simulador -- sin impacto de hardware. */
#define METRO_MAX_INJECT_BUTTONS 128
#define METRO_INJECT_RELEASE_GAP (HZ / 20)
#define METRO_INJECT_PRESS_GAP   (HZ / 4)
/* Token "WAIT" en METRO_SIM_BUTTONS: pausa de ~1s sin postear boton --
 * para secuencias deterministas a traves de transiciones sincronas
 * largas (coverflow_enter, flip, morph), donde el gap fijo de 250ms
 * dejaba botones cayendo dentro de la animacion y drenados por
 * drain_button_queue_if_full() de forma aleatoria. */
#define METRO_INJECT_WAIT_CODE  (-1L)
#define METRO_INJECT_WAIT_TICKS (HZ)
/* Token "USB_INSERT" en METRO_SIM_BUTTONS (Metro M-039, F9): simula
 * conectar el cable llamando sim_trigger_usb(true) directamente (el
 * mismo camino que dispara el menu interactivo del simulador) en vez
 * de postear un boton -- METRO_SIM_FORCE_USB de PLAN_MAESTRO.md F9 se
 * volvio innecesario una vez que quedo claro que reusar el mecanismo
 * de inyeccion existente alcanzaba. Util para F9-usb.png y cualquier
 * verificacion futura de metro_screen_usb.c sin depender de hardware
 * ni de la ventana SDL interactiva. */
#define METRO_INJECT_USB_CODE   (-2L)
/* Token "POWEROFF" (Metro M-039, F9): broadcasts SYS_POWEROFF directly
 * -- POWEROFF_BUTTON/POWEROFF_COUNT poweroff-by-held-button detection
 * lives in the button DRIVER (a sustained press the injector, which
 * only does press-then-release, has no way to simulate), so this
 * skips that and posts the event the driver would have posted, for
 * F9-shutdown.png and any future verification of code that reacts to
 * SYS_POWEROFF. */
#define METRO_INJECT_POWEROFF_CODE (-3L)
/* Token "HOLD" (Metro M-104): CONMUTA el interruptor Hold simulado, el
 * mismo estado que la tecla `h` de la ventana SDL
 * (firmware/target/hosted/sdl/button-sdl.c: `hold_button_state`). El
 * Hold del 6G no es un boton -- no se puede postear, se lee por sondeo
 * (`pmu_holdswitch_locked()` en el aparato, esta variable en el
 * simulador) -- asi que sin este token la maquina de estados del
 * bloqueo por Hold y el icono de candado de la barra solo se podian
 * verificar a mano en la ventana SDL. Mismo caracter que USB_INSERT
 * (M-039) y POWEROFF: tocar directo lo que el driver tocaria. */
#define METRO_INJECT_HOLD_CODE  (-4L)

/* Metro (M-101): sufijo "+HOLD" en cualquier nombre de boton de
 * METRO_SIM_BUTTONS (p.ej. "SELECT+HOLD") -- una pulsacion SOSTENIDA.
 * El injector solo sabia hacer press-then-release, asi que ningun gesto
 * "boton mantenido" de Metro (MENU held = ir al hub, SELECT held en el
 * reproductor = aleatorio, SELECT held en "acerca de" = revelar la
 * marca de agua de pila) se podia verificar sin la ventana SDL. Con el
 * sufijo, la secuencia posteada es press -> BUTTON_REPEAT -> BUTTON_REL,
 * que es exactamente lo que el driver del 6G produce cuando la
 * pulsacion pasa el umbral de repeticion: el BUTTON_REL posterior ya NO
 * casa con el mapeo corto, porque su prebutton exige que el ultimo
 * boton haya sido el codigo a secas. Herramienta de pruebas, solo
 * compila en el simulador. */
#define METRO_INJECT_HOLD_SUFFIX "+HOLD"

static long inject_codes[METRO_MAX_INJECT_BUTTONS];
static bool inject_hold[METRO_MAX_INJECT_BUTTONS];
static int inject_count = 0;
static int inject_pos = 0;
/* 0 = falta postear la pulsacion, 1 = falta el REPEAT (solo si es
 * sostenida), 2 = falta el REL. */
static int inject_phase = 0;
static long inject_next_tick = 0;

static long aura_button_name_to_code(const char *name)
{
    if (!strcmp(name, "SELECT"))      return BUTTON_SELECT;
    if (!strcmp(name, "MENU"))        return BUTTON_MENU;
    if (!strcmp(name, "SCROLL_FWD"))  return BUTTON_SCROLL_FWD;
    if (!strcmp(name, "SCROLL_BACK")) return BUTTON_SCROLL_BACK;
    if (!strcmp(name, "PLAY"))        return BUTTON_PLAY;
    if (!strcmp(name, "LEFT"))        return BUTTON_LEFT;
    if (!strcmp(name, "RIGHT"))       return BUTTON_RIGHT;
    if (!strcmp(name, "WAIT"))        return METRO_INJECT_WAIT_CODE;
    if (!strcmp(name, "USB_INSERT"))  return METRO_INJECT_USB_CODE;
    if (!strcmp(name, "POWEROFF"))    return METRO_INJECT_POWEROFF_CODE;
    if (!strcmp(name, "HOLD"))        return METRO_INJECT_HOLD_CODE;
    return BUTTON_NONE;
}

static void aura_parse_inject_buttons(const char *spec)
{
    char buf[2048];
    char *tok, *saveptr;

    strlcpy(buf, spec, sizeof(buf));
    tok = strtok_r(buf, ",", &saveptr);
    while (tok && inject_count < METRO_MAX_INJECT_BUTTONS)
    {
        /* Metro (M-101): "<BOTON>+HOLD" -> pulsacion sostenida. */
        size_t tlen = strlen(tok);
        size_t slen = sizeof(METRO_INJECT_HOLD_SUFFIX) - 1;
        bool hold = (tlen > slen &&
                     !strcmp(tok + tlen - slen, METRO_INJECT_HOLD_SUFFIX));
        long code;

        if (hold)
            tok[tlen - slen] = '\0';
        code = aura_button_name_to_code(tok);
        if (code != BUTTON_NONE)
        {
            inject_hold[inject_count] = hold && code > 0;
            inject_codes[inject_count++] = code;
        }
        tok = strtok_r(NULL, ",", &saveptr);
    }
}

static void sim_thread(void);
static long sim_thread_stack[DEFAULT_STACK_SIZE/sizeof(long)];
            /* stack isn't actually used in the sim */
static const char sim_thread_name[] = "sim";
static struct event_queue sim_queue;

/* possible events for the sim thread */
enum {
    SIM_SCREENDUMP,
    SIM_USB_INSERTED,
    SIM_USB_EXTRACTED,
#ifdef HAVE_HOTSWAP
    SIM_EXT_INSERTED,
    SIM_EXT_EXTRACTED,
#endif
};

#ifdef HAVE_HOTSWAP
extern void sim_ext_extracted(int drive);
#endif

void sim_thread(void)
{
    struct queue_event ev;
    long last_broadcast_tick = current_tick;
    int num_acks_to_expect = 0;

    while (1)
    {
        bool fine_poll = autodump_pending || (inject_pos < inject_count);
        /* Metro (M-069), R3-F8: HZ/10 -> HZ/50 mientras hay un dump o
         * botones pendientes. Con 10 ticks de resolucion, una animacion
         * de 24 ticks (8 cuadros x 3, el PUSH de animations=all) solo se
         * puede muestrear dos veces, y la primera cae ya pasado el
         * tercer cuadro -- imposible capturar el arranque de CONTINUUM,
         * que es justo el criterio de "hecho" de esa fase. A 2 ticks hay
         * ~12 muestras dentro de la misma animacion. Mismo caracter que
         * la ampliacion de METRO_MAX_INJECT_BUTTONS de mas arriba:
         * herramienta de pruebas, solo compila en el simulador, sin
         * impacto de hardware. */
        queue_wait_w_tmo(&sim_queue, &ev, fine_poll ? HZ/50 : 5*HZ);

        if (inject_pos < inject_count && TIME_AFTER(current_tick, inject_next_tick))
        {
            if (inject_codes[inject_pos] == METRO_INJECT_WAIT_CODE)
            {
                inject_pos++;
                inject_next_tick = current_tick + METRO_INJECT_WAIT_TICKS;
                if (inject_pos == inject_count && autodump_settle_ticks >= 0)
                {
                    autodump_pending = true;
                    autodump_tick = current_tick + METRO_INJECT_WAIT_TICKS + autodump_settle_ticks;
                }
            }
            else if (inject_codes[inject_pos] == METRO_INJECT_USB_CODE)
            {
                sim_trigger_usb(true);
                inject_pos++;
                inject_next_tick = current_tick + METRO_INJECT_WAIT_TICKS;
                if (inject_pos == inject_count && autodump_settle_ticks >= 0)
                {
                    autodump_pending = true;
                    autodump_tick = current_tick + METRO_INJECT_WAIT_TICKS + autodump_settle_ticks;
                }
            }
            else if (inject_codes[inject_pos] == METRO_INJECT_HOLD_CODE)
            {
                /* Metro (M-104): conmuta el mismo estado que la tecla
                 * `h` de la ventana SDL. */
#ifdef HAS_BUTTON_HOLD
                extern bool hold_button_state;
                hold_button_state = !hold_button_state;
#endif
                inject_pos++;
                inject_next_tick = current_tick + METRO_INJECT_WAIT_TICKS;
                if (inject_pos == inject_count && autodump_settle_ticks >= 0)
                {
                    autodump_pending = true;
                    autodump_tick = current_tick + METRO_INJECT_WAIT_TICKS + autodump_settle_ticks;
                }
            }
            else if (inject_codes[inject_pos] == METRO_INJECT_POWEROFF_CODE)
            {
                queue_broadcast(SYS_POWEROFF, 0);
                inject_pos++;
                inject_next_tick = current_tick + METRO_INJECT_WAIT_TICKS;
                if (inject_pos == inject_count && autodump_settle_ticks >= 0)
                {
                    autodump_pending = true;
                    autodump_tick = current_tick + METRO_INJECT_WAIT_TICKS + autodump_settle_ticks;
                }
            }
            else if (inject_phase == 0)
            {
                button_queue_post(inject_codes[inject_pos], 0);
                inject_phase = inject_hold[inject_pos] ? 1 : 2;
                inject_next_tick = current_tick + METRO_INJECT_RELEASE_GAP;
            }
            else if (inject_phase == 1)
            {
                /* Metro (M-101): el REPEAT que el driver postearia al
                 * pasar el umbral de repeticion de una pulsacion
                 * sostenida -- mismo criterio que el token POWEROFF
                 * (postear lo que el driver hubiera posteado). */
                button_queue_post(inject_codes[inject_pos] | BUTTON_REPEAT, 0);
                inject_phase = 2;
                inject_next_tick = current_tick + METRO_INJECT_RELEASE_GAP;
            }
            else
            {
                button_queue_post(inject_codes[inject_pos] | BUTTON_REL, 0);
                inject_phase = 0;
                inject_pos++;
                inject_next_tick = current_tick + METRO_INJECT_PRESS_GAP;

                if (inject_pos == inject_count && autodump_settle_ticks >= 0)
                {
                    autodump_pending = true;
                    autodump_tick = current_tick + autodump_settle_ticks;
                }
            }
        }

        if (autodump_pending && TIME_AFTER(current_tick, autodump_tick))
        {
            autodump_pending = false;
            screen_dump();
            if (autodump_quit)
                exit(0);
        }

        switch(ev.id)
        {
            case SYS_TIMEOUT:
                call_storage_idle_notifys(false);
                break;

            case SIM_SCREENDUMP:
                screen_dump();
#ifdef HAVE_REMOTE_LCD
                remote_screen_dump();
#endif
                break;
            case SIM_USB_INSERTED:
            /* from firmware/usb.c: */
                /* Tell all threads that they have to back off the storage.
                   We subtract one for our own thread. Expect an ACK for every
                   listener for each broadcast they received. If it has been too
                   long, the user might have entered a screen that didn't ACK
                   when inserting the cable, such as a debugging screen. In that
                   case, reset the count or else USB would be locked out until
                   rebooting because it most likely won't ever come. Simply
                   resetting to the most recent broadcast count is racy. */
                if(TIME_AFTER(current_tick, last_broadcast_tick + HZ*5))
                {
                    num_acks_to_expect = 0;
                    last_broadcast_tick = current_tick;
                }

                /* NOTE: Unlike the USB code, we do not subtract one here
                 * because the sim_queue is not registered for broadcasts! */
                num_acks_to_expect += queue_broadcast(SYS_USB_CONNECTED, 0);
                DEBUGF("USB inserted. Waiting for %d acks...\n",
                       num_acks_to_expect);
                break;
            case SYS_USB_CONNECTED_ACK:
                if(num_acks_to_expect > 0 && --num_acks_to_expect == 0)
                {
                    DEBUGF("All threads have acknowledged the connect.\n");
                }
                else
                {
                    DEBUGF("usb: got ack, %d to go...\n",
                           num_acks_to_expect);
                }
                break;
            case SIM_USB_EXTRACTED:
                /* in usb.c, this is only done for exclusive storage
                 * do it here anyway but don't depend on the acks */
                queue_broadcast(SYS_USB_DISCONNECTED, 0);
                break;
#ifdef HAVE_HOTSWAP
            case SIM_EXT_INSERTED:
            case SIM_EXT_EXTRACTED:
                sim_ext_extracted(ev.data);
                queue_broadcast(ev.id == SIM_EXT_INSERTED ?
                                SYS_HOTSWAP_INSERTED : SYS_HOTSWAP_EXTRACTED, 0);
                sleep(HZ/20);
                queue_broadcast(SYS_FS_CHANGED, 0);
                break;
#endif /* HAVE_MULTIDRIVE */
            default:
                DEBUGF("sim_tasks: unhandled event: %ld\n", ev.id);
                break;
        }
    }
}

void sim_tasks_init(void)
{
    const char *ticks_env = getenv("METRO_SIM_AUTODUMP_TICKS");
    const char *buttons_env = getenv("METRO_SIM_BUTTONS");

    queue_init(&sim_queue, false);

    if (buttons_env)
        aura_parse_inject_buttons(buttons_env);

    if (ticks_env || buttons_env)
    {
        autodump_settle_ticks = ticks_env ? atoi(ticks_env) : HZ / 2;
        autodump_quit = getenv("METRO_SIM_AUTODUMP_QUIT") != NULL;
    }

    if (inject_count > 0)
    {
        /* El primer boton se dispara enseguida; el dump se programa
         * cuando termine de inyectar todos (ver sim_thread()). */
        inject_next_tick = current_tick + METRO_INJECT_PRESS_GAP;
    }
    else if (ticks_env)
    {
        autodump_pending = true;
        autodump_tick = current_tick + autodump_settle_ticks;
    }

    create_thread(sim_thread, sim_thread_stack, sizeof(sim_thread_stack), 0,
                  sim_thread_name IF_PRIO(,PRIORITY_BACKGROUND) IF_COP(,CPU));
}

void sim_trigger_screendump(void)
{
    queue_post(&sim_queue, SIM_SCREENDUMP, 0);
}

#ifdef HAVE_HEADPHONE_DETECTION
static bool is_hp_inserted = true;
bool headphones_inserted(void)
{
    return is_hp_inserted;
}
void sim_trigger_hp(bool inserted)
{
    is_hp_inserted = inserted;
    DEBUGF("Headphone %s.\n", inserted ? "inserted":"removed");
}
#endif

#ifdef HAVE_LINEOUT_DETECTION
static bool is_lo_inserted = false;
bool lineout_inserted(void)
{
    return is_lo_inserted;
}
void sim_trigger_lo(bool inserted)
{
    is_lo_inserted = inserted;
    DEBUGF("Lineout %s.\n", inserted ? "inserted":"removed");
}
#endif

static bool is_usb_inserted;
void sim_trigger_usb(bool inserted)
{
    int usbmode = -1;
    if (inserted)
    {
        send_event(SYS_EVENT_USB_INSERTED, &usbmode);
        queue_post(&sim_queue, SIM_USB_INSERTED, 0);
    }
    else
    {
        send_event(SYS_EVENT_USB_EXTRACTED, NULL);
        queue_post(&sim_queue, SIM_USB_EXTRACTED, 0);
        DEBUGF("USB %s.\n", inserted ? "inserted":"removed");
    }
    is_usb_inserted = inserted;

}

int usb_detect(void)
{
    return is_usb_inserted ? USB_INSERTED : USB_EXTRACTED;
}

bool usb_inserted(void)
{
    return is_usb_inserted;
}

void usb_init(void)
{
}

void usb_start_monitoring(void)
{
}

void usb_acknowledge(long id, intptr_t seqnum)
{
    queue_post(&sim_queue, id, seqnum);
}

void usb_wait_for_disconnect(struct event_queue *q)
{
    struct queue_event ev;

    /* Don't return until we get SYS_USB_DISCONNECTED */
    while(1)
    {
        queue_wait(q, &ev);
        if(ev.id == SYS_USB_DISCONNECTED)
            return;
    }
}

static bool is_ext_inserted;

#ifdef HAVE_HOTSWAP
void sim_trigger_external(bool inserted)
{
    is_ext_inserted = inserted;

    int drive = 1; /* Can do others! */
    if (inserted)
        queue_post(&sim_queue, SIM_EXT_INSERTED, drive);
    else
        queue_post(&sim_queue, SIM_EXT_EXTRACTED, drive);

    DEBUGF("Ext %s\n", inserted ? "inserted":"removed");
}
#endif

bool hostfs_present(int drive)
{
    return drive > 0  ? is_ext_inserted : true;
}

bool hostfs_removable(int drive)
{
    return drive > 0;
}

#ifdef HAVE_HOTSWAP
bool volume_removable(int volume)
{
    /* volume == drive for now */
    return hostfs_removable(volume);
}

bool volume_present(int volume)
{
    /* volume == drive for now */
    return hostfs_present(volume);
}
#endif /* HAVE_HOTSWAP */

#ifdef HAVE_MULTIDRIVE
int volume_drive(int volume)
{
    /* volume == drive for now */
    return volume;
}
#endif

int volume_partition(int volume)
{
    (void)volume;
    /* Sims only implement a single parition per drive */
    return 0;
}

#if (CONFIG_STORAGE & STORAGE_MMC)
bool mmc_touched(void)
{
    return hostfs_present(1);
}
#endif

#ifdef CONFIG_STORAGE_MULTI
int hostfs_driver_type(int drive)
{
    /* Hack alert */
#if (CONFIG_STORAGE & STORAGE_ATA)
 #define SIMEXT1_TYPE_NUM   STORAGE_ATA_NUM
#elif (CONFIG_STORAGE & STORAGE_SD)
 #define SIMEXT1_TYPE_NUM   STORAGE_SD_NUM
#elif (CONFIG_STORAGE & STORAGE_MMC)
 #define SIMEXT1_TYPE_NUM   STORAGE_MMC_NUM
#elif (CONFIG_STORAGE & STORAGE_NAND)
 #define SIMEXT1_TYPE_NUM   STORAGE_NAND_NUM
#elif (CONFIG_STORAGE & STORAGE_RAMDISK)
 #define SIMEXT1_TYPE_NUM   STORAGE_RAMDISK_NUM
#elif (CONFIG_STORAGE & STORAGE_USB)
 #define SIMEXT1_TYPE_NUM   STORAGE_USB_NUM
#else
#error Unknown storage driver
#endif /* CONFIG_STORAGE */

    return drive > 0 ? SIMEXT1_TYPE_NUM : STORAGE_HOSTFS_NUM;
}
#endif /* CONFIG_STORAGE_MULTI */

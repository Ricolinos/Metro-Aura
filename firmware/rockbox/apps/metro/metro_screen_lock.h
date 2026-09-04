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
/* R3-F7/DD-8 (M-068): candado de INTERFAZ de 4 dígitos.
 *
 * **No es protección de datos, y la documentación lo dice con esas
 * palabras** (`docs/ESTADO_FINAL.md`): el volumen es FAT sin cifrar, así
 * que cualquiera con el cable lee el disco entero con o sin candado. Por
 * eso Metro, a diferencia de Aura-Firmware (D-238), **no** porta el
 * parche de ~28 líneas a `apps/misc.c` que difiere el montaje USB
 * mientras el candado está activo: diferir el montaje protegería la
 * interfaz, no los datos, y cuesta un archivo de core más un caso borde
 * no obvio de `seqnum`. Una conexión USB durante el bloqueo se atiende
 * normal, igual que en cualquier otro momento (ver `docs/DESVIACIONES.md`
 * R3-6) -- y eso es justamente lo que mantiene alcanzable la salida de
 * emergencia de abajo.
 *
 * Tres estados, como Aura: NONE (sin clave configurada), ARMED (clave
 * configurada, aparato desbloqueado en esta sesión) y ACTIVE (clave
 * configurada y bloqueando la interfaz ahora mismo). Se entra a ACTIVE
 * solo al arrancar; desbloquear pasa a ARMED y no se vuelve a bloquear
 * hasta el siguiente arranque.
 *
 * **Salida de emergencia** (parte del contrato de esta pantalla, no un
 * extra): la clave vive en texto plano en `.rockbox/aura/aura.cfg`
 * (claves `screen_lock` y `screen_lock_pin`) -- coherente con que el
 * candado sea de interfaz y no de datos. Si el dueño la olvida, conecta
 * por USB (posible incluso con el candado activo, ver arriba), borra esas
 * dos líneas y el aparato queda sin candado. Este módulo recarga los
 * ajustes desde disco al terminar cada sesión USB, así que la salida de
 * emergencia surte efecto **sin reiniciar**. Y si el valor guardado
 * estuviera corrupto (no exactamente 4 dígitos), este módulo falla
 * ABIERTO -- sin candado -- nunca cerrado: un archivo dañado no puede
 * dejar el aparato inservible.
 */
#ifndef METRO_SCREEN_LOCK_H
#define METRO_SCREEN_LOCK_H

#include <stdbool.h>

#define METRO_LOCK_PIN_LEN 4

enum metro_lock_state {
    METRO_LOCK_NONE = 0, /* sin clave configurada */
    METRO_LOCK_ARMED,    /* con clave, desbloqueado en esta sesión */
    METRO_LOCK_ACTIVE,   /* con clave, bloqueando la interfaz ahora */
};

/* Una vez al arrancar, después de metro_settings_load(): deja el estado
 * en ACTIVE si hay una clave válida guardada, NONE si no (incluyendo el
 * caso de clave corrupta -- falla abierto, ver la cabecera). */
void metro_screen_lock_init(void);

enum metro_lock_state metro_screen_lock_state(void);

/* El punto de interceptación. No hace nada salvo que el estado sea
 * ACTIVE; si lo es, corre su propio bucle de entrada -- igual que
 * metro_run_sync_screen_if_needed() y metro_widgets_confirm(), el
 * patrón de pantalla modal que Metro ya usaba -- y no vuelve hasta que
 * se teclea la clave correcta (o hasta que una sesión USB la borra del
 * aura.cfg). Al volver, el estado es ARMED. Llamarlo desde el bucle de
 * metro_main() ANTES de cualquier despacho de pantalla es lo que hace
 * que el candado alcance a todo el aparato y no solo a una pantalla. */
void metro_screen_lock_run_if_active(void);

/* M-104 (plan maestro SS D): sondeo del interruptor Hold, una vez por
 * vuelta del bucle principal, JUSTO ANTES de
 * metro_screen_lock_run_if_active().
 *
 * Por que sondeo y no un evento: en el 6G el Hold no genera eventos de
 * boton -- `pmu_holdswitch_locked()` es un registro que hay que leer.
 * El bucle de Metro ya espera con timeout (HZ/10, o HZ/20 mientras el
 * hub anima), asi que el sondeo cuesta una lectura por vuelta y el
 * flanco se nota en <= 100 ms; el plan pedia <= HZ/2.
 *
 * Que hace en cada flanco:
 *   OFF->ON  con bloqueo configurado: muestra la pantalla de bloqueo EN
 *            REPOSO (candado, reloj y bateria, sin entrada de codigo) y
 *            no vuelve hasta que se quita el Hold. Sin bloqueo
 *            configurado: nada, solo pide un redibujo para que aparezca
 *            el icono de la barra.
 *   ON->OFF  segun `screen_lock_require`: HOLD pide el codigo siempre;
 *            1MIN/5MIN solo si el Hold estuvo puesto al menos ese
 *            tiempo; BOOT nunca (el comportamiento anterior a M-104).
 *
 * Devuelve true si el llamador debe redibujar la pantalla actual -- el
 * icono de candado de la barra depende de este estado y no se refresca
 * solo. */
bool metro_screen_lock_poll_hold(void);

/* Fila de Ajustes: pide una clave nueva y su confirmación. true si
 * quedó configurada (estado ARMED), false si el usuario canceló con
 * MENU o si las dos capturas no coincidieron -- en ambos casos la
 * configuración previa queda intacta. */
bool metro_screen_lock_setup(void);

/* Fila de Ajustes: quita la clave (estado NONE) y la borra de
 * aura.cfg. Sin pedir la clave actual a propósito: para llegar a esta
 * fila el aparato ya está desbloqueado, así que exigirla otra vez no
 * agregaría ninguna protección real. */
void metro_screen_lock_clear(void);

#endif /* METRO_SCREEN_LOCK_H */

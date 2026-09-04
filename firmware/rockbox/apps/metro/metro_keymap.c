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
#include "metro_keymap.h"

/* HUB (root): wheel moves selection, SELECT enters. No pivots, no
 * Back target -- MENU/LEFT/RIGHT simply aren't mapped, so they
 * resolve to ACTION_NONE (matches PLAN_MAESTRO.md S2.3 "sin efecto"). */
static const struct button_mapping hub_mapping[] = {
    { MACT_PREV,   BUTTON_SCROLL_BACK,                BUTTON_NONE },
    { MACT_PREV,   BUTTON_SCROLL_BACK | BUTTON_REPEAT, BUTTON_NONE },
    { MACT_NEXT,   BUTTON_SCROLL_FWD,                  BUTTON_NONE },
    { MACT_NEXT,   BUTTON_SCROLL_FWD | BUTTON_REPEAT,  BUTTON_NONE },
    { MACT_SELECT, BUTTON_SELECT | BUTTON_REL,         BUTTON_SELECT },
    /* R4/FA-8 (M-071): PLAY controla la reproducción desde aquí también
     * -- lo que suena no depende de en qué pantalla estés. */
    { MACT_PLAYPAUSE, BUTTON_PLAY | BUTTON_REL,        BUTTON_PLAY },
    LAST_ITEM_IN_LIST
};

/* LIST (any pivoted page, including Settings): wheel moves selection,
 * LEFT/RIGHT twist between pivots (no wrap, handled in metro_nav.c),
 * SELECT enters/activates, MENU goes back, MENU held jumps to the
 * hub. */
static const struct button_mapping list_mapping[] = {
    { MACT_PREV,       BUTTON_SCROLL_BACK,                BUTTON_NONE },
    { MACT_PREV,       BUTTON_SCROLL_BACK | BUTTON_REPEAT, BUTTON_NONE },
    { MACT_NEXT,       BUTTON_SCROLL_FWD,                  BUTTON_NONE },
    { MACT_NEXT,       BUTTON_SCROLL_FWD | BUTTON_REPEAT,  BUTTON_NONE },
    { MACT_PIVOT_PREV, BUTTON_LEFT  | BUTTON_REL,          BUTTON_LEFT  },
    { MACT_PIVOT_NEXT, BUTTON_RIGHT | BUTTON_REL,          BUTTON_RIGHT },
    { MACT_SELECT,     BUTTON_SELECT | BUTTON_REL,         BUTTON_SELECT },
    /* M-101: SELECT sostenido. Solo lo consume el pivot que declare
     * on_select_hold (hoy, "acerca de"); en cualquier otro la accion
     * llega a metro_screen_list_handle() y no hace nada. */
    { MACT_SELECT_HOLD, BUTTON_SELECT | BUTTON_REPEAT,     BUTTON_NONE },
    { MACT_BACK,       BUTTON_MENU | BUTTON_REL,           BUTTON_MENU },
    { MACT_HOME,       BUTTON_MENU | BUTTON_REPEAT,        BUTTON_NONE },
    /* R4/FA-8 (M-071): igual que en el hub. Deliberadamente NO se
     * agrega a dialog_mapping[] (un diálogo modal de sí/no debe seguir
     * enfocado en su propia pregunta) ni a lock_mapping[] (con el
     * candado puesto el aparato no ofrece ningún control -- M-068). */
    { MACT_PLAYPAUSE,  BUTTON_PLAY | BUTTON_REL,           BUTTON_PLAY },
    LAST_ITEM_IN_LIST
};

/* DIALOG (yes/no confirm): wheel toggles the choice, SELECT confirms,
 * MENU cancels -- PLAN_MAESTRO.md S2.3. */
static const struct button_mapping dialog_mapping[] = {
    { MACT_PREV,   BUTTON_SCROLL_BACK,                BUTTON_NONE },
    { MACT_NEXT,   BUTTON_SCROLL_FWD,                  BUTTON_NONE },
    { MACT_SELECT, BUTTON_SELECT | BUTTON_REL,         BUTTON_SELECT },
    { MACT_BACK,   BUTTON_MENU | BUTTON_REL,           BUTTON_MENU },
    LAST_ITEM_IN_LIST
};

/* PLAYER (Now Playing, F5): wheel is volume, not selection -- there is
 * nothing to scroll on this screen. LEFT/RIGHT short = track skip,
 * held = seek. SELECT short pushes the options page; held toggles
 * shuffle in place, no page change. PLAY is play/pause -- MENU|REPEAT
 * (home) falls through to hub_mapping's absence here on purpose, same
 * "not mapped -> ACTION_NONE" rule as HUB's LEFT/RIGHT (S2.3 doesn't
 * give Now Playing a "jump to hub" gesture of its own). */
static const struct button_mapping player_mapping[] = {
    { MACT_VOL_DOWN,    BUTTON_SCROLL_BACK,                BUTTON_NONE },
    { MACT_VOL_DOWN,    BUTTON_SCROLL_BACK | BUTTON_REPEAT, BUTTON_NONE },
    { MACT_VOL_UP,      BUTTON_SCROLL_FWD,                  BUTTON_NONE },
    { MACT_VOL_UP,      BUTTON_SCROLL_FWD | BUTTON_REPEAT,  BUTTON_NONE },
    { MACT_TRACK_PREV,  BUTTON_LEFT  | BUTTON_REL,          BUTTON_LEFT  },
    { MACT_TRACK_NEXT,  BUTTON_RIGHT | BUTTON_REL,          BUTTON_RIGHT },
    { MACT_SEEK_BACK,   BUTTON_LEFT  | BUTTON_REPEAT,       BUTTON_NONE },
    { MACT_SEEK_FWD,    BUTTON_RIGHT | BUTTON_REPEAT,       BUTTON_NONE },
    { MACT_OPTIONS,     BUTTON_SELECT | BUTTON_REL,         BUTTON_SELECT },
    { MACT_TOGGLE_SHUFFLE, BUTTON_SELECT | BUTTON_REPEAT,   BUTTON_NONE },
    { MACT_BACK,        BUTTON_MENU | BUTTON_REL,           BUTTON_MENU },
    { MACT_PLAYPAUSE,   BUTTON_PLAY | BUTTON_REL,           BUTTON_PLAY },
    LAST_ITEM_IN_LIST
};

/* VIEWER (R2-F3, DD-10): wheel moves through the same pivot/category
 * the user entered from (prev/next photo, reusing MACT_PREV/MACT_NEXT
 * -- same meaning as everywhere else). SELECT toggles fit<->cover.
 * MENU/MENU-held behave exactly like LIST (back to the grid / home).
 * PLAY still controls global playback -- music keeps playing while
 * viewing a photo. */
static const struct button_mapping viewer_mapping[] = {
    { MACT_PREV,             BUTTON_SCROLL_BACK,        BUTTON_NONE },
    { MACT_NEXT,             BUTTON_SCROLL_FWD,          BUTTON_NONE },
    /* M-106 (plan maestro, Fase 4 del plan hijo): LEFT/RIGHT tambien
     * pasan de foto. La rueda ya lo hacia, pero en un visor a pantalla
     * completa el gesto que la mano espera es el del eje horizontal --
     * es el mismo que en el reproductor pasa de pista. Se mapean a las
     * MISMAS acciones que la rueda: pasar de foto es una sola operacion,
     * no dos, y duplicar la accion duplicaria el sitio donde equivocarse.
     * En el visor no hay pivots que torcer, asi que LEFT/RIGHT estaban
     * libres. */
    { MACT_PREV,             BUTTON_LEFT  | BUTTON_REL,  BUTTON_LEFT  },
    { MACT_NEXT,             BUTTON_RIGHT | BUTTON_REL,  BUTTON_RIGHT },
    { MACT_TOGGLE_VIEW_MODE, BUTTON_SELECT | BUTTON_REL, BUTTON_SELECT },
    { MACT_BACK,             BUTTON_MENU | BUTTON_REL,   BUTTON_MENU },
    { MACT_HOME,             BUTTON_MENU | BUTTON_REPEAT, BUTTON_NONE },
    { MACT_PLAYPAUSE,        BUTTON_PLAY | BUTTON_REL,   BUTTON_PLAY },
    LAST_ITEM_IN_LIST
};

/* LOCK (R3-F7/DD-8): captura de 4 dígitos. La rueda marca el dígito
 * enfocado (adelante/atrás, con vuelta), SELECT lo confirma y avanza,
 * MENU retrocede uno. LEFT/RIGHT/PLAY quedan SIN mapear a propósito --
 * no es que "no hagan falta": con el candado activo el aparato no debe
 * ofrecer NINGÚN control, ni siquiera play/pausa, y no mapearlos es lo
 * que garantiza que resuelvan a ACTION_NONE en vez de caer a otra
 * tabla. Sin MACT_HOME por la misma razón. */
static const struct button_mapping lock_mapping[] = {
    { MACT_PREV,   BUTTON_SCROLL_BACK,                 BUTTON_NONE },
    { MACT_PREV,   BUTTON_SCROLL_BACK | BUTTON_REPEAT, BUTTON_NONE },
    { MACT_NEXT,   BUTTON_SCROLL_FWD,                  BUTTON_NONE },
    { MACT_NEXT,   BUTTON_SCROLL_FWD | BUTTON_REPEAT,  BUTTON_NONE },
    { MACT_SELECT, BUTTON_SELECT | BUTTON_REL,         BUTTON_SELECT },
    { MACT_BACK,   BUTTON_MENU | BUTTON_REL,           BUTTON_MENU },
    LAST_ITEM_IN_LIST
};

const struct button_mapping *metro_keymap_get_context_map(int context)
{
    switch (context & ~CONTEXT_PLUGIN)
    {
        case MCTX_HUB:    return hub_mapping;
        case MCTX_LIST:   return list_mapping;
        case MCTX_DIALOG: return dialog_mapping;
        case MCTX_PLAYER: return player_mapping;
        case MCTX_VIEWER: return viewer_mapping;
        case MCTX_LOCK:   return lock_mapping;
        default:          return NULL;
    }
}

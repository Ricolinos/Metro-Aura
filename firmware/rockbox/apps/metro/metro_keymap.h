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
#ifndef METRO_KEYMAP_H
#define METRO_KEYMAP_H

#include "action.h"

/* Metro's own action space and keymap tables -- see DECISIONS.md M-007
 * and PLAN_MAESTRO.md S2.3 for the full gesture table (this file only
 * implements the contexts F3 needs: HUB, LIST, DIALOG -- PLAYER/OPTIONS
 * land with Now Playing in F5).
 *
 * Values start at LAST_ACTION_PLACEHOLDER+1 (apps/action.h), the core's
 * own mechanism for custom action codes that can never collide with
 * the core's ACTION_STD_ or ACTION_WPS_ ones. Every metro_keymap
 * mapping table ends
 * with LAST_ITEM_IN_LIST (not the __NEXTLIST variant) so an unmatched
 * button resolves to ACTION_NONE instead of falling through to
 * CONTEXT_STD -- Metro's action space is fully disjoint from the
 * core's, there is nothing useful to chain to. */
enum metro_action {
    MACT_PREV = LAST_ACTION_PLACEHOLDER + 1, /* wheel back: move selection up */
    MACT_NEXT,                                /* wheel fwd: move selection down */
    MACT_PIVOT_PREV,                          /* LEFT (short): twist to the previous pivot */
    MACT_PIVOT_NEXT,                          /* RIGHT (short): twist to the next pivot */
    MACT_SELECT,                              /* SELECT (short): enter/activate */
    MACT_BACK,                                /* MENU (short): go back one level */
    MACT_HOME,                                /* MENU (held): go straight to the hub */

    /* F5: PLAYER context only (Now Playing) -- PLAN_MAESTRO.md S2.3. */
    MACT_VOL_UP,                               /* wheel fwd: volume up */
    MACT_VOL_DOWN,                             /* wheel back: volume down */
    MACT_TRACK_PREV,                           /* LEFT (short): previous track */
    MACT_TRACK_NEXT,                           /* RIGHT (short): next track */
    MACT_SEEK_BACK,                            /* LEFT (held): rewind */
    MACT_SEEK_FWD,                             /* RIGHT (held): fast-forward */
    MACT_OPTIONS,                              /* SELECT (short): push the options page */
    MACT_TOGGLE_SHUFFLE,                       /* SELECT (held): toggle shuffle */
    MACT_PLAYPAUSE,                            /* PLAY (short): audio_pause()/audio_resume() */

    /* R2-F3: VIEWER context only (photo viewer) -- PLAN-metro-r2-maestro.md
     * DD-10. MACT_PREV/MACT_NEXT (already declared above) are reused
     * for previous/next photo -- same "wheel moves through a
     * collection" meaning they already have everywhere else; MACT_BACK/
     * MACT_HOME/MACT_PLAYPAUSE are reused too (back to the grid, home,
     * play/pause). Only the fit/cover toggle needs a new action. */
    MACT_TOGGLE_VIEW_MODE,                     /* SELECT (short): fit <-> cover */

    /* M-101: LIST context only -- SELECT held. Today its only consumer
     * is the About pivot, which uses it to reveal the main-stack
     * watermark row. Same short/held pair the player already proves
     * with MACT_OPTIONS/MACT_TOGGLE_SHUFFLE: the REL that follows a
     * REPEAT no longer matches the short mapping, because that
     * mapping's prebutton demands the last button to have been a plain
     * BUTTON_SELECT. */
    MACT_SELECT_HOLD,
};

#define MACT_NONE ACTION_NONE

/* Small ints -- always OR with CONTEXT_PLUGIN before passing to
 * get_custom_action(); see metro_input.c and action_code_lookup()'s
 * "context & CONTEXT_PLUGIN" check (apps/action.c) for why: without
 * that bit the core looks up its OWN table instead of ever calling
 * metro_keymap_get_context_map(). */
enum metro_context {
    MCTX_HUB = 0,
    MCTX_LIST,
    MCTX_DIALOG,
    MCTX_PLAYER,
    MCTX_VIEWER, /* R2-F3: photo viewer (DD-10) */
    MCTX_LOCK,   /* R3-F7: PIN entry on the screen lock (DD-8) */
};

/* get_context_map callback for get_custom_action() -- pass this
 * directly, never call it yourself. */
const struct button_mapping *metro_keymap_get_context_map(int context);

#endif /* METRO_KEYMAP_H */

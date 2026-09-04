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
/* M-097 (contract v16): background builder of the shared master art
 * cache. From boot, without any screen, on its own low-priority
 * Rockbox thread: walks albums -> artists -> photos and creates every
 * missing /.aura/art/<kind>/<key>.art (or .none), one element at a
 * time, sleeping between elements, so that by the time the user opens
 * a grid (or switches firmware family) nothing has to decode a JPEG.
 *
 * Yields to the user: it does nothing until >= 2s have passed since
 * the last input (metro_master_art_builder_note_input()), while a
 * transition/animation is running (metro_master_art_builder_pause()),
 * while a sync job runs, and while playback is low on buffer. Each
 * element runs under the master-art lock (metro_master_art.h) so a
 * UI-thread decode/tagcache walk never interleaves with it; pause()
 * takes that lock once to wait out the element in flight. */
#ifndef METRO_MASTER_ART_BUILDER_H
#define METRO_MASTER_ART_BUILDER_H

#include <stdbool.h>

/* M-100: phase of the pass, for the "updating library" screen. Same
 * order as the walk (albums -> artists -> photos). */
typedef enum {
    METRO_MASTER_ART_PHASE_IDLE = 0,
    METRO_MASTER_ART_PHASE_ALBUMS,
    METRO_MASTER_ART_PHASE_ARTISTS,
    METRO_MASTER_ART_PHASE_PHOTOS,
} metro_master_art_phase_t;

/* Creates the thread (once, from metro_main() after the disk handoff)
 * and requests the first pass. */
void metro_master_art_builder_init(void);

/* true: finish the element in flight and stop; false: resume. Called
 * around every transition by metro_transitions.c. */
void metro_master_art_builder_pause(bool paused);

/* Any user action -- restarts the 2s idle window. */
void metro_master_art_builder_note_input(void);

/* Asks for another full pass (a sync finished, the database was
 * rebuilt -- metro_thumbs_mark_dirty() calls this). Cheap: a flag. */
void metro_master_art_builder_request_pass(void);

/* M-100 (owner's request: "actualizar biblioteca"). An EXPLICIT
 * preparation -- the manual one from Settings, the marker from a Studio
 * sync, the first boot after a firmware update -- finishes the image
 * pass BEFORE handing control back, with its progress on the same
 * "updating library" screen. Only metro_sync.c calls these four.
 *
 * Foreground differs from the normal background pass in the gates it
 * skips, which matters here: may_run() normally refuses while a sync
 * job is active, while a transition is paused, and until 2s of user
 * idle -- during an explicit preparation ALL THREE would be true at
 * once (the user just pressed a button and the sync state machine is
 * what asked for the pass), so the builder would never move and the
 * screen would wait forever. The audio gate is NOT skipped: if music
 * is playing, the disk is its. */
void metro_master_art_builder_set_foreground(bool foreground);

/* Progress of the current pass. `total` 0 = not known yet (photos walk
 * the directory in streaming). false if no pass is running. */
bool metro_master_art_builder_progress(metro_master_art_phase_t *phase,
                                        int *done, int *total);

/* true once a COMPLETE pass (all three phases, uninterrupted) finished
 * since the last begin_full_pass(). */
bool metro_master_art_builder_pass_done(void);

/* true if the thread exists right now -- so the preparation screen
 * never waits for a pass that can never arrive. */
bool metro_master_art_builder_is_running(void);

/* Restarts the pass from the beginning of albums (a preparation must
 * walk EVERYTHING, not continue a half-done pass). */
void metro_master_art_builder_begin_full_pass(void);

#endif /* METRO_MASTER_ART_BUILDER_H */

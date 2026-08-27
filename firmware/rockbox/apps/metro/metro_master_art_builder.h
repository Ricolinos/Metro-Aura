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

#endif /* METRO_MASTER_ART_BUILDER_H */

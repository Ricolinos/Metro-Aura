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
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "thread.h"
#include "version.h"

#include "metro_screen_about.h"
#include "metro_device.h"
#include "metro_manifest.h"
#include "metro_lang.h"
#include "metro_screen_text.h" /* M-103: avisos legales */

/* M-101: marca de agua de la pila del hilo principal (plan maestro de
 * la ronda homologacion, seccion E.4). No hay forma de que el dueno
 * confirme en el iPod que el aumento de 8 -> 12 KB de app.lds alcanza,
 * salvo esperar otro panic; esta fila lo dice sin esperar nada.
 *
 * Se apoya en thread_get_debug_info(), la MISMA API publica que usa el
 * menu de depuracion de Rockbox (apps/debug_menu.c:184) -- no se copia
 * su lectura de la marca DEADBEEF, que vive en un header privado del
 * kernel (firmware/kernel/thread-internal.h). El tamano total sale de
 * los simbolos del enlazador, que son exactamente los que
 * __get_main_stack() le entrega al hilo principal en init_threads().
 *
 * En el simulador la pila del hilo principal es la del hilo de SDL, que
 * el sistema operativo administra: Rockbox ni siquiera compila los
 * campos (struct thread_debug_info los envuelve en #ifndef
 * HAVE_SDL_THREADS). Ahi la fila existe siempre -- para poder
 * capturarla -- pero dice "n/d en el simulador". */
#ifndef HAVE_SDL_THREADS
#define METRO_ABOUT_HAS_STACK_WATERMARK 1
#endif

/* La fila esta OCULTA por defecto en hardware: se revela con SELECT
 * sostenido sobre la fila de version y se vuelve a ocultar con otro.
 * En el simulador arranca visible porque no hay gesto sostenido comodo
 * en la ventana SDL y porque es la unica forma de capturarla. */
#ifdef SIMULATOR
static bool s_show_stack = true;
#else
static bool s_show_stack = false;
#endif

/* Row layout: device name, then either "based on rockbox" straight
 * after 1 "not synced yet" row, or (if sync_summary.cfg exists, R2-F1/
 * DD-5, M-055) music/video/photo/playlist counts followed by whichever
 * category breakdown rows the manifest actually carries (0, 3, or 6
 * extra rows depending on has_video_categories/has_photo_categories --
 * a manifest written before Studio added category breakdown has
 * neither), then "based on rockbox". */

static int synced_row_count(const metro_manifest_t *m)
{
    int n = 4; /* music, video, photo, playlists */
    if (m->has_video_categories)
        n += 3; /* movies, series, clips */
    if (m->has_photo_categories)
        n += 3; /* images, photos, ai */
    return n;
}

/* R5-F1 (M-081): both providers run per FRAME (the pivot slide redraws
 * every row each tick), so they read the RAM copy that
 * metro_disk_handoff() refreshes -- never the disk. The previous
 * metro_manifest_load() here (open + parse + close of sync_summary.cfg,
 * once per row per frame) is what wedged the real iPod on entering
 * About while the simulator, backed by the host filesystem, showed
 * nothing wrong. */
static int about_count(void *ctx)
{
    const metro_manifest_t *m = metro_manifest_cached();
    (void)ctx;
    /* +1 por la fila de avisos legales (M-103), que va SIEMPRE al final
     * -- después de la de versión y, si está revelada, de la de pila. */
    return 3 + (m ? synced_row_count(m) : 1) + (s_show_stack ? 1 : 0);
}

/* Indice de la fila de version ("basado en rockbox"). */
static int version_row_index(void)
{
    const metro_manifest_t *m = metro_manifest_cached();

    return 1 + (m ? synced_row_count(m) : 1);
}

/* M-103: "avisos legales" es SIEMPRE la ultima fila, este o no
 * revelada la de pila entre ella y la de version. */
static int legal_row_index(void)
{
    return version_row_index() + (s_show_stack ? 2 : 1);
}

/* M-101: "62 % de 12 KB". El porcentaje es la marca de agua real (el
 * maximo que el hilo llego a usar), no el uso instantaneo. */
static const char *stack_subtitle(char *buf, size_t bufsz)
{
#ifdef METRO_ABOUT_HAS_STACK_WATERMARK
    extern uintptr_t stackbegin[];
    extern uintptr_t stackend[];
    struct thread_debug_info info;
    size_t total = (uintptr_t)stackend - (uintptr_t)stackbegin;

    if (thread_get_debug_info(thread_self(), &info) > 0 && total > 0)
    {
        snprintf(buf, bufsz, "%u %% de %u KB",
                 info.stack_usage, (unsigned)(total / 1024));
        return buf;
    }
#endif
    (void)buf;
    (void)bufsz;
    return metro_lang_str(LANG_ABOUT_STACK_NA);
}

static void about_get_row(void *ctx, int index, struct metro_row *out)
{
    static char buf[64];
    const metro_manifest_t *mp = metro_manifest_cached();
    metro_manifest_t m;
    bool synced = (mp != NULL);
    const char *name;

    (void)ctx;
    out->subtitle = NULL;
    out->kind = METRO_ROW_ACTION;

    if (index == 0)
    {
        name = metro_device_name();
        out->title = name ? name : metro_lang_str(LANG_ABOUT_DEVICE_DEFAULT);
        return;
    }

    if (synced)
        m = *mp;

    if (!synced)
    {
        if (index == 1)
        {
            out->title = metro_lang_str(LANG_ABOUT_NOT_SYNCED);
            return;
        }
    }
    else
    {
        /* R2-F1/DD-5 (M-055): row order mirrors Aura-Firmware's own
         * About screen (consulted read-only, aura_screens.c) -- top-level
         * counts, then playlists, then video breakdown, then photo
         * breakdown, each breakdown only if the manifest actually
         * carries it. */
        int row = 1;

        if (index == row++)
        {
            snprintf(buf, sizeof(buf), "%d %s", m.music_count, metro_lang_str(LANG_ABOUT_SONGS));
            out->title = buf;
            return;
        }
        if (index == row++)
        {
            snprintf(buf, sizeof(buf), "%d %s", m.video_count, metro_lang_str(LANG_HUB_VIDEOS));
            out->title = buf;
            return;
        }
        if (index == row++)
        {
            snprintf(buf, sizeof(buf), "%d %s", m.photo_count, metro_lang_str(LANG_HUB_PHOTOS));
            out->title = buf;
            return;
        }
        if (index == row++)
        {
            snprintf(buf, sizeof(buf), "%d %s", m.playlist_count, metro_lang_str(LANG_ABOUT_PLAYLISTS));
            out->title = buf;
            return;
        }
        if (m.has_video_categories)
        {
            if (index == row++)
            {
                snprintf(buf, sizeof(buf), "%d %s", m.video_movies_count, metro_lang_str(LANG_ABOUT_MOVIES));
                out->title = buf;
                return;
            }
            if (index == row++)
            {
                snprintf(buf, sizeof(buf), "%d %s", m.video_series_count, metro_lang_str(LANG_ABOUT_SERIES));
                out->title = buf;
                return;
            }
            if (index == row++)
            {
                snprintf(buf, sizeof(buf), "%d %s", m.video_clips_count, metro_lang_str(LANG_ABOUT_CLIPS));
                out->title = buf;
                return;
            }
        }
        if (m.has_photo_categories)
        {
            if (index == row++)
            {
                snprintf(buf, sizeof(buf), "%d %s", m.photo_images_count, metro_lang_str(LANG_ABOUT_IMAGES));
                out->title = buf;
                return;
            }
            if (index == row++)
            {
                snprintf(buf, sizeof(buf), "%d %s", m.photo_photos_count, metro_lang_str(LANG_ABOUT_PHOTOS_TAKEN));
                out->title = buf;
                return;
            }
            if (index == row++)
            {
                snprintf(buf, sizeof(buf), "%d %s", m.photo_ai_count, metro_lang_str(LANG_ABOUT_AI));
                out->title = buf;
                return;
            }
        }
    }

    if (index == legal_row_index())
    {
        /* M-103 (plan maestro SS C): la GPL v2 SS3 pide que el aviso de
         * licencia esté a la vista del usuario, no solo en el
         * repositorio. Va en "acerca de" -- que es donde alguien busca
         * licencias -- y es la única fila accionable de este pivot. */
        out->title = metro_lang_str(LANG_SETTING_LEGAL);
        out->kind = METRO_ROW_NAV;
        return;
    }

    if (s_show_stack && index > version_row_index())
    {
        static char stackbuf[32];

        out->title = metro_lang_str(LANG_ABOUT_STACK);
        out->subtitle = stack_subtitle(stackbuf, sizeof(stackbuf));
        return;
    }

    /* M-101: la version del build va de subtitulo. Es la fila sobre la
     * que el gesto sostenido revela la pila, asi que tiene que decir
     * QUE version esta corriendo -- si no, la marca de agua que reporte
     * el dueno no se puede atribuir a un binario concreto. */
    out->title = metro_lang_str(LANG_ABOUT_BASED_ON_ROCKBOX);
    out->subtitle = rbversion;
}

static void about_on_select(void *ctx, int index)
{
    (void)ctx;

    if (index == legal_row_index())
        metro_screen_text_show(metro_lang_str(LANG_SETTING_LEGAL),
                                metro_lang_str(LANG_LEGAL_BODY));
}

/* M-101: SELECT sostenido sobre la fila de version revela/oculta la
 * marca de agua de la pila. Sobre cualquier otra fila no hace nada --
 * un gesto oculto no debe disparar desde donde el usuario no lo espera. */
static void about_on_select_hold(void *ctx, int index)
{
    (void)ctx;

    if (index == version_row_index())
        s_show_stack = !s_show_stack;
}

const struct metro_pivot metro_screen_about_pivot = {
    .name = LANG_PIVOT_ABOUT, .count = about_count, .get_row = about_get_row,
    .on_select = about_on_select, .on_select_hold = about_on_select_hold
};

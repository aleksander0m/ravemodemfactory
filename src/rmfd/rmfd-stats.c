/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 * rmfd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2015 Zodiac Inflight Innovations
 *
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#include <glib.h>
#include <stdio.h>
#include <errno.h>

#include "rmfd-stats.h"

static FILE *stats_file;

void
rmfd_stats_setup (const gchar *path)
{
    g_assert (!stats_file);

    errno = 0;
    if (!(stats_file = fopen (path, "a")))
        g_warning ("error: cannot open stats file: %s", g_strerror (errno));
}

void
rmfd_stats_teardown (void)
{
    if (stats_file) {
        fclose (stats_file);
        stats_file = NULL;
    }
}

gboolean
rmfd_stats_enabled (void)
{
    return !!stats_file;
}

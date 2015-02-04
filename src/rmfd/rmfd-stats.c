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

static FILE      *stats_file;
static GDateTime *start_system_time;
static time_t     start_time;

static void
write_record (gchar      record_type,
              GDateTime *first_system_time,
              time_t     first_time,
              GDateTime *second_system_time,
              time_t     second_time,
              guint64    rx_bytes,
              guint64    tx_bytes)
{
    gchar *first_system_time_str;
    gchar *second_system_time_str;

    g_assert (record_type == 'P' || record_type == 'F');

    first_system_time_str  = first_system_time  ? g_date_time_format (first_system_time, "%F %T")  : g_strdup ("N/A");
    second_system_time_str = second_system_time ? g_date_time_format (second_system_time, "%F %T") : g_strdup ("N/A");

    if (fprintf (stats_file, "%c\t%s\t%lu\t%s\t%lu\t%" G_GUINT64_FORMAT "\t%" G_GUINT64_FORMAT "\n",
                 record_type,
                 first_system_time_str,
                 (gulong) first_time,
                 second_system_time_str,
                 (gulong) second_time,
                 rx_bytes,
                 tx_bytes) < 0)
        g_warning ("error: cannot write to stats file: %s", g_strerror (ferror (stats_file)));

    g_free (first_system_time_str);
    g_free (second_system_time_str);
}

void
rmfd_stats_start (GDateTime *system_time)
{
    /* Keep track of when this was started */
    if (start_system_time)
        g_date_time_unref (start_system_time);
    start_system_time = system_time ? g_date_time_ref (system_time) : NULL;
    start_time = time (NULL);

    write_record ('S', start_system_time, start_time, start_system_time, start_time, 0, 0);
}

void
rmfd_stats_tmp (GDateTime *tmp_system_time,
                guint64    rx_bytes,
                guint64    tx_bytes)
{
    write_record ('T', start_system_time, start_time, tmp_system_time, time (NULL), rx_bytes, tx_bytes);
}

void
rmfd_stats_stop (GDateTime *stop_system_time,
                 guint64    rx_bytes,
                 guint64    tx_bytes)
{
    /* If for any reason stop is called multiple times, don't write multiple final records */
    if (!start_system_time)
        return;

    write_record ('F', start_system_time, start_time, stop_system_time, time (NULL), rx_bytes, tx_bytes);

    /* Cleanup start time */
    if (start_system_time)
        g_date_time_unref (start_system_time);
    start_system_time =NULL;
    start_time = 0;
}

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

    if (start_system_time) {
        g_date_time_unref (start_system_time);
        start_system_time = NULL;
    }

    start_time = 0;
}

gboolean
rmfd_stats_enabled (void)
{
    return !!stats_file;
}

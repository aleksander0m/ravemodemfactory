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
#include <string.h>
#include <errno.h>

#include "rmfd-stats.h"
#include "rmfd-syslog.h"

static FILE      *stats_file;
static gchar     *stats_file_path;
static GDateTime *start_system_time;
static time_t     start_time;

#define MAX_LINE_LENGTH 255

/******************************************************************************/
/* Build date string to show */

static gchar *
common_build_date_string (GDateTime *system_time,
                          time_t     timestamp)
{
    /* Prefer system type when available */
    if (system_time)
        return g_date_time_format (system_time, "%F %T");
    return g_strdup_printf ("(unix) %lu", timestamp);
}


/******************************************************************************/
/* Write to syslog */

static void
write_syslog_record (const gchar *from_timestamp,
                     const gchar *to_timestamp,
                     gulong       duration,
                     guint64      rx_bytes,
                     guint64      tx_bytes)
{
    rmfd_syslog (LOG_INFO,
                 "Connection stats [From: %s] [To: %s] [Duration: %lu] "
                 "[RX: %" G_GUINT64_FORMAT "] [TX: %" G_GUINT64_FORMAT "]",
                 from_timestamp,
                 to_timestamp,
                 duration,
                 rx_bytes,
                 tx_bytes);
}

/******************************************************************************/
/* Write to tmp stats file */

static void
write_record (gchar      record_type,
              GDateTime *from_system_time,
              time_t     from_time,
              GDateTime *to_system_time,
              time_t     to_time,
              guint64    rx_bytes,
              guint64    tx_bytes)
{

    gchar  line[MAX_LINE_LENGTH + 1];
    gchar *from_str;
    gchar *to_str;

    g_assert (record_type == 'S' || record_type == 'P' || record_type == 'F');

    /* Bail out if stats not enabled */
    if (!stats_file)
        return;

    from_str = common_build_date_string (from_system_time, from_time);
    to_str   = common_build_date_string (to_system_time,   to_time);

    /* We'll cap the max line length to a known value by default, just in case */
    g_snprintf (line, MAX_LINE_LENGTH, "%c\t%s\t%s\t%lu\t%" G_GUINT64_FORMAT "\t%" G_GUINT64_FORMAT "\n",
                record_type,
                from_str,
                to_str,
                (gulong) (to_time > from_time ? (to_time - from_time) : 0),
                rx_bytes,
                tx_bytes);

    if (fprintf (stats_file, "%s", line) < 0)
        g_warning ("error: cannot write to stats file: %s", g_strerror (ferror (stats_file)));
    else
        fflush (stats_file);

    g_free (from_str);
    g_free (to_str);
}

/******************************************************************************/
/* Read the last full valid record from the stats file, if any */

enum {
    FIELD_RECORD_TYPE      = 0,
    FIELD_FROM_SYSTEM_TIME = 1,
    FIELD_TO_SYSTEM_TIME   = 2,
    FIELD_DURATION         = 3,
    FIELD_RX_BYTES         = 4,
    FIELD_TX_BYTES         = 5,
    N_FIELDS
};

static gboolean
process_record (FILE  *file)
{
    gchar  line  [MAX_LINE_LENGTH + 1];
    gchar *fields[N_FIELDS];
    gchar *aux;
    guint  i = 0;

    if (!fgets (line, sizeof (line), file))
        return FALSE;

    for (i = 0, aux = line; i < N_FIELDS; i++, aux++) {
        fields[i] = aux;
        if (i < (N_FIELDS -1))
            aux = strchr (aux, '\t');
        else
            aux = strchr (aux, '\n');
        if (!aux)
            return FALSE;
        *aux = '\0';
    }

    g_debug ("previous stats file found:");
    for (i = 0; i < N_FIELDS; i++)
        g_debug ("\tlast record [%u]: '%s'", i, fields[i]);

    write_syslog_record (fields[FIELD_FROM_SYSTEM_TIME],
                         fields[FIELD_TO_SYSTEM_TIME],
                         (gulong) g_ascii_strtoull (fields[FIELD_DURATION], NULL, 10),
                         g_ascii_strtoull (fields[FIELD_RX_BYTES], NULL, 10),
                         g_ascii_strtoull (fields[FIELD_TX_BYTES], NULL, 10));

    return TRUE;
}

static gboolean
seek_current_record (FILE *file)
{
    guint n_rewinds = 0;
    glong offset;

    offset = ftell (file);
    if (offset < 0)
        return FALSE;

    /* Beginning of first line already */
    if (offset == 0)
        return TRUE;

    /* Move file pointer 1 byte back */
    if (fseek (file, -1, SEEK_CUR) < 0)
        return FALSE;

    while (1) {
        gint c;

        /* Absolute offset 0? We're in the beginning of first line already */
        offset = ftell (file);
        if (offset < 0)
            return FALSE;
        if (offset == 0)
            return TRUE;

        /* Read single byte */
        if ((c = fgetc (file)) == EOF)
            return FALSE;

        /* If previous char is EOL, return record start found.
         * Note: the read() operation moved the file pointer already */
        if (c == '\n')
            return TRUE;

        /* If too many rewinds looking for an EOL, fail */
        if (n_rewinds == MAX_LINE_LENGTH) {
            g_warning ("stats file record line too long");
            return FALSE;
        }
        n_rewinds++;

        /* Move file pointer 2 bytes back; we want to put the file pointer
         * in the byte before the one we think may be the record start. */
        if (fseek (file, -2, SEEK_CUR) < 0)
            return FALSE;
    }

    g_assert_not_reached ();
}

static gboolean
process_last_record (FILE *file)
{
    glong offset;

    /* Move file pointer to the last one */
    if (fseek (file, 0, SEEK_END) < 0)
        return FALSE;

    while (1) {
        /* Seek to start of the current record */
        if (!seek_current_record (file))
            return FALSE;

        /* Store offset */
        if ((offset = ftell (file)) < 0)
            return FALSE;

        /* Try to process record */
        if (process_record (file))
            return TRUE;

        /* Need to go backwards one more line */
        if (fseek (file, offset - 1, SEEK_SET) < 0)
            return FALSE;
    }

    g_assert_not_reached ();
}

static void
process_last_stats (void)
{
    FILE *file;

    if ((file = fopen (stats_file_path, "r"))) {
        gboolean processed;

        processed = process_last_record (file);
        fclose (file);

        g_debug ("removing previous stats file: (%s)", processed ? "processed" : "couldn't be processed") ;
        g_unlink (stats_file_path);
    }
}

/******************************************************************************/

void
rmfd_stats_start (GDateTime *system_time)
{
    /* Open the file only when started */
    errno = 0;
    if (!(stats_file = fopen (stats_file_path, "w"))) {
        g_warning ("error: cannot open stats file: %s", g_strerror (errno));
        return;
    }

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
    write_record ('P', start_system_time, start_time, tmp_system_time, time (NULL), rx_bytes, tx_bytes);
}

void
rmfd_stats_stop (GDateTime *stop_system_time,
                 guint64    rx_bytes,
                 guint64    tx_bytes)
{
    time_t stop_time;

    /* If for any reason stop is called multiple times, don't write multiple final records */
    if (!start_system_time)
        return;

    stop_time = time (NULL);

    write_record ('F', start_system_time, start_time, stop_system_time, stop_time, rx_bytes, tx_bytes);

    /* Syslog writing */
    {
        gchar *from_str;
        gchar *to_str;


        from_str = common_build_date_string (start_system_time, start_time);
        to_str   = common_build_date_string (stop_system_time,  stop_time);

        g_debug ("writing stats to syslog...");
        write_syslog_record (from_str,
                             to_str,
                             (stop_time > start_time ? (stop_time - start_time) : 0),
                             rx_bytes,
                             tx_bytes);

        g_free (from_str);
        g_free (to_str);
    }

    /* Cleanup start time */
    if (start_system_time)
        g_date_time_unref (start_system_time);
    start_system_time = NULL;
    start_time = 0;

    if (stats_file) {
        fclose (stats_file);
        stats_file = NULL;
    }

    /* Once written to syslog, remove the file */
    g_debug ("removing stats file...");
    g_unlink (stats_file_path);
}

void
rmfd_stats_setup (const gchar *path)
{
    g_assert (!stats_file);
    g_assert (!stats_file_path);

    stats_file_path = g_strdup (path);

    /* Try to process last stats right away */
    process_last_stats ();
}

void
rmfd_stats_teardown (void)
{
    if (start_system_time) {
        g_date_time_unref (start_system_time);
        start_system_time = NULL;
    }

    if (stats_file) {
        fclose (stats_file);
        stats_file = NULL;
    }

    g_free (stats_file_path);
    stats_file_path = NULL;
}

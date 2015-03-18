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
#include <glib/gstdio.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "rmfd-stats.h"
#include "rmfd-syslog.h"

static FILE      *stats_file;
static GDateTime *start_system_time;
static time_t     start_time;

#define STATS_FILE_PATH "/var/log/rmfd.stats"

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
write_syslog_record (gboolean     previous_run,
                     const gchar *from_timestamp,
                     const gchar *to_timestamp,
                     gulong       duration,
                     guint64      rx_bytes,
                     guint64      tx_bytes,
                     const gchar *radio_interface,
                     gint8        rssi,
                     guint16      mcc,
                     guint16      mnc,
                     guint16      lac,
                     guint32      cid,
                     guint        monthly_year,
                     guint        monthly_month,
                     guint64      monthly_rx_bytes,
                     guint64      monthly_tx_bytes)
{
    rmfd_syslog (LOG_INFO,
                 "Connection stats %s"
                 "[from: %s, to: %s, duration: %lus] "
                 "[rx: %" G_GUINT64_FORMAT ", tx: %" G_GUINT64_FORMAT "] "
                 "[access tech: %s, rssi: %ddBm] "
                 "[mcc: %u, mnc: %u, lac: %u, cid: %u] "
                 "[month %u/%u rx: %" G_GUINT64_FORMAT ", tx: %" G_GUINT64_FORMAT "] ",
                 previous_run ? "(previous run) " : "",
                 from_timestamp, to_timestamp, duration,
                 rx_bytes, tx_bytes,
                 radio_interface, rssi,
                 mcc, mnc, lac, cid,
                 monthly_year, monthly_month, monthly_rx_bytes, monthly_tx_bytes);
}

static void
write_monthly_stats (guint   year,
                     guint   month,
                     guint64 rx_bytes,
                     guint64 tx_bytes)
{
    rmfd_syslog (LOG_INFO,
                 "Month stats (%u/%u) "
                 "[rx: %" G_GUINT64_FORMAT ", tx: %" G_GUINT64_FORMAT "] ",
                 year, month,
                 rx_bytes, tx_bytes);
}

/******************************************************************************/
/* Write to tmp stats file */

static void
write_record (gchar        record_type,
              GDateTime   *from_system_time,
              time_t       from_time,
              GDateTime   *to_system_time,
              time_t       to_time,
              guint64      rx_bytes,
              guint64      tx_bytes,
              const gchar *radio_interface,
              gint8        rssi,
              guint16      mcc,
              guint16      mnc,
              guint16      lac,
              guint32      cid)
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
    g_snprintf (line, MAX_LINE_LENGTH, "%c\t%s\t%s\t%lu\t%" G_GUINT64_FORMAT "\t%" G_GUINT64_FORMAT "\t%s\t%d\t%u\t%u\t%u\t%u\n",
                record_type,
                from_str,
                to_str,
                (gulong) (to_time > from_time ? (to_time - from_time) : 0),
                rx_bytes,
                tx_bytes,
                radio_interface,
                (gint) rssi,
                (guint) mcc,
                (guint) mnc,
                (guint) lac,
                (guint) cid);

    if (fprintf (stats_file, "%s", line) < 0)
        g_warning ("error: cannot write to stats file: %s", g_strerror (ferror (stats_file)));
    else
        fflush (stats_file);

    g_free (from_str);
    g_free (to_str);
}

/******************************************************************************/
/* Record parser */

enum {
    FIELD_RECORD_TYPE      = 0,
    FIELD_FROM_SYSTEM_TIME = 1,
    FIELD_TO_SYSTEM_TIME   = 2,
    FIELD_DURATION         = 3,
    FIELD_RX_BYTES         = 4,
    FIELD_TX_BYTES         = 5,
    FIELD_RADIO_INTERFACE  = 6,
    FIELD_RSSI             = 7,
    FIELD_MCC              = 8,
    FIELD_MNC              = 9,
    FIELD_LAC              = 10,
    FIELD_CID              = 11,
    N_FIELDS
};

static gboolean
parse_record (gchar *line,
              gchar *fields[N_FIELDS])
{
    gchar *aux;
    guint  i = 0;

    g_assert (line);
    g_assert (fields);

    /* Start, Partial, or Final record? */
    if (line[0] != 'S' && line[0] != 'P' && line[0] != 'F')
        return FALSE;

    for (i = 0, aux = line; i < N_FIELDS; i++, aux++) {
        fields[i] = aux;
        aux = strchr (fields[i], '\t');
        if (!aux) {
            if (i < (N_FIELDS -1))
                return FALSE;
            aux = strchr (fields[i], '\n');
            if (!aux)
                return FALSE;
        }
        *aux = '\0';
    }

    return TRUE;
}

/******************************************************************************/
/* Monthly stats computation */

typedef struct {
    guint   year;
    guint   month;
    guint64 rx_bytes;
    guint64 tx_bytes;
} MonthlyStats;

static MonthlyStats monthly_stats;

static void
monthly_stats_append_record (const gchar *system_time_str,
                             const gchar *rx_bytes_str,
                             const gchar *tx_bytes_str)
{
    gchar   **split;
    guint     year;
    guint     month;
    guint64   rx_bytes;
    guint64   tx_bytes;

    /* Try to gather year and month integers from the string */
    split = g_strsplit_set (system_time_str, "- :", -1);
    if (g_strv_length (split) == 1) {
        gint64     timestamp;
        GDateTime *datetime;

        /* We have a unix timestamp here */
        timestamp = g_ascii_strtoll (split[0], NULL, 10);
        datetime  = g_date_time_new_from_unix_utc (timestamp);
        year      = g_date_time_get_year (datetime);
        month     = g_date_time_get_month (datetime);
        g_date_time_unref (datetime);
    } else {
        /* Time may be given in ISO861 format: YYYY-MM-DD HH:MM:SS */
        year  = (guint) g_ascii_strtoull (split[0], NULL, 10);
        month = (guint) g_ascii_strtoull (split[1], NULL, 10);
    }
    g_strfreev (split);

    rx_bytes = g_ascii_strtoull (rx_bytes_str, NULL, 10);
    tx_bytes = g_ascii_strtoull (tx_bytes_str, NULL, 10);

    /* If year/month info not yet added, do it right away */
    if (monthly_stats.year == 0 || monthly_stats.month == 0) {
        g_debug ("  set initial stats date: %u/%u", year, month);
        monthly_stats.year = year;
        monthly_stats.month = month;
    }

    /* If stats for the same month as the first one, add them */
    if (year == monthly_stats.year && month == monthly_stats.month) {
        g_debug ("  record (%u/%u): rx+=%" G_GUINT64_FORMAT ", tx+=%" G_GUINT64_FORMAT,
                 year, month, rx_bytes, tx_bytes);
        monthly_stats.rx_bytes += rx_bytes;
        monthly_stats.tx_bytes += tx_bytes;
    } else if ((year == monthly_stats.year && month > monthly_stats.month) ||
               (year > monthly_stats.year)) {
        g_debug ("  updated stats date: %u/%u", year, month);
        g_debug ("  record (%u/%u): rx=%" G_GUINT64_FORMAT ", tx=%" G_GUINT64_FORMAT,
                 year, month, rx_bytes, tx_bytes);
        monthly_stats.year     = year;
        monthly_stats.month    = month;
        monthly_stats.rx_bytes = rx_bytes;
        monthly_stats.tx_bytes = tx_bytes;
    } else {
        g_debug ("  ignoring record with wrong date: %u/%u (reference: %u/%u)",
                 year, month, monthly_stats.year, monthly_stats.month);
        g_debug ("  record (%u/%u): rx (ignored) %" G_GUINT64_FORMAT ", tx (ignored) %" G_GUINT64_FORMAT,
                 year, month, rx_bytes, tx_bytes);
    }
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
            g_warning ("  stats file record line too long");
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

static void
process_previous_stats (FILE     *file,
                        glong     record_offset,
                        gboolean  notify,
                        gboolean  append_monthly_stats)
{
    if (fseek (file, record_offset, SEEK_SET) < 0) {
        g_warning ("  cannot seek to previous record");
        return;
    }

    while (1) {
        gchar  line  [MAX_LINE_LENGTH + 1];
        gchar *fields[N_FIELDS];

        /* This may happen if e.g. the immediate previous record wasn't correctly
         * parsed and also was actually the first one in the log file */
        if (!fgets (line, sizeof (line), file))
            return;

        /* If correctly parsed, notify and we're done */
        if (parse_record (line, fields)) {
            if (append_monthly_stats)
                monthly_stats_append_record (fields[FIELD_FROM_SYSTEM_TIME],
                                             fields[FIELD_RX_BYTES],
                                             fields[FIELD_TX_BYTES]);
            if (notify)
                write_syslog_record (TRUE,
                                     fields[FIELD_FROM_SYSTEM_TIME],
                                     fields[FIELD_TO_SYSTEM_TIME],
                                     (gulong) g_ascii_strtoull (fields[FIELD_DURATION], NULL, 10),
                                     g_ascii_strtoull (fields[FIELD_RX_BYTES], NULL, 10),
                                     g_ascii_strtoull (fields[FIELD_TX_BYTES], NULL, 10),
                                     fields[FIELD_RADIO_INTERFACE],
                                     (gint) g_ascii_strtoll (fields[FIELD_RSSI], NULL, 10),
                                     (guint16) g_ascii_strtoull (fields[FIELD_MCC], NULL, 10),
                                     (guint16) g_ascii_strtoull (fields[FIELD_MNC], NULL, 10),
                                     (guint32) g_ascii_strtoull (fields[FIELD_LAC], NULL, 10),
                                     (guint32) g_ascii_strtoull (fields[FIELD_CID], NULL, 10),
                                     monthly_stats.year,
                                     monthly_stats.month,
                                     monthly_stats.rx_bytes,
                                     monthly_stats.tx_bytes);

            return;
        }

        /* If not correctly parsed, go one record back */
        /* Need to go backwards one more line */
        if (fseek (file, record_offset - 1, SEEK_SET) < 0)
            return;
        /* Seek to start of the current record */
        if (!seek_current_record (file))
            return;
        /* Store new record offset */
        if ((record_offset = ftell (file)) < 0)
            return;
        /* Looooop */
    }

    g_assert_not_reached ();
}

static void
load_previous_stats (void)
{
    FILE     *file;
    gchar     line [MAX_LINE_LENGTH + 1];
    gboolean  started = FALSE;
    glong     previous_line_offset = -1;
    glong     current_line_offset = 0;

    g_debug ("loading previous monthly stats...");

    if (!(file = fopen (STATS_FILE_PATH, "r"))) {
        g_debug ("  stats file doesn't exist");
        return;
    }

    do {
        current_line_offset = ftell (file);
        if (current_line_offset < 0)
            break;

        if (!fgets (line, sizeof (line), file)) {
            /* When reaching EOF, check if the last log was notified to syslog or not */
            if (feof (file)) {
                if (started && previous_line_offset >= 0) {
                    /* We got a new Start record without a previous Final record.
                     * This means that rmfd was halted before being able to log
                     * to syslog, so we must do it ourselves now. Re-read the
                     * previous record as final and continue. */
                    process_previous_stats (file, previous_line_offset, TRUE, TRUE);
                }
            }
            break;
        }

        if (line[0] == 'S') {
            if (started) {
                current_line_offset = ftell (file);
                if (current_line_offset < 0)
                    break;

                /* We got a new Start record without a previous Final record. We
                 * need to parse the previous record and add it as if it were a
                 * final one */
                if (previous_line_offset >= 0)
                    process_previous_stats (file, previous_line_offset, FALSE, TRUE);

                /* Seek to the start record */
                if (fseek (file, current_line_offset, SEEK_SET) < 0)
                    break;

                /* Re-read the new start record, this time we won't have the started
                 * flag set */
                started = FALSE;
                continue;
            }

            /* Flag the session started */
            started = TRUE;
        }
        else if (line[0] == 'F') {
            gchar *fields[N_FIELDS];

            /* If correctly parsed, notify and we're done */
            if (parse_record (line, fields))
                monthly_stats_append_record (fields[FIELD_FROM_SYSTEM_TIME],
                                             fields[FIELD_RX_BYTES],
                                             fields[FIELD_TX_BYTES]);
            started = FALSE;
        }

        previous_line_offset = current_line_offset;
    } while (1);

    fclose (file);

    if (monthly_stats.year && monthly_stats.month)
        g_debug ("  monthly stats (%u/%u): rx %" G_GUINT64_FORMAT ", tx %" G_GUINT64_FORMAT,
                 monthly_stats.year, monthly_stats.month, monthly_stats.rx_bytes, monthly_stats.tx_bytes);
}

/******************************************************************************/

void
rmfd_stats_record (RmfdStatsRecordType  type,
                   GDateTime           *system_time,
                   guint64              rx_bytes,
                   guint64              tx_bytes,
                   const gchar         *radio_interface,
                   gint8                rssi,
                   guint16              mcc,
                   guint16              mnc,
                   guint16              lac,
                   guint32              cid)
{
    time_t current_time;

    current_time = time (NULL);

    /* Start record */
    if (type == RMFD_STATS_RECORD_TYPE_START) {
        guint    year;
        guint    month;
        gboolean append = TRUE;

        /* If we changed month, remove previous file */
        if (system_time) {
            year = g_date_time_get_year   (system_time);
            month = g_date_time_get_month (system_time);
        } else {
            GDateTime *datetime;

            datetime  = g_date_time_new_from_unix_utc (current_time);
            year      = g_date_time_get_year (datetime);
            month     = g_date_time_get_month (datetime);
            g_date_time_unref (datetime);
        }

        /* If changing stats month, syslog and remove the previous file */
        if ((year == monthly_stats.year && month > monthly_stats.month) ||
            (year > monthly_stats.year)) {
            write_monthly_stats (monthly_stats.year,
                                 monthly_stats.month,
                                 monthly_stats.rx_bytes,
                                 monthly_stats.tx_bytes);

            g_debug ("updated stats date: %u/%u", year, month);
            monthly_stats.year     = year;
            monthly_stats.month    = month;
            monthly_stats.rx_bytes = 0;
            monthly_stats.tx_bytes = 0;

            append = FALSE;
        }

        /* Open the file only when started */
        errno = 0;
        if (!(stats_file = fopen (STATS_FILE_PATH, append ? "a" : "w"))) {
            g_warning ("error: cannot open stats file: %s", g_strerror (errno));
            return;
        }

        /* Keep track of when this was started */
        if (start_system_time)
            g_date_time_unref (start_system_time);
        start_system_time = system_time ? g_date_time_ref (system_time) : NULL;
        start_time = current_time;

        write_record ('S',
                      start_system_time, start_time,
                      system_time, current_time,
                      rx_bytes, tx_bytes,
                      radio_interface, rssi,
                      mcc, mnc, lac, cid);

        return;
    }

    /* Partial record? */
    if (type == RMFD_STATS_RECORD_TYPE_PARTIAL) {
        write_record ('P',
                      start_system_time, start_time,
                      system_time, current_time,
                      rx_bytes, tx_bytes,
                      radio_interface, rssi,
                      mcc, mnc, lac, cid);
        return;
    }

    g_assert (type == RMFD_STATS_RECORD_TYPE_FINAL);

    /* If for any reason stop is called multiple times, don't write multiple final records */
    if (!start_system_time)
        return;

    /* Final record */
    write_record ('F',
                  start_system_time, start_time,
                  system_time, current_time,
                  rx_bytes, tx_bytes,
                  radio_interface, rssi,
                  mcc, mnc, lac, cid);

    /* Update monthly stats */
    monthly_stats.rx_bytes += rx_bytes;
    monthly_stats.tx_bytes += tx_bytes;

    /* Syslog writing */
    {
        gchar *from_str;
        gchar *to_str;

        from_str = common_build_date_string (start_system_time, start_time);
        to_str   = common_build_date_string (system_time,       current_time);

        g_debug ("writing stats to syslog...");
        write_syslog_record (FALSE,
                             from_str,
                             to_str,
                             (current_time > start_time ? (current_time - start_time) : 0),
                             rx_bytes,
                             tx_bytes,
                             radio_interface, rssi,
                             mcc, mnc, lac, cid,
                             monthly_stats.year,
                             monthly_stats.month,
                             monthly_stats.rx_bytes,
                             monthly_stats.tx_bytes);

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
}

void
rmfd_stats_setup (void)
{
    /* Try to process last stats right away */
    load_previous_stats ();
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
}

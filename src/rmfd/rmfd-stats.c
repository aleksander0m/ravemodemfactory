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
#include <unistd.h>
#include <sys/types.h>

#include "rmfd-stats.h"
#include "rmfd-syslog.h"

typedef struct {
    guint   year;
    guint   month;
    guint64 rx_bytes;
    guint64 tx_bytes;
} MonthlyStats;

struct _RmfdStatsContext {
    gchar        *path;
    FILE         *file;
    GDateTime    *start_system_time;
    time_t        start_time;
    MonthlyStats  monthly_stats;
};

#define MAX_LINE_LENGTH 255

/******************************************************************************/
/* Build date string to show */

#define UNIX_TIMESTAMP_PREFIX "(unix)"

static gchar *
common_build_date_string (GDateTime *system_time,
                          time_t     timestamp)
{
    /* Prefer system type when available */
    if (system_time)
        return g_date_time_format (system_time, "%F %T");
    return g_strdup_printf ("%s %lu", UNIX_TIMESTAMP_PREFIX, timestamp);
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
write_record (FILE        *file,
              gchar        record_type,
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

    if (fprintf (file, "%s", line) < 0)
        g_warning ("error: cannot write to stats file: %s", g_strerror (ferror (file)));
    else
        fflush (file);

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

static void
monthly_stats_append_record (RmfdStatsContext *ctx,
                             const gchar      *system_time_str,
                             const gchar      *rx_bytes_str,
                             const gchar      *tx_bytes_str)
{
    guint   year  = 0;
    guint   month = 0;
    guint64 rx_bytes;
    guint64 tx_bytes;

    /* Try to gather year and month integers from the string */

    if (g_str_has_prefix (system_time_str, UNIX_TIMESTAMP_PREFIX)) {
        gint64       timestamp;
        GDateTime   *datetime;
        const gchar *aux;

        aux = system_time_str + strlen (UNIX_TIMESTAMP_PREFIX);
        timestamp = g_ascii_strtoll (aux, NULL, 10);
        datetime  = g_date_time_new_from_unix_utc (timestamp);
        if (datetime) {
            year  = g_date_time_get_year  (datetime);
            month = g_date_time_get_month (datetime);
            g_date_time_unref (datetime);
        }
    } else {
        gchar **split;

        split = g_strsplit_set (system_time_str, "- :", -1);
        if (split && split[0] && split[1]) {
            /* Time may be given in ISO861 format: YYYY-MM-DD HH:MM:SS */
            year  = (guint) g_ascii_strtoull (split[0], NULL, 10);
            month = (guint) g_ascii_strtoull (split[1], NULL, 10);
        } else {

        }
        g_strfreev (split);
    }

    if (year == 0 || month == 0) {
        g_warning ("  cannot read timestamp: '%s'", system_time_str);
        return;
    }

    rx_bytes = g_ascii_strtoull (rx_bytes_str, NULL, 10);
    tx_bytes = g_ascii_strtoull (tx_bytes_str, NULL, 10);

    /* If year/month info not yet added, do it right away */
    if (ctx->monthly_stats.year == 0 || ctx->monthly_stats.month == 0) {
        g_debug ("  set initial stats date: %u/%u", year, month);
        ctx->monthly_stats.year = year;
        ctx->monthly_stats.month = month;
    }

    /* If stats for the same month as the first one, add them */
    if (year == ctx->monthly_stats.year && month == ctx->monthly_stats.month) {
        g_debug ("  record (%u/%u): rx+=%" G_GUINT64_FORMAT ", tx+=%" G_GUINT64_FORMAT,
                 year, month, rx_bytes, tx_bytes);
        ctx->monthly_stats.rx_bytes += rx_bytes;
        ctx->monthly_stats.tx_bytes += tx_bytes;
    } else if ((year == ctx->monthly_stats.year && month > ctx->monthly_stats.month) ||
               (year > ctx->monthly_stats.year)) {
        g_debug ("  updated stats date: %u/%u", year, month);
        g_debug ("  record (%u/%u): rx=%" G_GUINT64_FORMAT ", tx=%" G_GUINT64_FORMAT,
                 year, month, rx_bytes, tx_bytes);
        ctx->monthly_stats.year     = year;
        ctx->monthly_stats.month    = month;
        ctx->monthly_stats.rx_bytes = rx_bytes;
        ctx->monthly_stats.tx_bytes = tx_bytes;
    } else {
        g_debug ("  ignoring record with wrong date: %u/%u (reference: %u/%u)",
                 year, month, ctx->monthly_stats.year, ctx->monthly_stats.month);
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
process_previous_stats (RmfdStatsContext *ctx,
                        glong             record_offset,
                        gboolean          set_as_final,
                        gboolean          append_monthly_stats)
{
    if (fseek (ctx->file, record_offset, SEEK_SET) < 0) {
        g_warning ("  cannot seek to previous record");
        return;
    }

    while (1) {
        gchar  line  [MAX_LINE_LENGTH + 1];
        gchar *fields[N_FIELDS];

        /* This may happen if e.g. the immediate previous record wasn't correctly
         * parsed and also was actually the first one in the log file */
        if (!fgets (line, sizeof (line), ctx->file))
            return;

        /* If correctly parsed, notify and we're done */
        if (parse_record (line, fields)) {
            if (append_monthly_stats)
                monthly_stats_append_record (ctx,
                                             fields[FIELD_FROM_SYSTEM_TIME],
                                             fields[FIELD_RX_BYTES],
                                             fields[FIELD_TX_BYTES]);
            if (set_as_final) {
                glong record_end;

                record_end = ftell (ctx->file);

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
                                     ctx->monthly_stats.year,
                                     ctx->monthly_stats.month,
                                     ctx->monthly_stats.rx_bytes,
                                     ctx->monthly_stats.tx_bytes);

                if (fseek (ctx->file, record_offset, SEEK_SET) < 0)
                    g_warning ("  cannot seek to previous record to update it");
                else {
                    g_debug ("  previous record set as final");
                    fputc ('F', ctx->file);
                    /* Also, remove any additional text found after this record,
                     * like e.g. a possible record which wasn't correctly parsed */
                    if (record_end > 0)
                        truncate (ctx->path, (off_t) record_end);
                }
            }

            return;
        }

        /* If not correctly parsed, go one record back */
        /* Need to go backwards one more line */
        if (fseek (ctx->file, record_offset - 1, SEEK_SET) < 0)
            return;
        /* Seek to start of the current record */
        if (!seek_current_record (ctx->file))
            return;
        /* Store new record offset */
        if ((record_offset = ftell (ctx->file)) < 0)
            return;
        /* Looooop */
    }

    g_assert_not_reached ();
}

static void
load_previous_stats (RmfdStatsContext *ctx)
{
    gchar    line [MAX_LINE_LENGTH + 1];
    gboolean started = FALSE;
    glong    previous_line_offset = -1;
    glong    current_line_offset = 0;

    g_debug ("loading previous monthly stats...");

    if (!(ctx->file = fopen (ctx->path, "r+"))) {
        g_debug ("  stats file doesn't exist");
        return;
    }

    do {
        current_line_offset = ftell (ctx->file);
        if (current_line_offset < 0)
            break;

        if (!fgets (line, sizeof (line), ctx->file)) {
            /* When reaching EOF, check if the last log was notified to syslog or not */
            if (feof (ctx->file)) {
                if (started && previous_line_offset >= 0) {
                    /* We got a new Start record without a previous Final record.
                     * This means that rmfd was halted before being able to log
                     * to syslog, so we must do it ourselves now. Re-read the
                     * previous record as final and continue. */
                    process_previous_stats (ctx, previous_line_offset, TRUE, TRUE);
                    /* Seek to the end again */
                    if (fseek (ctx->file, 0, SEEK_END) < 0)
                        break;
                }
            }
            break;
        }

        if (line[0] == 'S') {
            if (started) {
                current_line_offset = ftell (ctx->file);
                if (current_line_offset < 0)
                    break;

                /* We got a new Start record without a previous Final record. We
                 * need to parse the previous record and add it as if it were a
                 * final one */
                if (previous_line_offset >= 0)
                    process_previous_stats (ctx, previous_line_offset, FALSE, TRUE);

                /* Seek to the start record */
                if (fseek (ctx->file, current_line_offset, SEEK_SET) < 0)
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
                monthly_stats_append_record (ctx,
                                             fields[FIELD_FROM_SYSTEM_TIME],
                                             fields[FIELD_RX_BYTES],
                                             fields[FIELD_TX_BYTES]);
            started = FALSE;
        }

        previous_line_offset = current_line_offset;
    } while (1);

    fclose (ctx->file);
    ctx->file = NULL;

    if (ctx->monthly_stats.year && ctx->monthly_stats.month)
        g_debug ("  monthly stats (%u/%u): rx %" G_GUINT64_FORMAT ", tx %" G_GUINT64_FORMAT,
                 ctx->monthly_stats.year, ctx->monthly_stats.month, ctx->monthly_stats.rx_bytes, ctx->monthly_stats.tx_bytes);
}

/******************************************************************************/

void
rmfd_stats_record (RmfdStatsContext    *ctx,
                   RmfdStatsRecordType  type,
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

    /* Bail out if stats not enabled */
    if (!ctx)
        return;

    current_time = time (NULL);

    /* Start record */
    if (type == RMFD_STATS_RECORD_TYPE_START) {
        guint    year;
        guint    month;
        gboolean append = TRUE;

        /* If we changed month, remove previous file */
        if (system_time) {
            year  = g_date_time_get_year   (system_time);
            month = g_date_time_get_month (system_time);
        } else {
            GDateTime *datetime;

            datetime  = g_date_time_new_from_unix_utc (current_time);
            year      = g_date_time_get_year (datetime);
            month     = g_date_time_get_month (datetime);
            g_date_time_unref (datetime);
        }

        /* If changing stats month, syslog and remove the previous file */
        if ((year == ctx->monthly_stats.year && month > ctx->monthly_stats.month) ||
            (year > ctx->monthly_stats.year)) {
            if (ctx->monthly_stats.year > 0)
                write_monthly_stats (ctx->monthly_stats.year,
                                     ctx->monthly_stats.month,
                                     ctx->monthly_stats.rx_bytes,
                                     ctx->monthly_stats.tx_bytes);

            g_debug ("updated stats date: %u/%u", year, month);
            ctx->monthly_stats.year     = year;
            ctx->monthly_stats.month    = month;
            ctx->monthly_stats.rx_bytes = 0;
            ctx->monthly_stats.tx_bytes = 0;

            append = FALSE;
        }

        /* Open the file only when started */
        errno = 0;
        if (!(ctx->file = fopen (ctx->path, append ? "a" : "w"))) {
            g_warning ("error: cannot open stats file: %s", g_strerror (errno));
            return;
        }

        /* Keep track of when this was started */
        if (ctx->start_system_time)
            g_date_time_unref (ctx->start_system_time);
        ctx->start_system_time = system_time ? g_date_time_ref (system_time) : NULL;
        ctx->start_time = current_time;

        write_record (ctx->file,
                      'S',
                      ctx->start_system_time, ctx->start_time,
                      system_time, current_time,
                      rx_bytes, tx_bytes,
                      radio_interface, rssi,
                      mcc, mnc, lac, cid);

        return;
    }

    /* Partial record? */
    if (type == RMFD_STATS_RECORD_TYPE_PARTIAL) {
        write_record (ctx->file,
                      'P',
                      ctx->start_system_time, ctx->start_time,
                      system_time, current_time,
                      rx_bytes, tx_bytes,
                      radio_interface, rssi,
                      mcc, mnc, lac, cid);
        return;
    }

    g_assert (type == RMFD_STATS_RECORD_TYPE_FINAL);

    /* If for any reason stop is called multiple times, don't write multiple final records */
    if (!ctx->start_system_time)
        return;

    /* Final record */
    write_record (ctx->file,
                  'F',
                  ctx->start_system_time, ctx->start_time,
                  system_time, current_time,
                  rx_bytes, tx_bytes,
                  radio_interface, rssi,
                  mcc, mnc, lac, cid);

    /* Update monthly stats */
    ctx->monthly_stats.rx_bytes += rx_bytes;
    ctx->monthly_stats.tx_bytes += tx_bytes;

    /* Syslog writing */
    {
        gchar *from_str;
        gchar *to_str;

        from_str = common_build_date_string (ctx->start_system_time, ctx->start_time);
        to_str   = common_build_date_string (system_time, current_time);

        g_debug ("writing stats to syslog...");
        write_syslog_record (FALSE,
                             from_str,
                             to_str,
                             (current_time > ctx->start_time ? (current_time - ctx->start_time) : 0),
                             rx_bytes,
                             tx_bytes,
                             radio_interface, rssi,
                             mcc, mnc, lac, cid,
                             ctx->monthly_stats.year,
                             ctx->monthly_stats.month,
                             ctx->monthly_stats.rx_bytes,
                             ctx->monthly_stats.tx_bytes);

        g_free (from_str);
        g_free (to_str);
    }

    /* Cleanup start time */
    if (ctx->start_system_time)
        g_date_time_unref (ctx->start_system_time);
    ctx->start_system_time = NULL;
    ctx->start_time = 0;

    if (ctx->file) {
        fclose (ctx->file);
        ctx->file = NULL;
    }
}

/******************************************************************************/

RmfdStatsContext *
rmfd_stats_setup (const gchar *path)
{
    RmfdStatsContext *ctx;

    ctx = g_slice_new0 (RmfdStatsContext);
    ctx->path = g_strdup (path);

    /* Try to process last stats right away */
    load_previous_stats (ctx);

    return ctx;
}

void
rmfd_stats_teardown (RmfdStatsContext *ctx)
{
    if (!ctx)
        return;

    if (ctx->start_system_time)
        g_date_time_unref (ctx->start_system_time);
    if (ctx->file)
        fclose (ctx->file);
    g_free (ctx->path);
    g_slice_free (RmfdStatsContext, ctx);
}

guint
rmfd_stats_get_year (RmfdStatsContext *ctx)
{
    g_return_val_if_fail (ctx != NULL, 0);

    return ctx->monthly_stats.year;
}

guint
rmfd_stats_get_month (RmfdStatsContext *ctx)
{
    g_return_val_if_fail (ctx != NULL, 0);

    return ctx->monthly_stats.month;
}

guint64
rmfd_stats_get_rx_bytes (RmfdStatsContext *ctx)
{
    g_return_val_if_fail (ctx != NULL, 0);

    return ctx->monthly_stats.rx_bytes;
}

guint64
rmfd_stats_get_tx_bytes (RmfdStatsContext *ctx)
{
    g_return_val_if_fail (ctx != NULL, 0);

    return ctx->monthly_stats.tx_bytes;
}

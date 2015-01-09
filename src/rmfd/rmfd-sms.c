/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details:
 *
 * SMS support based on ModemManager:
 *   Copyright (C) 2010 - 2012 Red Hat, Inc.
 *   Copyright (C) 2012 Google, Inc.
 *
 * Copyright (C) 2015 Zodiac Inflight Innovations
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#include "rmfd-sms.h"
#include "rmfd-error.h"
#include "rmfd-error-types.h"

struct _RmfdSms {
    QmiWmsStorageType    storage;
    gboolean             is_multipart;
    guint                multipart_reference;

    /* List of SMS parts */
    guint  max_parts;
    GList *parts;

    /* Set to true when all needed parts were received,
     * parsed and assembled */
    gboolean is_assembled;

    /* Contents */
    RmfdSmsPduType  pdu_type;
    GString        *fulltext;
    GByteArray     *fulldata;
    gchar          *smsc;
    gchar          *number;
    gchar          *timestamp;
};

/*****************************************************************************/

static gboolean
assemble_sms (RmfdSms  *self,
              GError  **error)
{
    GList        *l;
    guint         idx;
    RmfdSmsPart **sorted_parts;

    sorted_parts = g_new0 (RmfdSmsPart *, self->max_parts);

    /* Note that sequence in multipart messages start with '1', while singlepart
     * messages have '0' as sequence. */

    if (self->max_parts == 1) {
        if (g_list_length (self->parts) != 1) {
            g_set_error (error,
                         RMFD_ERROR,
                         RMFD_ERROR_UNKNOWN,
                         "Single part message with multiple parts (%u) found",
                         g_list_length (self->parts));
            g_free (sorted_parts);
            return FALSE;
        }

        sorted_parts[0] = (RmfdSmsPart *)self->parts->data;
    } else {
        /* Check if we have duplicate parts */
        for (l = self->parts; l; l = g_list_next (l)) {
            idx = rmfd_sms_part_get_concat_sequence ((RmfdSmsPart *)l->data);

            if (idx < 1 || idx > self->max_parts) {
                g_warning ("Invalid part index (%u) found, ignoring", idx);
                continue;
            }

            if (sorted_parts[idx - 1]) {
                g_warning ("Duplicate part index (%u) found, ignoring", idx);
                continue;
            }

            /* Add the part to the proper index */
            sorted_parts[idx - 1] = (RmfdSmsPart *)l->data;
        }
    }

    self->fulltext = g_string_new ("");
    self->fulldata = g_byte_array_sized_new (160 * self->max_parts);

    /* Assemble text and data from all parts. Now 'idx' is the index of the
     * array, so for multipart messages the real index of the part is 'idx + 1'
     */
    for (idx = 0; idx < self->max_parts; idx++) {
        const gchar      *parttext;
        const GByteArray *partdata;

        if (!sorted_parts[idx]) {
            g_set_error (error,
                         RMFD_ERROR,
                         RMFD_ERROR_UNKNOWN,
                         "Cannot assemble SMS, missing part at index (%u)",
                         self->max_parts == 1 ? idx : idx + 1);
            g_free (sorted_parts);
            return FALSE;
        }

        /* When the user creates the SMS, it will have either 'text' or 'data',
         * not both. Also status report PDUs may not have neither text nor data. */
        parttext = rmfd_sms_part_get_text (sorted_parts[idx]);
        partdata = rmfd_sms_part_get_data (sorted_parts[idx]);

        if (!parttext && !partdata &&
            rmfd_sms_part_get_pdu_type (sorted_parts[idx]) != RMFD_SMS_PDU_TYPE_STATUS_REPORT) {
            g_set_error (error,
                         RMFD_ERROR,
                         RMFD_ERROR_UNKNOWN,
                         "Cannot assemble SMS, part at index (%u) has neither text nor data",
                         self->max_parts == 1 ? idx : idx + 1);
            g_free (sorted_parts);
            return FALSE;
        }

        if (parttext)
            g_string_append (self->fulltext, parttext);
        if (partdata)
            g_byte_array_append (self->fulldata, partdata->data, partdata->len);
    }

    /* If we got all parts, we also have the first one always */
    g_assert (sorted_parts[0] != NULL);

    /* Store other contents */
    self->pdu_type  = rmfd_sms_part_get_pdu_type            (sorted_parts[0]);
    self->smsc      = g_strdup (rmfd_sms_part_get_smsc      (sorted_parts[0]));
    self->number    = g_strdup (rmfd_sms_part_get_number    (sorted_parts[0]));
    self->timestamp = g_strdup (rmfd_sms_part_get_timestamp (sorted_parts[0]));

    g_free (sorted_parts);

    self->is_assembled = TRUE;

    return TRUE;
}

/*****************************************************************************/

void
rmfd_sms_free (RmfdSms *self)
{
    g_free (self->smsc);
    g_free (self->number);
    g_free (self->timestamp);
    if (self->fulltext)
        g_string_free (self->fulltext, TRUE);
    if (self->fulldata)
        g_byte_array_free (self->fulldata, TRUE);
    g_list_free_full (self->parts, (GDestroyNotify) rmfd_sms_part_free);
    g_slice_free (RmfdSms, self);
}

/*****************************************************************************/

RmfdSms *
rmfd_sms_singlepart_new (QmiWmsStorageType      storage,
                         RmfdSmsPart             *part,
                         GError               **error)
{
    RmfdSms *self;

    self = g_slice_new0 (RmfdSms);
    self->storage   = storage;
    self->max_parts = 1;

    /* Keep the single part in the list */
    self->parts = g_list_prepend (self->parts, part);

    if (!assemble_sms (self, error)) {
        /* Note: we need to remove the part from the list without freeing it, as
         * we really didn't take it, and therefore the caller is responsible for
         * freeing it. */
        self->parts = g_list_remove (self->parts, part);
        rmfd_sms_free (self);
        return NULL;
    }

    return self;
}

/*****************************************************************************/

static guint
cmp_sms_part_sequence (RmfdSmsPart *a,
                       RmfdSmsPart *b)
{
    return (rmfd_sms_part_get_concat_sequence (a) - rmfd_sms_part_get_concat_sequence (b));
}

gboolean
rmfd_sms_multipart_is_complete (RmfdSms *self)
{
    return (g_list_length (self->parts) == self->max_parts);
}

gboolean
rmfd_sms_multipart_is_assembled (RmfdSms *self)
{
    return self->is_assembled;
}

gboolean
rmfd_sms_multipart_take_part (RmfdSms      *self,
                              RmfdSmsPart  *part,
                              GError      **error)
{
    if (!self->is_multipart) {
        g_set_error (error,
                     RMFD_ERROR,
                     RMFD_ERROR_UNKNOWN,
                     "This SMS is not a multipart message");
        return FALSE;
    }

    if (g_list_length (self->parts) >= self->max_parts) {
        g_set_error (error,
                     RMFD_ERROR,
                     RMFD_ERROR_UNKNOWN,
                     "Already took %u parts, cannot take more",
                     g_list_length (self->parts));
        return FALSE;
    }

    if (g_list_find_custom (self->parts, part, (GCompareFunc)cmp_sms_part_sequence)) {
        g_set_error (error,
                     RMFD_ERROR,
                     RMFD_ERROR_UNKNOWN,
                     "Cannot take part, sequence %u already taken",
                     rmfd_sms_part_get_concat_sequence (part));
        return FALSE;
    }

    if (rmfd_sms_part_get_concat_sequence (part) > self->max_parts) {
        g_set_error (error,
                     RMFD_ERROR,
                     RMFD_ERROR_UNKNOWN,
                     "Cannot take part with sequence %u, maximum is %u",
                     rmfd_sms_part_get_concat_sequence (part),
                     self->max_parts);
        return FALSE;
    }

    /* Insert sorted by concat sequence */
    self->parts = g_list_insert_sorted (self->parts, part, (GCompareFunc) cmp_sms_part_sequence);

    /* We only populate contents when the multipart SMS is complete */
    if (rmfd_sms_multipart_is_complete (self)) {
        GError *inner_error = NULL;

        if (!assemble_sms (self, &inner_error)) {
            /* We DO NOT propagate the error. The part was properly taken
             * so ownership passed to the object. */
            g_warning ("Couldn't assemble SMS: '%s'", inner_error->message);
            g_error_free (inner_error);
        }
    }

    return TRUE;
}

RmfdSms *
rmfd_sms_multipart_new (QmiWmsStorageType      storage,
                        guint                  reference,
                        guint                  max_parts,
                        RmfdSmsPart           *first_part,
                        GError               **error)
{
    RmfdSms *self;

    self = g_slice_new0 (RmfdSms);
    self->storage             = storage;
    self->is_multipart        = TRUE;
    self->multipart_reference = reference;
    self->max_parts           = max_parts;

    /* Take part */
    if (!rmfd_sms_multipart_take_part (self, first_part, error)) {
        rmfd_sms_free (self);
        return NULL;
    }

    return self;
}

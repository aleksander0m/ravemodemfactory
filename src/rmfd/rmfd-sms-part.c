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
 *
 * SMS part support based on ModemManager:
 *   Copyright (C) 2010 - 2012 Red Hat, Inc.
 *   Copyright (C) 2012 Google, Inc.
 *
 * Copyright (C) 2015 Zodiac Inflight Innovations
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#include <ctype.h>
#include <string.h>

#include <glib.h>

#include "rmfd-sms-part.h"

/******************************************************************************/

const gchar *
rmfd_sms_encoding_get_string (RmfdSmsEncoding encoding)
{
    static const gchar *encoding_strings [] = {
        "unknown",
        "gsm7",
        "utf8"
        "ucs2"
    };

    if (encoding < RMFD_SMS_ENCODING_UNKNOWN || encoding > RMFD_SMS_ENCODING_UCS2)
        return "invalid";

    return encoding_strings[encoding];
}

/******************************************************************************/

struct _RmfdSmsPart {
    /* Reference counting */
    volatile gint ref_count;

    guint index;
    RmfdSmsPduType pdu_type;
    gchar *smsc;
    gchar *timestamp;
    gchar *discharge_timestamp;
    gchar *number;
    gchar *text;
    RmfdSmsEncoding encoding;
    GByteArray *data;
    gint  class;
    guint validity_relative;
    gboolean delivery_report_request;
    guint message_reference;
    /* NOT a RmfdSmsDeliveryState, which just includes the known values */
    guint delivery_state;

    gboolean should_concat;
    guint concat_reference;
    guint concat_max;
    guint concat_sequence;
};

RmfdSmsPart *
rmfd_sms_part_ref (RmfdSmsPart *self)
{
    g_atomic_int_inc (&self->ref_count);
    return self;
}

void
rmfd_sms_part_unref (RmfdSmsPart *self)
{
    if (g_atomic_int_dec_and_test (&self->ref_count)) {
        g_free (self->discharge_timestamp);
        g_free (self->timestamp);
        g_free (self->smsc);
        g_free (self->number);
        g_free (self->text);
        if (self->data)
            g_byte_array_unref (self->data);
        g_slice_free (RmfdSmsPart, self);
    }
}

#define PART_GET_FUNC(type, name)                 \
    type                                          \
    rmfd_sms_part_get_##name (RmfdSmsPart *self)  \
    {                                             \
        return self->name;                        \
    }

#define PART_SET_FUNC(type, name)                 \
    void                                          \
    rmfd_sms_part_set_##name (RmfdSmsPart *self,  \
                              type value)         \
    {                                             \
        self->name = value;                       \
    }

#define PART_SET_TAKE_STR_FUNC(name)                \
    void                                            \
    rmfd_sms_part_set_##name (RmfdSmsPart *self,    \
                              const gchar *value)   \
    {                                               \
        g_free (self->name);                        \
        self->name = g_strdup (value);              \
    }                                               \
                                                    \
    void                                            \
    rmfd_sms_part_take_##name (RmfdSmsPart *self,   \
                               gchar *value)        \
    {                                               \
        g_free (self->name);                        \
        self->name = value;                         \
    }

PART_GET_FUNC (guint, index)
PART_SET_FUNC (guint, index)
PART_GET_FUNC (RmfdSmsPduType, pdu_type)
PART_SET_FUNC (RmfdSmsPduType, pdu_type)
PART_GET_FUNC (const gchar *, smsc)
PART_SET_TAKE_STR_FUNC (smsc)
PART_GET_FUNC (const gchar *, number)
PART_SET_TAKE_STR_FUNC (number)
PART_GET_FUNC (const gchar *, timestamp)
PART_SET_TAKE_STR_FUNC (timestamp)
PART_GET_FUNC (const gchar *, discharge_timestamp)
PART_SET_TAKE_STR_FUNC (discharge_timestamp)
PART_GET_FUNC (guint, concat_max)
PART_SET_FUNC (guint, concat_max)
PART_GET_FUNC (guint, concat_sequence)
PART_SET_FUNC (guint, concat_sequence)
PART_GET_FUNC (const gchar *, text)
PART_SET_TAKE_STR_FUNC (text)
PART_GET_FUNC (RmfdSmsEncoding, encoding)
PART_SET_FUNC (RmfdSmsEncoding, encoding)
PART_GET_FUNC (gint,  class)
PART_SET_FUNC (gint,  class)
PART_GET_FUNC (guint, validity_relative)
PART_SET_FUNC (guint, validity_relative)
PART_GET_FUNC (gboolean, delivery_report_request)
PART_SET_FUNC (gboolean, delivery_report_request)
PART_GET_FUNC (guint, message_reference)
PART_SET_FUNC (guint, message_reference)
PART_GET_FUNC (guint, delivery_state)
PART_SET_FUNC (guint, delivery_state)

PART_GET_FUNC (guint, concat_reference)

void
rmfd_sms_part_set_concat_reference (RmfdSmsPart *self,
                                    guint value)
{
    self->should_concat = TRUE;
    self->concat_reference = value;
}

PART_GET_FUNC (const GByteArray *, data)

void
rmfd_sms_part_set_data (RmfdSmsPart *self,
                        GByteArray *value)
{
    if (self->data)
        g_byte_array_unref (self->data);
    self->data = (value ? g_byte_array_ref (value) : NULL);
}

void
rmfd_sms_part_take_data (RmfdSmsPart *self,
                         GByteArray *value)
{
    if (self->data)
        g_byte_array_unref (self->data);
    self->data = value;
}

gboolean
rmfd_sms_part_should_concat (RmfdSmsPart *self)
{
    return self->should_concat;
}

RmfdSmsPart *
rmfd_sms_part_new (guint ind,
                   RmfdSmsPduType pdu_type)
{
    RmfdSmsPart *sms_part;

    sms_part = g_slice_new0 (RmfdSmsPart);
    sms_part->ref_count = 1;
    sms_part->index = ind;
    sms_part->pdu_type = pdu_type;
    sms_part->encoding = RMFD_SMS_ENCODING_UNKNOWN;
    sms_part->delivery_state = RMFD_SMS_DELIVERY_STATE_UNKNOWN;
    sms_part->class = -1;

    return sms_part;
}

GType
rmfd_sms_part_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile)) {
        GType g_define_type_id =
            g_boxed_type_register_static (g_intern_static_string ("RmfdSmsPart"),
                                          (GBoxedCopyFunc) rmfd_sms_part_ref,
                                          (GBoxedFreeFunc) rmfd_sms_part_unref);

        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

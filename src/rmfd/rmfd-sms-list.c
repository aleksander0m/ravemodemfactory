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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "rmfd-sms-list.h"
#include "rmfd-sms.h"
#include "rmfd-error.h"
#include "rmfd-error-types.h"

G_DEFINE_TYPE (RmfdSmsList, rmfd_sms_list, G_TYPE_OBJECT)

enum {
    SIGNAL_ADDED,
    SIGNAL_LAST
};
static guint signals[SIGNAL_LAST];

struct _RmfdSmsListPrivate {
    /* List of sms objects */
    GList *list;
};

/*****************************************************************************/

static guint
cmp_sms_by_concat_reference (RmfdSms  *sms,
                             gpointer  user_data)
{
    if (!rmfd_sms_is_multipart (sms))
        return -1;

    return (GPOINTER_TO_UINT (user_data) - rmfd_sms_get_multipart_reference (sms));
}

typedef struct {
    guint             part_index;
    QmiWmsStorageType storage;
} PartIndexAndStorage;

static guint
cmp_sms_by_part_index_and_storage (RmfdSms             *sms,
                                   PartIndexAndStorage *ctx)
{
    return !(rmfd_sms_get_storage (sms) == ctx->storage &&
             rmfd_sms_has_part_index (sms, ctx->part_index));
}

static gboolean
take_singlepart (RmfdSmsList           *self,
                 RmfdSmsPart           *part,
                 QmiWmsStorageType      storage,
                 QmiWmsMessageTagType   tag,
                 GError               **error)
{
    RmfdSms *sms;

    sms = rmfd_sms_singlepart_new (storage, part, error);
    if (!sms)
        return FALSE;

    self->priv->list = g_list_prepend (self->priv->list, sms);
    g_signal_emit (self, signals[SIGNAL_ADDED], 0, sms);
    return TRUE;
}

static gboolean
take_multipart (RmfdSmsList           *self,
                RmfdSmsPart           *part,
                QmiWmsStorageType      storage,
                QmiWmsMessageTagType   tag,
                GError               **error)
{
    GList   *l;
    RmfdSms *sms;
    guint    concat_reference;

    concat_reference = rmfd_sms_part_get_concat_reference (part);
    l = g_list_find_custom (self->priv->list,
                            GUINT_TO_POINTER (concat_reference),
                            (GCompareFunc)cmp_sms_by_concat_reference);
    if (l)
        /* Try to take the part */
        return rmfd_sms_multipart_take_part ((RmfdSms *) (l->data), part, error);

    /* Create new Multipart */
    sms = rmfd_sms_multipart_new (storage,
                                  concat_reference,
                                  rmfd_sms_part_get_concat_max (part),
                                  part,
                                  error);
    if (!sms)
        return FALSE;

    self->priv->list = g_list_prepend (self->priv->list, sms);
    g_signal_emit (self, signals[SIGNAL_ADDED], 0, sms);
    return TRUE;
}

gboolean
rmfd_sms_list_take_part (RmfdSmsList           *self,
                         RmfdSmsPart           *part,
                         QmiWmsStorageType      storage,
                         QmiWmsMessageTagType   tag,
                         GError               **error)
{
    PartIndexAndStorage ctx;

    ctx.part_index = rmfd_sms_part_get_index (part);
    ctx.storage    = storage;

    /* Ensure we don't have already taken a part with the same index */
    if (ctx.part_index != SMS_PART_INVALID_INDEX &&
        g_list_find_custom (self->priv->list, &ctx, (GCompareFunc)cmp_sms_by_part_index_and_storage)) {
        g_set_error (error,
                     RMFD_ERROR,
                     RMFD_ERROR_UNKNOWN,
                     "A part with index %u was already taken",
                     ctx.part_index);
        return FALSE;
    }

    /* Did we just get a part of a multi-part SMS? */
    if (rmfd_sms_part_should_concat (part)) {
        if (ctx.part_index != SMS_PART_INVALID_INDEX)
            g_debug ("SMS part at '%s/%u' is from a multipart SMS (reference: '%u', sequence: '%u')",
                     qmi_wms_storage_type_get_string (storage),
                     ctx.part_index,
                     rmfd_sms_part_get_concat_reference (part),
                     rmfd_sms_part_get_concat_sequence (part));
        else
            g_debug ("SMS part (not stored) is from a multipart SMS (reference: '%u', sequence: '%u')",
                     rmfd_sms_part_get_concat_reference (part),
                     rmfd_sms_part_get_concat_sequence (part));

        return take_multipart (self, part, storage, tag, error);
    }

    /* Otherwise, we build a whole new single-partRmfdSms just from this part */
    if (ctx.part_index != SMS_PART_INVALID_INDEX)
        g_debug ("SMS part at '%s/%u' is from a singlepart SMS",
                 qmi_wms_storage_type_get_string (storage),
                 ctx.part_index);
    else
        g_debug ("SMS part (not stored) is from a singlepart SMS");
    return take_singlepart (self, part, storage, tag, error);
}

/*****************************************************************************/

RmfdSmsList *
rmfd_sms_list_new (void)
{
    return g_object_new (RMFD_TYPE_SMS_LIST, NULL);
}

static void
rmfd_sms_list_init (RmfdSmsList *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, RMFD_TYPE_SMS_LIST, RmfdSmsListPrivate);
}

static void
dispose (GObject *object)
{
    RmfdSmsList *self = RMFD_SMS_LIST (object);

    g_list_free_full (self->priv->list, (GDestroyNotify) rmfd_sms_unref);

    G_OBJECT_CLASS (rmfd_sms_list_parent_class)->dispose (object);
}

static void
rmfd_sms_list_class_init (RmfdSmsListClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (RmfdSmsListPrivate));

    /* Virtual methods */
    object_class->dispose = dispose;

    /* Signals */
    signals[SIGNAL_ADDED] =
        g_signal_new ("sms-added",
                      G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (RmfdSmsListClass, sms_added),
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      G_TYPE_NONE, 1, RMFD_TYPE_SMS);
}

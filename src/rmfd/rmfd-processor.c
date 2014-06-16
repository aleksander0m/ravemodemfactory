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
 * Copyright (C) 2013 - 2014 Zodiac Inflight Innovations
 *
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#include "rmfd-processor.h"

G_DEFINE_TYPE (RmfdProcessor, rmfd_processor, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_FILE,
    LAST_PROP
};

struct _RmfdProcessorPrivate {
    /* Device file */
    GFile *file;
};

/**********************/

GFile *
rmfd_processor_peek_file (RmfdProcessor *self)
{
    g_return_val_if_fail (RMFD_IS_PROCESSOR (self), NULL);
    return self->priv->file;
}

/**********************/

GByteArray *
rmfd_processor_run_finish (RmfdProcessor *self,
                           GAsyncResult  *res,
                           GError       **error)
{
    g_assert (RMFD_PROCESSOR_GET_CLASS (self)->run_finish != NULL);
    RMFD_PROCESSOR_GET_CLASS (self)->run_finish (self, res, error);
}

void
rmfd_processor_run (RmfdProcessor       *self,
                    GByteArray          *request,
                    RmfdData            *data,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
    g_assert (RMFD_PROCESSOR_GET_CLASS (self)->run != NULL);
    RMFD_PROCESSOR_GET_CLASS (self)->run (self, request, data, callback, user_data);
}

/*****************************************************************************/

static void
rmfd_processor_init (RmfdProcessor *self)
{
    /* Setup private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, RMFD_TYPE_PROCESSOR, RmfdProcessorPrivate);
}

static void
set_property (GObject *object,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    RmfdProcessorPrivate *priv = RMFD_PROCESSOR (object)->priv;

    switch (prop_id) {
    case PROP_FILE:
        if (priv->file)
            g_object_unref (priv->file);
        priv->file = g_value_dup_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
get_property (GObject *object,
              guint prop_id,
              GValue *value,
              GParamSpec *pspec)
{
    RmfdProcessorPrivate *priv = RMFD_PROCESSOR (object)->priv;

    switch (prop_id) {
    case PROP_FILE:
        g_value_set_object (value, priv->file);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
dispose (GObject *object)
{
    RmfdProcessor *self = RMFD_PROCESSOR (object);

    g_clear_object (&self->priv->file);

    G_OBJECT_CLASS (rmfd_processor_parent_class)->dispose (object);
}

static void
rmfd_processor_class_init (RmfdProcessorClass *processor_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (processor_class);

    g_type_class_add_private (object_class, sizeof (RmfdProcessorPrivate));

    /* Virtual methods */
    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->dispose = dispose;

    /* Properties */
    g_object_class_install_property
        (object_class, PROP_FILE,
         g_param_spec_object (RMFD_PROCESSOR_FILE,
                              "File",
                              "File to control the device",
                              G_TYPE_FILE,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

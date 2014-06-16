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
 * Copyright (C) 2014 Zodiac Inflight Innovations
 *
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#include "rmfd-data.h"

G_DEFINE_TYPE (RmfdData, rmfd_data, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_NAME,
    LAST_PROP
};

struct _RmfdDataPrivate {
    /* Interface name */
    gchar *name;
};

/*****************************************************************************/

const gchar *
rmfd_data_get_name (RmfdData *self)
{
    g_return_val_if_fail (RMFD_IS_DATA (self), NULL);
    return self->priv->name;
}

/*****************************************************************************/
/* Setup */

gboolean
rmfd_data_setup_finish (RmfdData      *self,
                        GAsyncResult  *res,
                        GError       **error)
{
    g_assert (RMFD_DATA_GET_CLASS (self)->setup_finish != NULL);
    return RMFD_DATA_GET_CLASS (self)->setup_finish (self, res, error);
}

void
rmfd_data_setup (RmfdData            *self,
                 gboolean             start,
                 GAsyncReadyCallback  callback,
                 gpointer             user_data)
{
    g_assert (RMFD_DATA_GET_CLASS (self)->setup != NULL);
    RMFD_DATA_GET_CLASS (self)->setup (self, start, callback, user_data);
}

/*****************************************************************************/

static void
rmfd_data_init (RmfdData *self)
{
    /* Setup private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, RMFD_TYPE_DATA, RmfdDataPrivate);
}

static void
set_property (GObject *object,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    RmfdDataPrivate *priv = RMFD_DATA (object)->priv;

    switch (prop_id) {
    case PROP_NAME:
        g_free (priv->name);
        priv->name = g_value_dup_string (value);
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
    RmfdDataPrivate *priv = RMFD_DATA (object)->priv;

    switch (prop_id) {
    case PROP_NAME:
        g_value_set_string (value, priv->name);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
finalize (GObject *object)
{
    RmfdDataPrivate *priv = RMFD_DATA (object)->priv;

    g_free (priv->name);

    G_OBJECT_CLASS (rmfd_data_parent_class)->finalize (object);
}

static void
rmfd_data_class_init (RmfdDataClass *data_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (data_class);

    g_type_class_add_private (object_class, sizeof (RmfdDataPrivate));

    /* Virtual methods */
    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->finalize = finalize;

    /* Properties */
    g_object_class_install_property
        (object_class, PROP_NAME,
         g_param_spec_string (RMFD_DATA_NAME,
                              "Name",
                              "Name of the data interface",
                              NULL,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

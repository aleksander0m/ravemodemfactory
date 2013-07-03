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
 * Copyright (C) 2013 Zodiac Inflight Innovations
 *
 * Author: Aleksander Morgado <aleksander@lanedo.com>
 */

#include <libqmi-glib.h>

#include "rmfd-wwan.h"
#include "rmfd-error.h"
#include "rmfd-error-types.h"

G_DEFINE_TYPE (RmfdWwan, rmfd_wwan, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_NAME,
    LAST_PROP
};

struct _RmfdWwanPrivate {
    /* Interface name */
    gchar *name;
};

/*****************************************************************************/

RmfdWwan *
rmfd_wwan_new (const gchar *name)
{
    return RMFD_WWAN (g_object_new (RMFD_TYPE_WWAN,
                                    RMFD_WWAN_NAME, name,
                                    NULL));
}

/*****************************************************************************/

static void
rmfd_wwan_init (RmfdWwan *self)
{
    /* Setup private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                              RMFD_TYPE_WWAN,
                                              RmfdWwanPrivate);
}

static void
set_property (GObject *object,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    RmfdWwanPrivate *priv = RMFD_WWAN (object)->priv;

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
    RmfdWwanPrivate *priv = RMFD_WWAN (object)->priv;

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
    RmfdWwanPrivate *priv = RMFD_WWAN (object)->priv;

    g_free (priv->name);

    G_OBJECT_CLASS (rmfd_wwan_parent_class)->finalize (object);
}

static void
rmfd_wwan_class_init (RmfdWwanClass *wwan_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (wwan_class);

    g_type_class_add_private (object_class, sizeof (RmfdWwanPrivate));

    /* Virtual methods */
    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->finalize = finalize;

    /* Properties */
    g_object_class_install_property
        (object_class, PROP_NAME,
         g_param_spec_string (RMFD_WWAN_NAME,
                              "Name",
                              "Name of the WWAN interface",
                              NULL,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

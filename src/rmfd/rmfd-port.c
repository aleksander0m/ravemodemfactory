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

#include "rmfd-port.h"

G_DEFINE_TYPE (RmfdPort, rmfd_port, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_INTERFACE,
    LAST_PROP
};

struct _RmfdPortPrivate {
    /* Interface name */
    gchar *interface;
};

/*****************************************************************************/

const gchar *
rmfd_port_get_interface (RmfdPort *self)
{
    g_return_val_if_fail (RMFD_IS_PORT (self), NULL);
    return self->priv->interface;
}

/*****************************************************************************/

static void
rmfd_port_init (RmfdPort *self)
{
    /* Setup private port */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, RMFD_TYPE_PORT, RmfdPortPrivate);
}

static void
set_property (GObject *object,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    RmfdPortPrivate *priv = RMFD_PORT (object)->priv;

    switch (prop_id) {
    case PROP_INTERFACE:
        g_free (priv->interface);
        priv->interface = g_value_dup_string (value);
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
    RmfdPortPrivate *priv = RMFD_PORT (object)->priv;

    switch (prop_id) {
    case PROP_INTERFACE:
        g_value_set_string (value, priv->interface);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
finalize (GObject *object)
{
    RmfdPortPrivate *priv = RMFD_PORT (object)->priv;

    g_free (priv->interface);

    G_OBJECT_CLASS (rmfd_port_parent_class)->finalize (object);
}

static void
rmfd_port_class_init (RmfdPortClass *port_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (port_class);

    g_type_class_add_private (object_class, sizeof (RmfdPortPrivate));

    /* Virtual methods */
    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->finalize = finalize;

    /* Properties */
    g_object_class_install_property
        (object_class, PROP_INTERFACE,
         g_param_spec_string (RMFD_PORT_INTERFACE,
                              "Interface",
                              "Name of the port interface",
                              NULL,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

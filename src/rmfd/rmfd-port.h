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

#ifndef RMFD_PORT_H
#define RMFD_PORT_H

#include <glib-object.h>

#define RMFD_TYPE_PORT            (rmfd_port_get_type ())
#define RMFD_PORT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RMFD_TYPE_PORT, RmfdPort))
#define RMFD_PORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RMFD_TYPE_PORT, RmfdPortClass))
#define RMFD_IS_PORT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RMFD_TYPE_PORT))
#define RMFD_IS_PORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), RMFD_TYPE_PORT))
#define RMFD_PORT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), RMFD_TYPE_PORT, RmfdPortClass))

#define RMFD_PORT_INTERFACE "port-interface"

typedef struct _RmfdPort RmfdPort;
typedef struct _RmfdPortClass RmfdPortClass;
typedef struct _RmfdPortPrivate RmfdPortPrivate;

struct _RmfdPort {
    GObject parent;
    RmfdPortPrivate *priv;
};

struct _RmfdPortClass {
    GObjectClass parent;
};

GType        rmfd_port_get_type      (void);
const gchar *rmfd_port_get_interface (RmfdPort *self);

#endif /* RMFD_PORT_H */

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
 * Copyright (C) 2013-2015 Zodiac Inflight Innovations
 *
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#ifndef RMFD_PORT_DATA_WWAN_H
#define RMFD_PORT_DATA_WWAN_H

#include <glib-object.h>

#include "rmfd-port-data.h"

#define RMFD_TYPE_PORT_DATA_WWAN            (rmfd_port_data_wwan_get_type ())
#define RMFD_PORT_DATA_WWAN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RMFD_TYPE_PORT_DATA_WWAN, RmfdPortDataWwan))
#define RMFD_PORT_DATA_WWAN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RMFD_TYPE_PORT_DATA_WWAN, RmfdPortDataWwanClass))
#define RMFD_IS_PORT_DATA_WWAN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RMFD_TYPE_PORT_DATA_WWAN))
#define RMFD_IS_PORT_DATA_WWAN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), RMFD_TYPE_PORT_DATA_WWAN))
#define RMFD_PORT_DATA_WWAN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), RMFD_TYPE_PORT_DATA_WWAN, RmfdPortDataWwanClass))

typedef struct _RmfdPortDataWwan RmfdPortDataWwan;
typedef struct _RmfdPortDataWwanClass RmfdPortDataWwanClass;

struct _RmfdPortDataWwan {
    RmfdPortData parent;
};

struct _RmfdPortDataWwanClass {
    RmfdPortDataClass parent;
};

GType rmfd_port_data_wwan_get_type (void);

/* Create a wwan data interface */
RmfdPortData *rmfd_port_data_wwan_new (const gchar *interface);

#endif /* RMFD_PORT_DATA_WWAN_H */

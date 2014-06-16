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

#ifndef RMFD_PORT_DATA_H
#define RMFD_PORT_DATA_H

#include <glib-object.h>
#include <gio/gio.h>

#include "rmfd-port.h"

#define RMFD_TYPE_PORT_DATA            (rmfd_port_data_get_type ())
#define RMFD_PORT_DATA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RMFD_TYPE_PORT_DATA, RmfdPortData))
#define RMFD_PORT_DATA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RMFD_TYPE_PORT_DATA, RmfdPortDataClass))
#define RMFD_IS_PORT_DATA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RMFD_TYPE_PORT_DATA))
#define RMFD_IS_PORT_DATA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), RMFD_TYPE_PORT_DATA))
#define RMFD_PORT_DATA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), RMFD_TYPE_PORT_DATA, RmfdPortDataClass))

typedef struct _RmfdPortData RmfdPortData;
typedef struct _RmfdPortDataClass RmfdPortDataClass;

struct _RmfdPortData {
    RmfdPort parent;
};

struct _RmfdPortDataClass {
    RmfdPortClass parent;

    void     (* setup)        (RmfdPortData         *self,
                               gboolean              start,
                               GAsyncReadyCallback   callback,
                               gpointer              user_port_data);
    gboolean (* setup_finish) (RmfdPortData         *self,
                               GAsyncResult         *res,
                               GError              **error);
};

GType        rmfd_port_data_get_type      (void);
void         rmfd_port_data_setup         (RmfdPortData         *self,
                                           gboolean              start,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_port_data);
gboolean     rmfd_port_data_setup_finish  (RmfdPortData         *self,
                                           GAsyncResult         *res,
                                           GError              **error);

#endif /* RMFD_PORT_DATA_H */

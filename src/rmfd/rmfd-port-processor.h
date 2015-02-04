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

#ifndef RMFD_PORT_PROCESSOR_H
#define RMFD_PORT_PROCESSOR_H

#include <glib-object.h>

#include "rmfd-port.h"
#include "rmfd-port-data.h"

#define RMFD_TYPE_PORT_PROCESSOR            (rmfd_port_processor_get_type ())
#define RMFD_PORT_PROCESSOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RMFD_TYPE_PORT_PROCESSOR, RmfdPortProcessor))
#define RMFD_PORT_PROCESSOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RMFD_TYPE_PORT_PROCESSOR, RmfdPortProcessorClass))
#define RMFD_IS_PORT_PROCESSOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RMFD_TYPE_PORT_PROCESSOR))
#define RMFD_IS_PORT_PROCESSOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), RMFD_TYPE_PORT_PROCESSOR))
#define RMFD_PORT_PROCESSOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), RMFD_TYPE_PORT_PROCESSOR, RmfdPortProcessorClass))

typedef struct _RmfdPortProcessor RmfdPortProcessor;
typedef struct _RmfdPortProcessorClass RmfdPortProcessorClass;

struct _RmfdPortProcessor {
    RmfdPort parent;
};

struct _RmfdPortProcessorClass {
    RmfdPortClass parent;

    void         (* run)        (RmfdPortProcessor    *self,
                                 GByteArray           *request,
                                 RmfdPortData         *data,
                                 GAsyncReadyCallback   callback,
                                 gpointer              user_data);
    GByteArray * (* run_finish) (RmfdPortProcessor    *self,
                                 GAsyncResult         *res,
                                 GError              **error);
};

GType       rmfd_port_processor_get_type   (void);
void        rmfd_port_processor_run        (RmfdPortProcessor    *self,
                                            GByteArray           *request,
                                            RmfdPortData         *data,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
GByteArray *rmfd_port_processor_run_finish (RmfdPortProcessor    *self,
                                            GAsyncResult         *res,
                                            GError              **error);

#endif /* RMFD_PORT_PROCESSOR_H */

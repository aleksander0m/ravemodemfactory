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
 * Copyright (C) 2013-2015 Safran Passenger Innovations
 *
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#include "rmfd-port-processor.h"

G_DEFINE_TYPE (RmfdPortProcessor, rmfd_port_processor, RMFD_TYPE_PORT)

/**********************/

GByteArray *
rmfd_port_processor_run_finish (RmfdPortProcessor  *self,
                                GAsyncResult       *res,
                                GError            **error)
{
    g_assert (RMFD_PORT_PROCESSOR_GET_CLASS (self)->run_finish != NULL);
    return RMFD_PORT_PROCESSOR_GET_CLASS (self)->run_finish (self, res, error);
}

void
rmfd_port_processor_run (RmfdPortProcessor   *self,
                         GByteArray          *request,
                         RmfdPortData        *data,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
    g_assert (RMFD_PORT_PROCESSOR_GET_CLASS (self)->run != NULL);
    RMFD_PORT_PROCESSOR_GET_CLASS (self)->run (self, request, data, callback, user_data);
}

/*****************************************************************************/

static void
rmfd_port_processor_init (RmfdPortProcessor *self)
{
}

static void
rmfd_port_processor_class_init (RmfdPortProcessorClass *processor_class)
{
}

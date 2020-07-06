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
 * Copyright (C) 2014 Safran Passenger Innovations
 *
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#include "rmfd-port-data.h"

G_DEFINE_TYPE (RmfdPortData, rmfd_port_data, RMFD_TYPE_PORT)

/*****************************************************************************/
/* Setup */

gboolean
rmfd_port_data_setup_finish (RmfdPortData  *self,
                             GAsyncResult  *res,
                             GError       **error)
{
    g_assert (RMFD_PORT_DATA_GET_CLASS (self)->setup_finish != NULL);
    return RMFD_PORT_DATA_GET_CLASS (self)->setup_finish (self, res, error);
}

void
rmfd_port_data_setup (RmfdPortData        *self,
                      gboolean             start,
                      const gchar         *ip_address,
                      const gchar         *netmask_address,
                      const gchar         *gateway_address,
                      const gchar         *dns1_address,
                      const gchar         *dns2_address,
                      guint32              mtu,
                      GAsyncReadyCallback  callback,
                      gpointer             user_port_data)
{
    g_assert (RMFD_PORT_DATA_GET_CLASS (self)->setup != NULL);
    RMFD_PORT_DATA_GET_CLASS (self)->setup (self,
                                            start,
                                            ip_address,
                                            netmask_address,
                                            gateway_address,
                                            dns1_address,
                                            dns2_address,
                                            mtu,
                                            callback,
                                            user_port_data);
}

/*****************************************************************************/

static void
rmfd_port_data_init (RmfdPortData *self)
{
}

static void
rmfd_port_data_class_init (RmfdPortDataClass *data_class)
{
}

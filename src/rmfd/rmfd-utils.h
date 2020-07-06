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

#ifndef RMFD_UTILS_H
#define RMFD_UTILS_H

#include <gudev/gudev.h>
#include <glib.h>

guint8 rmfd_utils_get_mnc_length_for_mcc (const gchar *mcc);

typedef enum {
    RMFD_MODEM_TYPE_UNKNOWN = 0,
    RMFD_MODEM_TYPE_QMI     = 1,
} RmfdModemType;

RmfdModemType rmfd_utils_get_modem_type (GUdevDevice *device);

gchar *rmfd_utils_build_interface_name (GUdevDevice *device);

GUdevDevice *rmfd_utils_get_physical_device (GUdevDevice *child);

#endif /* RMFD_UTILS_H */

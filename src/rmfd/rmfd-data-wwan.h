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
 * Copyright (C) 2013 - 2014 Zodiac Inflight Innovations
 *
 * Author: Aleksander Morgado <aleksander@lanedo.com>
 */

#ifndef RMFD_DATA_WWAN_H
#define RMFD_DATA_WWAN_H

#include <glib-object.h>
#include <gio/gio.h>

#include "rmfd-data.h"

#define RMFD_TYPE_DATA_WWAN            (rmfd_data_wwan_get_type ())
#define RMFD_DATA_WWAN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RMFD_TYPE_DATA_WWAN, RmfdDataWwan))
#define RMFD_DATA_WWAN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RMFD_TYPE_DATA_WWAN, RmfdDataWwanClass))
#define RMFD_IS_DATA_WWAN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RMFD_TYPE_DATA_WWAN))
#define RMFD_IS_DATA_WWAN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), RMFD_TYPE_DATA_WWAN))
#define RMFD_DATA_WWAN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), RMFD_TYPE_DATA_WWAN, RmfdDataWwanClass))

typedef struct _RmfdDataWwan RmfdDataWwan;
typedef struct _RmfdDataWwanClass RmfdDataWwanClass;

struct _RmfdDataWwan {
    RmfdData parent;
};

struct _RmfdDataWwanClass {
    RmfdDataClass parent;
};

GType rmfd_data_wwan_get_type (void);

/* Create a wwan data interface */
RmfdData *rmfd_data_wwan_new (const gchar *name);

#endif /* RMFD_DATA_WWAN_H */

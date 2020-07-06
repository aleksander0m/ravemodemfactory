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

#ifndef RMFD_MANAGER_H
#define RMFD_MANAGER_H

#include <glib-object.h>
#include <gio/gio.h>

#define RMFD_TYPE_MANAGER            (rmfd_manager_get_type ())
#define RMFD_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RMFD_TYPE_MANAGER, RmfdManager))
#define RMFD_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RMFD_TYPE_MANAGER, RmfdManagerClass))
#define RMFD_IS_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RMFD_TYPE_MANAGER))
#define RMFD_IS_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), RMFD_TYPE_MANAGER))
#define RMFD_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), RMFD_TYPE_MANAGER, RmfdManagerClass))

typedef struct _RmfdManager RmfdManager;
typedef struct _RmfdManagerClass RmfdManagerClass;
typedef struct _RmfdManagerPrivate RmfdManagerPrivate;

struct _RmfdManager {
    GObject parent;
    RmfdManagerPrivate *priv;
};

struct _RmfdManagerClass {
    GObjectClass parent;
};

GType rmfd_manager_get_type (void);

RmfdManager *rmfd_manager_new_unix (void);
RmfdManager *rmfd_manager_new_tcp  (const gchar *address,
                                    guint16      port);

#endif /* RMFD_MANAGER_H */

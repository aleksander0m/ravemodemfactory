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
 * Copyright (C) 2013 Zodiac Inflight Innovations
 *
 * Author: Aleksander Morgado <aleksander@lanedo.com>
 */

#ifndef RMFD_WWAN_H
#define RMFD_WWAN_H

#include <glib-object.h>
#include <gio/gio.h>
#include <libqmi-glib.h>
#include <rmf-messages.h>

#define RMFD_TYPE_WWAN            (rmfd_wwan_get_type ())
#define RMFD_WWAN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RMFD_TYPE_WWAN, RmfdWwan))
#define RMFD_WWAN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RMFD_TYPE_WWAN, RmfdWwanClass))
#define RMFD_IS_WWAN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RMFD_TYPE_WWAN))
#define RMFD_IS_WWAN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), RMFD_TYPE_WWAN))
#define RMFD_WWAN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), RMFD_TYPE_WWAN, RmfdWwanClass))

#define RMFD_WWAN_NAME "wwan-name"

typedef struct _RmfdWwan RmfdWwan;
typedef struct _RmfdWwanClass RmfdWwanClass;
typedef struct _RmfdWwanPrivate RmfdWwanPrivate;

struct _RmfdWwan {
    GObject parent;
    RmfdWwanPrivate *priv;
};

struct _RmfdWwanClass {
    GObjectClass parent;
};

GType rmfd_wwan_get_type (void);

/* Create a wwan */
RmfdWwan *rmfd_wwan_new (const gchar *name);

/* Setup  */
void     rmfd_wwan_setup        (RmfdWwan             *self,
                                 gboolean              start,
                                 GAsyncReadyCallback   callback,
                                 gpointer              user_data);
gboolean rmfd_wwan_setup_finish (RmfdWwan             *self,
                                 GAsyncResult         *res,
                                 GError              **error);


#endif /* RMFD_WWAN_H */

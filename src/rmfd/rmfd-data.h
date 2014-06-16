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

#ifndef RMFD_DATA_H
#define RMFD_DATA_H

#include <glib-object.h>
#include <gio/gio.h>
#include <libqmi-glib.h>
#include <rmf-messages.h>

#define RMFD_TYPE_DATA            (rmfd_data_get_type ())
#define RMFD_DATA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RMFD_TYPE_DATA, RmfdData))
#define RMFD_DATA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RMFD_TYPE_DATA, RmfdDataClass))
#define RMFD_IS_DATA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RMFD_TYPE_DATA))
#define RMFD_IS_DATA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), RMFD_TYPE_DATA))
#define RMFD_DATA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), RMFD_TYPE_DATA, RmfdDataClass))

#define RMFD_DATA_NAME "data-name"

typedef struct _RmfdData RmfdData;
typedef struct _RmfdDataClass RmfdDataClass;
typedef struct _RmfdDataPrivate RmfdDataPrivate;

struct _RmfdData {
    GObject parent;
    RmfdDataPrivate *priv;
};

struct _RmfdDataClass {
    GObjectClass parent;

    /* Setup data connection */
    void     (* setup)        (RmfdData             *self,
                               gboolean              start,
                               GAsyncReadyCallback   callback,
                               gpointer              user_data);
    gboolean (* setup_finish) (RmfdData             *self,
                               GAsyncResult         *res,
                               GError              **error);
};

GType rmfd_data_get_type (void);

const gchar *rmfd_data_get_name     (RmfdData *self);
void         rmfd_data_setup        (RmfdData             *self,
                                     gboolean              start,
                                     GAsyncReadyCallback   callback,
                                     gpointer              user_data);
gboolean     rmfd_data_setup_finish (RmfdData             *self,
                                     GAsyncResult         *res,
                                     GError              **error);


#endif /* RMFD_DATA_H */

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
 * Copyright (C) 2015 Safran Passenger Innovations
 *
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#ifndef RMFD_SMS_LIST_H
#define RMFD_SMS_LIST_H

#include <glib.h>
#include <glib-object.h>
#include <libqmi-glib.h>

#include "rmfd-sms.h"
#include "rmfd-sms-part.h"

#define RMFD_TYPE_SMS_LIST            (rmfd_sms_list_get_type ())
#define RMFD_SMS_LIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RMFD_TYPE_SMS_LIST, RmfdSmsList))
#define RMFD_SMS_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  RMFD_TYPE_SMS_LIST, RmfdSmsListClass))
#define RMFD_IS_SMS_LIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RMFD_TYPE_SMS_LIST))
#define RMFD_IS_SMS_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  RMFD_TYPE_SMS_LIST))
#define RMFD_SMS_LIST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  RMFD_TYPE_SMS_LIST, RmfdSmsListClass))

typedef struct _RmfdSmsList RmfdSmsList;
typedef struct _RmfdSmsListClass RmfdSmsListClass;
typedef struct _RmfdSmsListPrivate RmfdSmsListPrivate;

struct _RmfdSmsList {
    GObject parent;
    RmfdSmsListPrivate *priv;
};

struct _RmfdSmsListClass {
    GObjectClass parent;

    /* Signals */
    void (* sms_added) (RmfdSmsList *self,
                        RmfdSms     *sms);
};

GType rmfd_sms_list_get_type (void);

RmfdSmsList *rmfd_sms_list_new       (void);
gboolean     rmfd_sms_list_take_part (RmfdSmsList           *self,
                                      RmfdSmsPart           *part,
                                      QmiWmsStorageType      storage,
                                      QmiWmsMessageTagType   tag,
                                      GError               **error);

#endif /* RMFD_SMS_LIST_H */

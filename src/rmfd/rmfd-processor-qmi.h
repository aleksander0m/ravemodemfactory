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
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#ifndef RMFD_PROCESSOR_QMI_H
#define RMFD_PROCESSOR_QMI_H

#include <glib-object.h>
#include <gio/gio.h>

#include "rmfd-processor.h"

#define RMFD_TYPE_PROCESSOR_QMI            (rmfd_processor_qmi_get_type ())
#define RMFD_PROCESSOR_QMI(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RMFD_TYPE_PROCESSOR_QMI, RmfdProcessorQmi))
#define RMFD_PROCESSOR_QMI_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RMFD_TYPE_PROCESSOR_QMI, RmfdProcessorQmiClass))
#define RMFD_IS_PROCESSOR_QMI(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RMFD_TYPE_PROCESSOR_QMI))
#define RMFD_IS_PROCESSOR_QMI_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), RMFD_TYPE_PROCESSOR_QMI))
#define RMFD_PROCESSOR_QMI_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), RMFD_TYPE_PROCESSOR_QMI, RmfdProcessorQmiClass))

typedef struct _RmfdProcessorQmi RmfdProcessorQmi;
typedef struct _RmfdProcessorQmiClass RmfdProcessorQmiClass;
typedef struct _RmfdProcessorQmiPrivate RmfdProcessorQmiPrivate;

struct _RmfdProcessorQmi {
    RmfdProcessor parent;
    RmfdProcessorQmiPrivate *priv;
};

struct _RmfdProcessorQmiClass {
    RmfdProcessorClass parent;
};

GType rmfd_processor_qmi_get_type (void);

/* Create a QMI processor */
void           rmfd_processor_qmi_new        (GFile                *file,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
RmfdProcessor *rmfd_processor_qmi_new_finish (GAsyncResult         *res,
                                              GError              **error);

#endif /* RMFD_PROCESSOR_QMI_H */

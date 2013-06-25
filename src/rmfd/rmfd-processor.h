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
 */

#ifndef RMFD_PROCESSOR_H
#define RMFD_PROCESSOR_H

#include <gio/gio.h>
#include <libqmi-glib.h>
#include <rmf-messages.h>

/* Processes the request and gets back a response */
void          rmfd_processor_run        (QmiDevice            *device,
                                         const guint8         *request,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data);
const guint8 *rmfd_processor_run_finish (QmiDevice            *device,
                                         GAsyncResult         *res,
                                         GError              **error);

#endif /* RMFD_PROCESSOR_H */

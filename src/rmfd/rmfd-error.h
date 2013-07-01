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

#ifndef RMFD_ERROR_H
#define RMFD_ERROR_H

#include <glib.h>

typedef enum {
    RMFD_ERROR_UNKNOWN         = 1,
    RMFD_ERROR_INVALID_REQUEST = 2,
    RMFD_ERROR_UNKNOWN_COMMAND = 3,
    RMFD_ERROR_NO_MODEM        = 4,
} RmfdError;

GByteArray *rmfd_error_message_new_from_error  (const GByteArray *request,
                                                GQuark            error_domain,
                                                gint              error_code);

GByteArray *rmfd_error_message_new_from_gerror (const GByteArray *request,
                                                const GError     *error);

#endif /* RMFD_ERROR_H */

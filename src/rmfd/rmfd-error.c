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

#include "rmfd-error.h"
#include "rmfd-error-types.h"

#include <libqmi-glib.h>
#include <rmf-messages.h>

GByteArray *
rmfd_error_message_new_from_error (const GByteArray *request,
                                   GQuark            error_domain,
                                   gint              error_code,
                                   const gchar      *msg)
{
    guint32 status;
    guint8 *buffer;

    if (error_domain == RMFD_ERROR) {
        switch (error_code) {
        case RMFD_ERROR_UNKNOWN:
            status = RMF_RESPONSE_STATUS_ERROR_UNKNOWN;
            break;
        case RMFD_ERROR_INVALID_REQUEST:
            status = RMF_RESPONSE_STATUS_ERROR_INVALID_REQUEST;
            break;
        case RMFD_ERROR_UNKNOWN_COMMAND:
            status = RMF_RESPONSE_STATUS_ERROR_UNKNOWN_COMMAND;
            break;
        case RMFD_ERROR_NO_MODEM:
            status = RMF_RESPONSE_STATUS_ERROR_NO_MODEM;
            break;
        case RMFD_ERROR_INVALID_STATE:
            status = RMF_RESPONSE_STATUS_ERROR_INVALID_STATE;
            break;
        case RMFD_ERROR_INVALID_INPUT:
            status = RMF_RESPONSE_STATUS_ERROR_INVALID_INPUT;
            break;
        case RMFD_ERROR_NOT_SUPPORTED:
            status = RMF_RESPONSE_STATUS_ERROR_NOT_SUPPORTED_INTERNAL;
            break;
        default:
            g_assert_not_reached ();
        }
    } else if (error_domain == QMI_PROTOCOL_ERROR) {
        if (error_code <= QMI_PROTOCOL_ERROR_OPERATION_IN_PROGRESS)
            status = 100 + error_code;
        else
            status = RMF_RESPONSE_STATUS_ERROR_UNKNOWN;
    } else
        status = RMF_RESPONSE_STATUS_ERROR_UNKNOWN;

    buffer = rmf_message_error_response_new (rmf_message_get_command (request->data), status, msg);
    return g_byte_array_new_take (buffer, rmf_message_get_length (buffer));
}


GByteArray *
rmfd_error_message_new_from_gerror (const GByteArray *request,
                                    const GError     *error)
{
    return rmfd_error_message_new_from_error (request, error->domain, error->code, error->message);
}

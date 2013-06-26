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

#include "rmfd-error.h"
#include "rmfd-error-types.h"

#include <libqmi-glib.h>
#include <rmf-messages.h>

guint8 *
rmfd_error_message_new_from_gerror (const guint8 *request,
                                    const GError *error)
{
    guint32 status;

    if (error->domain == RMFD_ERROR) {
        switch (error->code) {
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
        default:
            g_assert_not_reached ();
        }
    } else if (error->domain == QMI_PROTOCOL_ERROR) {
        switch (error->code) {
        case QMI_PROTOCOL_ERROR_UIM_UNINITIALIZED:
            status = RMF_RESPONSE_STATUS_ERROR_PIN_REQUIRED;
            break;
        case QMI_PROTOCOL_ERROR_PIN_BLOCKED:
            status = RMF_RESPONSE_STATUS_ERROR_PUK_REQUIRED;
            break;
        case QMI_PROTOCOL_ERROR_NO_SIM:
        case QMI_PROTOCOL_ERROR_PIN_ALWAYS_BLOCKED:
            status = RMF_RESPONSE_STATUS_ERROR_SIM_ERROR;
            break;
        default:
            status = RMF_RESPONSE_STATUS_ERROR_UNKNOWN;
        }
    } else
        status = RMF_RESPONSE_STATUS_ERROR_UNKNOWN;

    return rmf_message_error_response_new (rmf_message_get_command (request), status);
}

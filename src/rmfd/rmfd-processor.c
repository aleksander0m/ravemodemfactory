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

#include "rmfd-processor.h"
#include "rmfd-error.h"
#include "rmfd-error-types.h"

const guint8 *
rmfd_processor_run_finish (QmiDevice     *device,
                           GAsyncResult  *res,
                           GError       **error)
{
    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return NULL;

    return g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res));
}

void
rmfd_processor_run (QmiDevice           *device,
                    const guint8        *request,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
    if (rmf_message_get_type (request) != RMF_MESSAGE_TYPE_REQUEST) {
        g_simple_async_report_error_in_idle (
            G_OBJECT (device),
            callback,
            user_data,
            RMFD_ERROR,
            RMFD_ERROR_INVALID_REQUEST,
            "Received message is not a request");
        return;
    }

    g_simple_async_report_error_in_idle (
        G_OBJECT (device),
        callback,
        user_data,
        RMFD_ERROR,
        RMFD_ERROR_UNKNOWN_COMMAND,
        "Unknown command received (0x%X)",
        rmf_message_get_command (request));
}

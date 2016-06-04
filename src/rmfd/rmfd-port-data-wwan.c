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
 * Copyright (C) 2013-2015 Zodiac Inflight Innovations
 *
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#include <string.h>

#include "rmfd-port-data-wwan.h"
#include "rmfd-error.h"
#include "rmfd-error-types.h"

G_DEFINE_TYPE (RmfdPortDataWwan, rmfd_port_data_wwan, RMFD_TYPE_PORT_DATA)

/*****************************************************************************/
/* Setup */

typedef struct {
    RmfdPortDataWwan *self;
    GSimpleAsyncResult *result;
    gboolean start;
} SetupContext;

static void
setup_context_complete_and_free (SetupContext *ctx)
{
    g_simple_async_result_complete_in_idle (ctx->result);
    g_object_unref (ctx->result);
    g_object_unref (ctx->self);
    g_slice_free (SetupContext, ctx);
}

static gboolean
setup_finish (RmfdPortData  *self,
              GAsyncResult  *res,
              GError       **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

static void
command_ready (GPid          pid,
               gint          status,
               SetupContext *ctx)
{
    if (status != 0) {
        g_simple_async_result_set_error (
            ctx->result,
            RMFD_ERROR,
            RMFD_ERROR_UNKNOWN,
            "couldn't %s WWAN interface '%s': failed with code %d",
            ctx->start ? "start" : "stop",
            rmfd_port_get_interface (RMFD_PORT (ctx->self)),
            status);
        setup_context_complete_and_free (ctx);
        return;
    }

    /* Done */
    g_debug ("WWAN interface '%s' is now %s",
             rmfd_port_get_interface (RMFD_PORT (ctx->self)),
             ctx->start ? "started" : "stopped");
    g_simple_async_result_set_op_res_gboolean (ctx->result, TRUE);
    setup_context_complete_and_free (ctx);
}

static void
setup (RmfdPortData        *self,
       gboolean             start,
       const gchar         *ip_address,
       const gchar         *netmask_address,
       const gchar         *gateway_address,
       const gchar         *dns1_address,
       const gchar         *dns2_address,
       guint32              mtu,
       GAsyncReadyCallback  callback,
       gpointer             user_port_data)
{
#define MAX_ARGS 10 /* 8 + 1 last NULL */

    SetupContext *ctx;
    gchar *command_split[MAX_ARGS];
    GError *error = NULL;
    GPid pid;

    ctx = g_slice_new (SetupContext);
    ctx->self = g_object_ref (self);
    ctx->result = g_simple_async_result_new (G_OBJECT (self), callback, user_port_data, setup);
    ctx->start = start;

    /* Build command */
    memset (command_split, 0, sizeof (command_split));
    command_split[0] = "rmfd-port-data-wwan-service";
    command_split[1] = (gchar *) rmfd_port_get_interface (RMFD_PORT (ctx->self));
    if (!ip_address) {
        command_split[2] = ctx->start ? "start" : "stop";
        g_debug ("%s WWAN interface '%s'",
                 ctx->start ? "starting" : "stopping",
                 rmfd_port_get_interface (RMFD_PORT (ctx->self)));
    } else {
        g_assert (start);
        command_split[2] = "static";
        command_split[3] = (gchar *) (ip_address);
        command_split[4] = (gchar *) (netmask_address ? netmask_address : "-");
        command_split[5] = (gchar *) (gateway_address ? gateway_address : "-");
        command_split[6] = (gchar *) (dns1_address ? dns1_address : "-");
        command_split[7] = (gchar *) (dns2_address ? dns2_address : "-");
        command_split[8] = mtu ? g_strdup_printf ("%u", mtu) : g_strdup ("-");

        g_debug ("starting WWAN interface '%s': (static) [%s, %s, %s, %s, %s, %s]",
                 rmfd_port_get_interface (RMFD_PORT (ctx->self)),
                 command_split[3],
                 command_split[4],
                 command_split[5],
                 command_split[6],
                 command_split[7],
                 command_split[8]);
    }

    g_spawn_async (NULL, /* working directory */
                   command_split, /* argv */
                   NULL, /* envp */
                   (G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH),
                   NULL, /* child_setup */
                   NULL, /* user_port_data */
                   &pid,
                   &error);

    g_free (command_split[8]);

    if (error) {
        g_prefix_error (&error,
                        "couldn't %s WWAN interface '%s': ",
                        ctx->start ? "start" : "stop",
                        rmfd_port_get_interface (RMFD_PORT (ctx->self)));
        g_debug ("error: %s", error->message);
        g_simple_async_result_take_error (ctx->result, error);
        setup_context_complete_and_free (ctx);
        return;
    }

    /* wait for pid to exit */
    g_child_watch_add (pid, (GChildWatchFunc)command_ready, ctx);
}

/*****************************************************************************/

RmfdPortData *
rmfd_port_data_wwan_new (const gchar *interface)
{
    return RMFD_PORT_DATA (g_object_new (RMFD_TYPE_PORT_DATA_WWAN,
                                         RMFD_PORT_INTERFACE, interface,
                                         NULL));
}

/*****************************************************************************/

static void
rmfd_port_data_wwan_init (RmfdPortDataWwan *self)
{
}

static void
rmfd_port_data_wwan_class_init (RmfdPortDataWwanClass *wwan_class)
{
    RmfdPortDataClass *data_class = RMFD_PORT_DATA_CLASS (wwan_class);

    /* Virtual methods */
    data_class->setup = setup;
    data_class->setup_finish = setup_finish;
}

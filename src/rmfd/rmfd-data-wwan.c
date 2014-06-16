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

#include "rmfd-data-wwan.h"
#include "rmfd-error.h"
#include "rmfd-error-types.h"

G_DEFINE_TYPE (RmfdDataWwan, rmfd_data_wwan, RMFD_TYPE_DATA)

/*****************************************************************************/
/* Setup */

typedef struct {
    RmfdDataWwan *self;
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
setup_finish (RmfdData      *self,
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
            rmfd_data_get_name (RMFD_DATA (ctx->self)),
            status);
        setup_context_complete_and_free (ctx);
        return;
    }

    /* Done */
    g_debug ("WWAN interface '%s' is now %s",
             rmfd_data_get_name (RMFD_DATA (ctx->self)),
             ctx->start ? "started" : "stopped");
    g_simple_async_result_set_op_res_gboolean (ctx->result, TRUE);
    setup_context_complete_and_free (ctx);
}

static void
setup (RmfdData            *self,
       gboolean             start,
       GAsyncReadyCallback  callback,
       gpointer             user_data)
{
    SetupContext *ctx;
    gchar *command;
    gchar **command_split;
    GError *error = NULL;
    GPid pid;

    ctx = g_slice_new (SetupContext);
    ctx->self = g_object_ref (self);
    ctx->result = g_simple_async_result_new (G_OBJECT (self), callback, user_data, setup);
    ctx->start = start;

    /* Build command */
    command = g_strdup_printf ("rmfd-data-wwan-service %s %s",
                               rmfd_data_get_name (RMFD_DATA (ctx->self)),
                               ctx->start ? "start" : "stop");
    command_split = g_strsplit (command, " ", -1);
    g_debug ("%s WWAN interface '%s': %s",
             ctx->start ? "starting" : "stopping",
             rmfd_data_get_name (RMFD_DATA (ctx->self)),
             command);
    g_spawn_async (NULL, /* working directory */
                   command_split, /* argv */
                   NULL, /* envp */
                   (G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH),
                   NULL, /* child_setup */
                   NULL, /* user_data */
                   &pid,
                   &error);
    g_strfreev (command_split);
    g_free (command);

    if (error) {
        g_prefix_error (&error,
                        "couldn't %s WWAN interface '%s': ",
                        ctx->start ? "start" : "stop",
                        rmfd_data_get_name (RMFD_DATA (ctx->self)));
        g_simple_async_result_take_error (ctx->result, error);
        setup_context_complete_and_free (ctx);
        return;
    }

    /* wait for pid to exit */
    g_child_watch_add (pid, (GChildWatchFunc)command_ready, ctx);
}

/*****************************************************************************/

RmfdData *
rmfd_data_wwan_new (const gchar *name)
{
    return RMFD_DATA (g_object_new (RMFD_TYPE_DATA_WWAN,
                                    RMFD_DATA_NAME, name,
                                    NULL));
}

/*****************************************************************************/

static void
rmfd_data_wwan_init (RmfdDataWwan *self)
{
}

static void
rmfd_data_wwan_class_init (RmfdDataWwanClass *wwan_class)
{
    RmfdDataClass *data_class = RMFD_DATA_CLASS (wwan_class);

    /* Virtual methods */
    data_class->setup = setup;
    data_class->setup_finish = setup_finish;
}

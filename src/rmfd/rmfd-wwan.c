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

#include <libqmi-glib.h>

#include "rmfd-wwan.h"
#include "rmfd-error.h"
#include "rmfd-error-types.h"

G_DEFINE_TYPE (RmfdWwan, rmfd_wwan, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_NAME,
    LAST_PROP
};

struct _RmfdWwanPrivate {
    /* Interface name */
    gchar *name;
};

/*****************************************************************************/
/* Setup */

typedef struct {
    RmfdWwan *self;
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

gboolean
rmfd_wwan_setup_finish (RmfdWwan      *self,
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
            ctx->self->priv->name,
            status);
        setup_context_complete_and_free (ctx);
        return;
    }

    /* Done */
    g_debug ("WWAN interface '%s' is now %s",
             ctx->self->priv->name,
             ctx->start ? "started" : "stopped");
    g_simple_async_result_set_op_res_gboolean (ctx->result, TRUE);
    setup_context_complete_and_free (ctx);
}

void
rmfd_wwan_setup (RmfdWwan            *self,
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
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             rmfd_wwan_setup);
    ctx->start = start;

    /* Build command */
    command = g_strdup_printf ("rmfd-wwan-service %s %s",
                               ctx->self->priv->name,
                               ctx->start ? "start" : "stop");
    command_split = g_strsplit (command, " ", -1);
    g_debug ("%s WWAN interface '%s': %s",
             ctx->start ? "starting" : "stopping",
             ctx->self->priv->name,
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
                        ctx->self->priv->name);
        g_simple_async_result_take_error (ctx->result, error);
        setup_context_complete_and_free (ctx);
        return;
    }

    /* wait for pid to exit */
    g_child_watch_add (pid, (GChildWatchFunc)command_ready, ctx);
}

/*****************************************************************************/

RmfdWwan *
rmfd_wwan_new (const gchar *name)
{
    return RMFD_WWAN (g_object_new (RMFD_TYPE_WWAN,
                                    RMFD_WWAN_NAME, name,
                                    NULL));
}

/*****************************************************************************/

static void
rmfd_wwan_init (RmfdWwan *self)
{
    /* Setup private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                              RMFD_TYPE_WWAN,
                                              RmfdWwanPrivate);
}

static void
set_property (GObject *object,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    RmfdWwanPrivate *priv = RMFD_WWAN (object)->priv;

    switch (prop_id) {
    case PROP_NAME:
        g_free (priv->name);
        priv->name = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
get_property (GObject *object,
              guint prop_id,
              GValue *value,
              GParamSpec *pspec)
{
    RmfdWwanPrivate *priv = RMFD_WWAN (object)->priv;

    switch (prop_id) {
    case PROP_NAME:
        g_value_set_string (value, priv->name);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
finalize (GObject *object)
{
    RmfdWwanPrivate *priv = RMFD_WWAN (object)->priv;

    g_free (priv->name);

    G_OBJECT_CLASS (rmfd_wwan_parent_class)->finalize (object);
}

static void
rmfd_wwan_class_init (RmfdWwanClass *wwan_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (wwan_class);

    g_type_class_add_private (object_class, sizeof (RmfdWwanPrivate));

    /* Virtual methods */
    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->finalize = finalize;

    /* Properties */
    g_object_class_install_property
        (object_class, PROP_NAME,
         g_param_spec_string (RMFD_WWAN_NAME,
                              "Name",
                              "Name of the WWAN interface",
                              NULL,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

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

#include <libqmi-glib.h>

#include <rmf-messages.h>

#include "rmfd-processor.h"
#include "rmfd-error.h"
#include "rmfd-error-types.h"

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (RmfdProcessor, rmfd_processor, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               async_initable_iface_init))

enum {
    PROP_0,
    PROP_FILE,
    LAST_PROP
};

struct _RmfdProcessorPrivate {
    /* QMI device file */
    GFile *file;
    /* QMI device */
    QmiDevice *qmi_device;
};

/*****************************************************************************/
/* Process an action */

const guint8 *
rmfd_processor_run_finish (RmfdProcessor *self,
                           GAsyncResult  *res,
                           GError       **error)
{
    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return NULL;

    return g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res));
}

void
rmfd_processor_run (RmfdProcessor       *self,
                    const guint8        *request,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
    if (rmf_message_get_type (request) != RMF_MESSAGE_TYPE_REQUEST) {
        g_simple_async_report_error_in_idle (
            G_OBJECT (self),
            callback,
            user_data,
            RMFD_ERROR,
            RMFD_ERROR_INVALID_REQUEST,
            "Received message is not a request");
        return;
    }

    g_simple_async_report_error_in_idle (
        G_OBJECT (self),
        callback,
        user_data,
        RMFD_ERROR,
        RMFD_ERROR_UNKNOWN_COMMAND,
        "Unknown command received (0x%X)",
        rmf_message_get_command (request));
}

/*****************************************************************************/
/* Processor init */

typedef struct {
    RmfdProcessor *self;
    GSimpleAsyncResult *result;
} InitContext;

static void
init_context_complete_and_free (InitContext *ctx)
{
    g_simple_async_result_complete (ctx->result);
    g_object_unref (ctx->result);
    g_object_unref (ctx->self);
    g_slice_free (InitContext, ctx);
}

static gboolean
initable_init_finish (GAsyncInitable  *initable,
                      GAsyncResult    *result,
                      GError         **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error);
}

static void
device_open_ready (QmiDevice    *qmi_device,
                   GAsyncResult *res,
                   InitContext  *ctx)
{
    GError *error = NULL;

    if (!qmi_device_open_finish (qmi_device, res, &error)) {
        g_simple_async_result_take_error (ctx->result, error);
        init_context_complete_and_free (ctx);
        return;
    }

    g_debug ("QmiDevice opened: %s", qmi_device_get_path (ctx->self->priv->qmi_device));
    g_simple_async_result_set_op_res_gboolean (ctx->result, TRUE);
    init_context_complete_and_free (ctx);
}

static void
device_new_ready (GObject      *source,
                  GAsyncResult *res,
                  InitContext  *ctx)
{
    GError *error = NULL;

    ctx->self->priv->qmi_device = qmi_device_new_finish (res, &error);
    if (!ctx->self->priv->qmi_device) {
        g_simple_async_result_take_error (ctx->result, error);
        init_context_complete_and_free (ctx);
        return;
    }

    g_debug ("QmiDevice created: %s", qmi_device_get_path (ctx->self->priv->qmi_device));

    /* Open the QMI port */
    qmi_device_open (ctx->self->priv->qmi_device,
                     (QMI_DEVICE_OPEN_FLAGS_VERSION_INFO |
                      QMI_DEVICE_OPEN_FLAGS_NET_802_3 |
                      QMI_DEVICE_OPEN_FLAGS_NET_NO_QOS_HEADER),
                     10,
                     NULL, /* cancellable */
                     (GAsyncReadyCallback) device_open_ready,
                     ctx);
}


static void
initable_init_async (GAsyncInitable *initable,
                     int io_priority,
                     GCancellable *cancellable,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
    InitContext *ctx;

    ctx = g_slice_new (InitContext);
    ctx->self = g_object_ref (initable);
    ctx->result = g_simple_async_result_new (G_OBJECT (initable),
                                             callback,
                                             user_data,
                                             initable_init_async);
    /* Launch device creation */
    qmi_device_new (ctx->self->priv->file,
                    NULL, /* cancellable */
                    (GAsyncReadyCallback) device_new_ready,
                    ctx);
}

/*****************************************************************************/

RmfdProcessor *
rmfd_processor_new_finish (GAsyncResult  *res,
                           GError       **error)
{
    GObject *source;
    GObject *self;

    source = g_async_result_get_source_object (res);
    self = g_async_initable_new_finish (G_ASYNC_INITABLE (source), res, error);
    g_object_unref (source);

    if (!self)
        return NULL;

    return RMFD_PROCESSOR (self);
}

void
rmfd_processor_new (GFile               *file,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
    g_async_initable_new_async (RMFD_TYPE_PROCESSOR,
                                G_PRIORITY_DEFAULT,
                                NULL,
                                callback,
                                user_data,
                                RMFD_PROCESSOR_FILE, file,
                                NULL);
}

/*****************************************************************************/

static void
rmfd_processor_init (RmfdProcessor *self)
{
    /* Setup private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                              RMFD_TYPE_PROCESSOR,
                                              RmfdProcessorPrivate);
}

static void
set_property (GObject *object,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    RmfdProcessorPrivate *priv = RMFD_PROCESSOR (object)->priv;

    switch (prop_id) {
    case PROP_FILE:
        if (priv->file)
            g_object_unref (priv->file);
        priv->file = g_value_dup_object (value);
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
    RmfdProcessorPrivate *priv = RMFD_PROCESSOR (object)->priv;

    switch (prop_id) {
    case PROP_FILE:
        g_value_set_object (value, priv->file);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
dispose (GObject *object)
{
    RmfdProcessorPrivate *priv = RMFD_PROCESSOR (object)->priv;

    if (priv->qmi_device && qmi_device_is_open (priv->qmi_device)) {
        GError *error = NULL;

        if (!qmi_device_close (priv->qmi_device, &error)) {
            g_warning ("error closing QMI device: %s", error->message);
            g_error_free (error);
        } else
            g_debug ("QmiDevice closed: %s", qmi_device_get_path (priv->qmi_device));
    }

    g_clear_object (&priv->qmi_device);
    g_clear_object (&priv->file);

    G_OBJECT_CLASS (rmfd_processor_parent_class)->dispose (object);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
    iface->init_async = initable_init_async;
    iface->init_finish = initable_init_finish;
}

static void
rmfd_processor_class_init (RmfdProcessorClass *processor_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (processor_class);

    g_type_class_add_private (object_class, sizeof (RmfdProcessorPrivate));

    /* Virtual methods */
    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->dispose = dispose;

    /* Properties */
    g_object_class_install_property
        (object_class, PROP_FILE,
         g_param_spec_object (RMFD_PROCESSOR_FILE,
                              "File",
                              "File to control the device",
                              G_TYPE_FILE,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

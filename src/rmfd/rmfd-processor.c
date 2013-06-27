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
    /* QMI clients */
    QmiClient *dms;
    QmiClient *nas;
    QmiClient *wds;
};

/*****************************************************************************/
/* Process an action */

typedef struct {
    RmfdProcessor *self;
    GSimpleAsyncResult *result;
    GByteArray *request;
} RunContext;

static void
run_context_complete_and_free (RunContext *ctx)
{
    g_simple_async_result_complete_in_idle (ctx->result);
    g_object_unref (ctx->result);
    g_byte_array_unref (ctx->request);
    g_object_unref (ctx->self);
    g_slice_free (RunContext, ctx);
}

GByteArray *
rmfd_processor_run_finish (RmfdProcessor *self,
                           GAsyncResult  *res,
                           GError       **error)
{
    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return NULL;

    return g_byte_array_ref (g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res)));
}

/**********************/
/* Get manufacturer */

static void
dms_get_manufacturer_ready (QmiClientDms *client,
                            GAsyncResult *res,
                            RunContext   *ctx)
{
    QmiMessageDmsGetManufacturerOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_dms_get_manufacturer_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_dms_get_manufacturer_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't get manufacturer: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else {
        const gchar *str;
        guint8 *response;

        qmi_message_dms_get_manufacturer_output_get_manufacturer (output, &str, NULL);

        response = rmf_message_get_manufacturer_response_new (str);
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    if (output)
        qmi_message_dms_get_manufacturer_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
get_manufacturer (RunContext *ctx)
{
    qmi_client_dms_get_manufacturer (QMI_CLIENT_DMS (ctx->self->priv->dms),
                                     NULL,
                                     5,
                                     NULL,
                                     (GAsyncReadyCallback) dms_get_manufacturer_ready,
                                     ctx);
}

/**********************/
/* Get model */

static void
dms_get_model_ready (QmiClientDms *client,
                     GAsyncResult *res,
                     RunContext   *ctx)
{
    QmiMessageDmsGetModelOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_dms_get_model_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_dms_get_model_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't get model: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else {
        const gchar *str;
        guint8 *response;

        qmi_message_dms_get_model_output_get_model (output, &str, NULL);

        response = rmf_message_get_model_response_new (str);
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    if (output)
        qmi_message_dms_get_model_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
get_model (RunContext *ctx)
{
    qmi_client_dms_get_model (QMI_CLIENT_DMS (ctx->self->priv->dms),
                              NULL,
                              5,
                              NULL,
                              (GAsyncReadyCallback) dms_get_model_ready,
                              ctx);
}

/**********************/
/* Get revision */

static void
dms_get_revision_ready (QmiClientDms *client,
                        GAsyncResult *res,
                        RunContext   *ctx)
{
    QmiMessageDmsGetRevisionOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_dms_get_revision_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_dms_get_revision_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't get revision: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else {
        const gchar *str;
        guint8 *response;

        qmi_message_dms_get_revision_output_get_revision (output, &str, NULL);

        response = rmf_message_get_software_revision_response_new (str);
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    if (output)
        qmi_message_dms_get_revision_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
get_revision (RunContext *ctx)
{
    qmi_client_dms_get_revision (QMI_CLIENT_DMS (ctx->self->priv->dms),
                                 NULL,
                                 5,
                                 NULL,
                                 (GAsyncReadyCallback) dms_get_revision_ready,
                                 ctx);
}

/**********************/
/* Get hardware revision */

static void
dms_get_hardware_revision_ready (QmiClientDms *client,
                                 GAsyncResult *res,
                                 RunContext   *ctx)
{
    QmiMessageDmsGetHardwareRevisionOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_dms_get_hardware_revision_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_dms_get_hardware_revision_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't get revision: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else {
        const gchar *str;
        guint8 *response;

        qmi_message_dms_get_hardware_revision_output_get_revision (output, &str, NULL);

        response = rmf_message_get_hardware_revision_response_new (str);
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    if (output)
        qmi_message_dms_get_hardware_revision_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
get_hardware_revision (RunContext *ctx)
{
    qmi_client_dms_get_hardware_revision (QMI_CLIENT_DMS (ctx->self->priv->dms),
                                          NULL,
                                          5,
                                          NULL,
                                          (GAsyncReadyCallback) dms_get_hardware_revision_ready,
                                          ctx);
}

/**********************/
/* Get IMEI */

static void
dms_get_ids_ready (QmiClientDms *client,
                   GAsyncResult *res,
                   RunContext   *ctx)
{
    QmiMessageDmsGetIdsOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_dms_get_ids_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_dms_get_ids_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't get IMEI: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else {
        const gchar *str;
        guint8 *response;

        qmi_message_dms_get_ids_output_get_imei (output, &str, NULL);

        response = rmf_message_get_imei_response_new (str);
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    if (output)
        qmi_message_dms_get_ids_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
get_imei (RunContext *ctx)
{
    qmi_client_dms_get_ids (QMI_CLIENT_DMS (ctx->self->priv->dms),
                            NULL,
                            5,
                            NULL,
                            (GAsyncReadyCallback) dms_get_ids_ready,
                            ctx);
}

/**********************/
/* Get IMSI */

static void
dms_uim_get_imsi_ready (QmiClientDms *client,
                        GAsyncResult *res,
                        RunContext   *ctx)
{
    QmiMessageDmsUimGetImsiOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_dms_uim_get_imsi_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_dms_uim_get_imsi_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't get IMSI: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else {
        const gchar *str;
        guint8 *response;

        qmi_message_dms_uim_get_imsi_output_get_imsi (output, &str, NULL);

        response = rmf_message_get_imsi_response_new (str);
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    if (output)
        qmi_message_dms_uim_get_imsi_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
get_imsi (RunContext *ctx)
{
    qmi_client_dms_uim_get_imsi (QMI_CLIENT_DMS (ctx->self->priv->dms),
                                 NULL,
                                 5,
                                 NULL,
                                 (GAsyncReadyCallback) dms_uim_get_imsi_ready,
                                 ctx);
}

/**********************/
/* Get ICCID */

static void
dms_uim_get_iccid_ready (QmiClientDms *client,
                         GAsyncResult *res,
                         RunContext   *ctx)
{
    QmiMessageDmsUimGetIccidOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_dms_uim_get_iccid_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_dms_uim_get_iccid_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't get ICCID: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else {
        const gchar *str;
        guint8 *response;

        qmi_message_dms_uim_get_iccid_output_get_iccid (output, &str, NULL);

        response = rmf_message_get_iccid_response_new (str);
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    if (output)
        qmi_message_dms_uim_get_iccid_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
get_iccid (RunContext *ctx)
{
    qmi_client_dms_uim_get_iccid (QMI_CLIENT_DMS (ctx->self->priv->dms),
                                  NULL,
                                  5,
                                  NULL,
                                  (GAsyncReadyCallback) dms_uim_get_iccid_ready,
                                  ctx);
}

/**********************/
/* Unlock PIN */

static void
dms_uim_verify_pin_ready (QmiClientDms *client,
                          GAsyncResult *res,
                          RunContext   *ctx)
{
    QmiMessageDmsUimVerifyPinOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_dms_uim_verify_pin_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_dms_uim_verify_pin_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't verify PIN: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else {
        guint8 *response;

        response = rmf_message_unlock_response_new ();
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    if (output)
        qmi_message_dms_uim_verify_pin_output_unref (output);
    run_context_complete_and_free (ctx);
}

static void
dms_uim_get_pin_status_ready (QmiClientDms *client,
                              GAsyncResult *res,
                              RunContext   *ctx)
{
    QmiMessageDmsUimGetPinStatusOutput *output = NULL;
    GError *error = NULL;
    QmiDmsUimPinStatus current_status;

    output = qmi_client_dms_uim_get_pin_status_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_dms_uim_get_pin_status_output_get_result (output, &error)) {
        /* QMI error internal when checking PIN status likely means NO SIM */
        if (g_error_matches (error, QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INTERNAL)) {
            g_error_free (error);
            error = g_error_new (QMI_PROTOCOL_ERROR,
                                 QMI_PROTOCOL_ERROR_NO_SIM,
                                 "missing SIM");
        }
        g_prefix_error (&error, "couldn't get PIN status: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_dms_uim_get_pin_status_output_get_pin1_status (
                   output,
                   &current_status,
                   NULL, /* verify_retries_left */
                   NULL, /* unblock_retries_left */
                   &error)) {
        g_prefix_error (&error, "couldn't get PIN1 status: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else {
        switch (current_status) {
        case QMI_DMS_UIM_PIN_STATUS_CHANGED:
            /* This state is possibly given when after an ChangePin() operation has been performed. */
        case QMI_DMS_UIM_PIN_STATUS_UNBLOCKED:
            /* This state is possibly given when after an Unblock() operation has been performed. */
        case QMI_DMS_UIM_PIN_STATUS_DISABLED:
        case QMI_DMS_UIM_PIN_STATUS_ENABLED_VERIFIED: {
            guint8 *response;

            response = rmf_message_unlock_response_new ();
            g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                       g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                       (GDestroyNotify)g_byte_array_unref);
            break;
        }

        case QMI_DMS_UIM_PIN_STATUS_BLOCKED:
            g_simple_async_result_set_error (ctx->result,
                                             QMI_PROTOCOL_ERROR,
                                             QMI_PROTOCOL_ERROR_PIN_BLOCKED,
                                             "PIN blocked (PUK required)");
            break;

        case QMI_DMS_UIM_PIN_STATUS_PERMANENTLY_BLOCKED:
            g_simple_async_result_set_error (ctx->result,
                                             QMI_PROTOCOL_ERROR,
                                             QMI_PROTOCOL_ERROR_PIN_ALWAYS_BLOCKED,
                                             "PIN/PUK always blocked");
            break;

        case QMI_DMS_UIM_PIN_STATUS_NOT_INITIALIZED:
        case QMI_DMS_UIM_PIN_STATUS_ENABLED_NOT_VERIFIED: {
            QmiMessageDmsUimVerifyPinInput *input;
            const gchar *pin;

            /* Send PIN */
            rmf_message_unlock_request_parse (ctx->request->data, &pin);
            input = qmi_message_dms_uim_verify_pin_input_new ();
            qmi_message_dms_uim_verify_pin_input_set_info (
                input,
                QMI_DMS_UIM_PIN_ID_PIN,
                pin,
                NULL);
            qmi_client_dms_uim_verify_pin (QMI_CLIENT_DMS (ctx->self->priv->dms),
                                           input,
                                           5,
                                           NULL,
                                           (GAsyncReadyCallback)dms_uim_verify_pin_ready,
                                           ctx);
            qmi_message_dms_uim_verify_pin_input_unref (input);
            return;
        }

        default:
            g_simple_async_result_set_error (ctx->result,
                                             RMFD_ERROR,
                                             RMFD_ERROR_UNKNOWN,
                                             "Unknown lock status");
            break;
        }
    }

    if (output)
        qmi_message_dms_uim_get_pin_status_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
unlock (RunContext *ctx)
{
    /* First, check current lock status */
    qmi_client_dms_uim_get_pin_status (QMI_CLIENT_DMS (ctx->self->priv->dms),
                                       NULL,
                                       5,
                                       NULL,
                                       (GAsyncReadyCallback)dms_uim_get_pin_status_ready,
                                       ctx);
}

/**********************/

void
rmfd_processor_run (RmfdProcessor       *self,
                    GByteArray          *request,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
    RunContext *ctx;

    ctx = g_slice_new (RunContext);
    ctx->self = g_object_ref (self);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             rmfd_processor_run);
    ctx->request = g_byte_array_ref (request);

    if (rmf_message_get_type (request->data) != RMF_MESSAGE_TYPE_REQUEST) {
        g_simple_async_result_set_error (ctx->result,
                                         RMFD_ERROR,
                                         RMFD_ERROR_INVALID_REQUEST,
                                         "received message is not a request");
        run_context_complete_and_free (ctx);
        return;
    }

    switch (rmf_message_get_command (request->data)) {
    case RMF_MESSAGE_COMMAND_GET_MANUFACTURER:
        get_manufacturer (ctx);
        return;
    case RMF_MESSAGE_COMMAND_GET_MODEL:
        get_model (ctx);
        return;
    case RMF_MESSAGE_COMMAND_GET_SOFTWARE_REVISION:
        get_revision (ctx);
        return;
    case RMF_MESSAGE_COMMAND_GET_HARDWARE_REVISION:
        get_hardware_revision (ctx);
        return;
    case RMF_MESSAGE_COMMAND_GET_IMEI:
        get_imei (ctx);
        return;
    case RMF_MESSAGE_COMMAND_GET_IMSI:
        get_imsi (ctx);
        return;
    case RMF_MESSAGE_COMMAND_GET_ICCID:
        get_iccid (ctx);
        return;
    case RMF_MESSAGE_COMMAND_UNLOCK:
        unlock (ctx);
        return;
    default:
        break;
    }

    g_simple_async_result_set_error (ctx->result,
                                     RMFD_ERROR,
                                     RMFD_ERROR_UNKNOWN_COMMAND,
                                     "unknown command received (0x%X)",
                                     rmf_message_get_command (request->data));
    run_context_complete_and_free (ctx);
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
allocate_wds_client_ready (QmiDevice    *qmi_device,
                           GAsyncResult *res,
                           InitContext  *ctx)
{
    GError *error = NULL;

    ctx->self->priv->wds = qmi_device_allocate_client_finish (qmi_device, res, &error);
    if (!ctx->self->priv->wds) {
        g_simple_async_result_take_error (ctx->result, error);
        init_context_complete_and_free (ctx);
        return;
    }

    g_debug ("QMI WDS client created");
    g_simple_async_result_set_op_res_gboolean (ctx->result, TRUE);
    init_context_complete_and_free (ctx);
}

static void
allocate_nas_client_ready (QmiDevice    *qmi_device,
                           GAsyncResult *res,
                           InitContext  *ctx)
{
    GError *error = NULL;

    ctx->self->priv->nas = qmi_device_allocate_client_finish (qmi_device, res, &error);
    if (!ctx->self->priv->nas) {
        g_simple_async_result_take_error (ctx->result, error);
        init_context_complete_and_free (ctx);
        return;
    }

    g_debug ("QMI NAS client created");
    qmi_device_allocate_client (ctx->self->priv->qmi_device,
                                QMI_SERVICE_WDS,
                                QMI_CID_NONE,
                                10,
                                NULL,
                                (GAsyncReadyCallback)allocate_wds_client_ready,
                                ctx);
}

static void
allocate_dms_client_ready (QmiDevice    *qmi_device,
                           GAsyncResult *res,
                           InitContext  *ctx)
{
    GError *error = NULL;

    ctx->self->priv->dms = qmi_device_allocate_client_finish (qmi_device, res, &error);
    if (!ctx->self->priv->dms) {
        g_simple_async_result_take_error (ctx->result, error);
        init_context_complete_and_free (ctx);
        return;
    }

    g_debug ("QMI DMS client created");
    qmi_device_allocate_client (ctx->self->priv->qmi_device,
                                QMI_SERVICE_NAS,
                                QMI_CID_NONE,
                                10,
                                NULL,
                                (GAsyncReadyCallback)allocate_nas_client_ready,
                                ctx);
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

    g_debug ("QMI device opened: %s", qmi_device_get_path (ctx->self->priv->qmi_device));
    qmi_device_allocate_client (ctx->self->priv->qmi_device,
                                QMI_SERVICE_DMS,
                                QMI_CID_NONE,
                                10,
                                NULL,
                                (GAsyncReadyCallback)allocate_dms_client_ready,
                                ctx);
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

    g_debug ("QMI device created: %s", qmi_device_get_path (ctx->self->priv->qmi_device));

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

    if (priv->qmi_device  && qmi_device_is_open (priv->qmi_device)) {
        GError *error = NULL;

        if (priv->dms)
            qmi_device_release_client (priv->qmi_device,
                                       priv->dms,
                                       QMI_DEVICE_RELEASE_CLIENT_FLAGS_RELEASE_CID,
                                       3, NULL, NULL, NULL);
        if (priv->nas)
            qmi_device_release_client (priv->qmi_device,
                                       priv->nas,
                                       QMI_DEVICE_RELEASE_CLIENT_FLAGS_RELEASE_CID,
                                       3, NULL, NULL, NULL);
        if (priv->wds)
            qmi_device_release_client (priv->qmi_device,
                                       priv->wds,
                                       QMI_DEVICE_RELEASE_CLIENT_FLAGS_RELEASE_CID,
                                       3, NULL, NULL, NULL);

        if (!qmi_device_close (priv->qmi_device, &error)) {
            g_warning ("error closing QMI device: %s", error->message);
            g_error_free (error);
        } else
            g_debug ("QmiDevice closed: %s", qmi_device_get_path (priv->qmi_device));
    }

    g_clear_object (&priv->dms);
    g_clear_object (&priv->nas);
    g_clear_object (&priv->wds);
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

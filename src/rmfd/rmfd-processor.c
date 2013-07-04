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

#include <rmf-messages.h>

#include "rmfd-processor.h"
#include "rmfd-error.h"
#include "rmfd-error-types.h"

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (RmfdProcessor, rmfd_processor, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               async_initable_iface_init))

#define GLOBAL_PACKET_DATA_HANDLE 0xFFFFFFFF

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
    /* Connection related info */
    RmfConnectionStatus connection_status;
    guint32 packet_data_handle;
};

/*****************************************************************************/
/* Process an action */

typedef struct {
    RmfdProcessor *self;
    GSimpleAsyncResult *result;
    GByteArray *request;
    RmfdWwan *wwan;
    gpointer additional_context;
    GDestroyNotify additional_context_free;
} RunContext;

static void
run_context_complete_and_free (RunContext *ctx)
{
    g_simple_async_result_complete_in_idle (ctx->result);
    g_object_unref (ctx->result);
    if (ctx->additional_context && ctx->additional_context_free)
        ctx->additional_context_free (ctx->additional_context);
    g_byte_array_unref (ctx->request);
    g_object_unref (ctx->wwan);
    g_object_unref (ctx->self);
    g_slice_free (RunContext, ctx);
}

static void
run_context_set_additional_context (RunContext     *ctx,
                                    gpointer        additional_context,
                                    GDestroyNotify  additional_context_free)
{
    g_assert (ctx != NULL);
    ctx->additional_context = additional_context;
    ctx->additional_context_free = additional_context_free;
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

typedef struct {
    guint after_unlock_checks;
} UnlockPinContext;

static void run_after_unlock_checks (RunContext *ctx);

static void
dms_uim_get_pin_status_after_unlock_ready (QmiClientDms *client,
                                           GAsyncResult *res,
                                           RunContext   *ctx)
{
    QmiMessageDmsUimGetPinStatusOutput *output = NULL;
    GError *error = NULL;
    QmiDmsUimPinStatus current_status;

    output = qmi_client_dms_uim_get_pin_status_finish (client, res, NULL);
    if (output &&
        qmi_message_dms_uim_get_pin_status_output_get_result (output, NULL) &&
        qmi_message_dms_uim_get_pin_status_output_get_pin1_status (
            output,
            &current_status,
            NULL, /* verify_retries_left */
            NULL, /* unblock_retries_left */
            NULL)) {
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
            qmi_message_dms_uim_get_pin_status_output_unref (output);
            run_context_complete_and_free (ctx);
            return;
        }

        case QMI_DMS_UIM_PIN_STATUS_BLOCKED:
        case QMI_DMS_UIM_PIN_STATUS_PERMANENTLY_BLOCKED:
        case QMI_DMS_UIM_PIN_STATUS_NOT_INITIALIZED:
        case QMI_DMS_UIM_PIN_STATUS_ENABLED_NOT_VERIFIED:
        default:
            break;
        }
    }

    if (output)
        qmi_message_dms_uim_get_pin_status_output_unref (output);

    /* Sleep & retry */
    run_after_unlock_checks (ctx);
}

static gboolean
after_unlock_check_cb (RunContext *ctx)
{
    qmi_client_dms_uim_get_pin_status (QMI_CLIENT_DMS (ctx->self->priv->dms),
                                       NULL,
                                       5,
                                       NULL,
                                       (GAsyncReadyCallback)dms_uim_get_pin_status_after_unlock_ready,
                                       ctx);
    return FALSE;
}

static void
run_after_unlock_checks (RunContext *ctx)
{
    UnlockPinContext *unlock_ctx = (UnlockPinContext *)ctx->additional_context;

    if (unlock_ctx->after_unlock_checks == 10) {
        g_simple_async_result_set_error (ctx->result,
                                         RMFD_ERROR,
                                         RMFD_ERROR_UNKNOWN,
                                         "PIN unlocked but too many unlock checks afterwards");
        run_context_complete_and_free (ctx);
        return;
    }

    /* Recheck lock status. The change is not immediate */
    unlock_ctx->after_unlock_checks++;
    g_timeout_add_seconds (1, (GSourceFunc)after_unlock_check_cb, ctx);
}

static void
dms_uim_verify_pin_ready (QmiClientDms *client,
                          GAsyncResult *res,
                          RunContext   *ctx)
{
    QmiMessageDmsUimVerifyPinOutput *output = NULL;
    GError *error = NULL;
    UnlockPinContext *unlock_ctx;

    output = qmi_client_dms_uim_verify_pin_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
        run_context_complete_and_free (ctx);
        return;
    }

    if (!qmi_message_dms_uim_verify_pin_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't verify PIN: ");
        g_simple_async_result_take_error (ctx->result, error);
        qmi_message_dms_uim_verify_pin_output_unref (output);
        run_context_complete_and_free (ctx);
        return;
    }

    qmi_message_dms_uim_verify_pin_output_unref (output);
    unlock_ctx = g_new0 (UnlockPinContext, 1);
    run_context_set_additional_context (ctx, unlock_ctx, g_free);
    run_after_unlock_checks (ctx);
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
            qmi_message_dms_uim_get_pin_status_output_unref (output);
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
/* Enable PIN */

static void
dms_uim_set_pin_protection_ready (QmiClientDms *client,
                                  GAsyncResult *res,
                                  RunContext *ctx)
{
    QmiMessageDmsUimSetPinProtectionOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_dms_uim_set_pin_protection_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_dms_uim_set_pin_protection_output_get_result (output, &error)) {
        /* QMI error internal when checking PIN status likely means NO SIM */
        if (g_error_matches (error, QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INTERNAL)) {
            g_error_free (error);
            error = g_error_new (QMI_PROTOCOL_ERROR,
                                 QMI_PROTOCOL_ERROR_NO_SIM,
                                 "missing SIM");
        }
        /* 'No effect' error means we're already either enabled or disabled */
        else if (g_error_matches (error, QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NO_EFFECT)) {
            g_clear_error (&error);
        }

        if (error) {
            g_prefix_error (&error, "couldn't enable/disable PIN: ");
            g_simple_async_result_take_error (ctx->result, error);
        }
    }

    if (!error) {
        guint8 *response;

        response = rmf_message_enable_pin_response_new ();
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    if (output)
        qmi_message_dms_uim_set_pin_protection_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
enable_pin (RunContext *ctx)
{
    QmiMessageDmsUimSetPinProtectionInput *input;
    guint32 enable;
    const gchar *pin;

    rmf_message_enable_pin_request_parse (ctx->request->data, &enable, &pin);

    input = qmi_message_dms_uim_set_pin_protection_input_new ();
    qmi_message_dms_uim_set_pin_protection_input_set_info (
        input,
        QMI_DMS_UIM_PIN_ID_PIN,
        !!enable,
        pin,
        NULL);
    qmi_client_dms_uim_set_pin_protection (QMI_CLIENT_DMS (ctx->self->priv->dms),
                                           input,
                                           5,
                                           NULL,
                                           (GAsyncReadyCallback)dms_uim_set_pin_protection_ready,
                                           ctx);
    qmi_message_dms_uim_set_pin_protection_input_unref (input);
}

/**********************/
/* Change PIN */

static void
dms_uim_change_pin_ready (QmiClientDms *client,
                          GAsyncResult *res,
                          RunContext *ctx)
{
    QmiMessageDmsUimChangePinOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_dms_uim_change_pin_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_dms_uim_change_pin_output_get_result (output, &error)) {
        /* QMI error internal when checking PIN status likely means NO SIM */
        if (g_error_matches (error, QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INTERNAL)) {
            g_error_free (error);
            error = g_error_new (QMI_PROTOCOL_ERROR,
                                 QMI_PROTOCOL_ERROR_NO_SIM,
                                 "missing SIM");
        }
        /* 'No effect' error means we already have that PIN */
        else if (g_error_matches (error, QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NO_EFFECT)) {
            g_clear_error (&error);
        }

        if (error) {
            g_prefix_error (&error, "couldn't change PIN: ");
            g_simple_async_result_take_error (ctx->result, error);
        }
    }

    if (!error) {
        guint8 *response;

        response = rmf_message_change_pin_response_new ();
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    if (output)
        qmi_message_dms_uim_change_pin_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
change_pin (RunContext *ctx)
{
    QmiMessageDmsUimChangePinInput *input;
    const gchar *old_pin;
    const gchar *new_pin;

    rmf_message_change_pin_request_parse (ctx->request->data, &old_pin, &new_pin);

    input = qmi_message_dms_uim_change_pin_input_new ();
    qmi_message_dms_uim_change_pin_input_set_info (
        input,
        QMI_DMS_UIM_PIN_ID_PIN,
        old_pin,
        new_pin,
        NULL);
    qmi_client_dms_uim_change_pin (QMI_CLIENT_DMS (ctx->self->priv->dms),
                                   input,
                                   5,
                                   NULL,
                                   (GAsyncReadyCallback)dms_uim_change_pin_ready,
                                   ctx);
    qmi_message_dms_uim_change_pin_input_unref (input);
}

/**********************/
/* Get power status */

static void
dms_get_operating_mode_ready (QmiClientDms *client,
                              GAsyncResult *res,
                              RunContext   *ctx)
{
    QmiMessageDmsGetOperatingModeOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_dms_get_operating_mode_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_dms_get_operating_mode_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't get operating mode: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else {
        QmiDmsOperatingMode mode = QMI_DMS_OPERATING_MODE_UNKNOWN;
        guint8 *response = NULL;

        qmi_message_dms_get_operating_mode_output_get_mode (output, &mode, NULL);

        switch (mode) {
        case QMI_DMS_OPERATING_MODE_ONLINE:
            response = rmf_message_get_power_status_response_new (RMF_POWER_STATUS_FULL);
            break;
        case QMI_DMS_OPERATING_MODE_LOW_POWER:
        case QMI_DMS_OPERATING_MODE_PERSISTENT_LOW_POWER:
        case QMI_DMS_OPERATING_MODE_MODE_ONLY_LOW_POWER:
        case QMI_DMS_OPERATING_MODE_OFFLINE:
            response = rmf_message_get_power_status_response_new (RMF_POWER_STATUS_LOW);
            break;
        default:
            g_simple_async_result_set_error (ctx->result,
                                             RMFD_ERROR,
                                             RMFD_ERROR_UNKNOWN,
                                             "Unhandled power state: '%s' (%u)",
                                             qmi_dms_operating_mode_get_string (mode),
                                             mode);
            break;
        }

        if (response)
            g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                       g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                       (GDestroyNotify)g_byte_array_unref);
    }

    if (output)
        qmi_message_dms_get_operating_mode_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
get_power_status (RunContext *ctx)
{
    qmi_client_dms_get_operating_mode (QMI_CLIENT_DMS (ctx->self->priv->dms),
                                       NULL,
                                       5,
                                       NULL,
                                       (GAsyncReadyCallback)dms_get_operating_mode_ready,
                                       ctx);
}

/**********************/
/* Set power status */

static void
dms_set_operating_mode_ready (QmiClientDms *client,
                              GAsyncResult *res,
                              RunContext   *ctx)
{
    QmiMessageDmsSetOperatingModeOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_dms_set_operating_mode_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_dms_set_operating_mode_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't set operating mode: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else {
        guint8 *response = NULL;

        response = rmf_message_set_power_status_response_new ();
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    if (output)
        qmi_message_dms_set_operating_mode_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
set_power_status (RunContext *ctx)
{
    QmiMessageDmsSetOperatingModeInput *input;
    uint32_t power_status;
    QmiDmsOperatingMode mode;

    rmf_message_set_power_status_request_parse (ctx->request->data, &power_status);

    switch (power_status) {
    case RMF_POWER_STATUS_FULL:
        mode = QMI_DMS_OPERATING_MODE_ONLINE;
        break;
    case RMF_POWER_STATUS_LOW:
        mode = QMI_DMS_OPERATING_MODE_LOW_POWER;
        break;
    default:
        g_simple_async_result_set_error (ctx->result,
                                         RMFD_ERROR,
                                         RMFD_ERROR_UNKNOWN,
                                         "Unhandled power state: '%u'",
                                         power_status);
        run_context_complete_and_free (ctx);
        return;
    }

    input = qmi_message_dms_set_operating_mode_input_new ();
    qmi_message_dms_set_operating_mode_input_set_mode (input, mode, NULL);
    qmi_client_dms_set_operating_mode (QMI_CLIENT_DMS (ctx->self->priv->dms),
                                       input,
                                       20,
                                       NULL,
                                       (GAsyncReadyCallback)dms_set_operating_mode_ready,
                                       ctx);
    qmi_message_dms_set_operating_mode_input_unref (input);
}

/**********************/
/* Get power info */

typedef struct {
    gboolean gsm_run;
    guint32 gsm_in_traffic;
    gint32 gsm_tx_power;
    guint32 gsm_rx0_radio_tuned;
    gint32 gsm_rx0_power;
    guint32 gsm_rx1_radio_tuned;
    gint32 gsm_rx1_power;
    gboolean umts_run;
    guint32 umts_in_traffic;
    gint32 umts_tx_power;
    guint32 umts_rx0_radio_tuned;
    gint32 umts_rx0_power;
    guint32 umts_rx1_radio_tuned;
    gint32 umts_rx1_power;
    gboolean lte_run;
    guint32 lte_in_traffic;
    gint32 lte_tx_power;
    guint32 lte_rx0_radio_tuned;
    gint32 lte_rx0_power;
    guint32 lte_rx1_radio_tuned;
    gint32 lte_rx1_power;
} GetPowerInfoContext;

static void
get_power_info_context_free (GetPowerInfoContext *ctx)
{
    g_slice_free (GetPowerInfoContext, ctx);
}

static void get_next_power_info (RunContext *ctx);

static void
nas_get_tx_rx_info_ready (QmiClientNas *client,
                          GAsyncResult *res,
                          RunContext   *ctx)
{
    GetPowerInfoContext *additional_context;
    QmiMessageNasGetTxRxInfoOutput *output;
    GError *error = NULL;
    gboolean is_radio_tuned;
    gboolean is_in_traffic;
    gint32 power;

    additional_context = (GetPowerInfoContext *)ctx->additional_context;

    output = qmi_client_nas_get_tx_rx_info_finish (client, res, &error);
    if (output && qmi_message_nas_get_tx_rx_info_output_get_result (output, NULL)) {
        /* RX Channel 0 */
        if (qmi_message_nas_get_tx_rx_info_output_get_rx_chain_0_info (
                output,
                &is_radio_tuned,
                &power,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL)) {
            if (!additional_context->gsm_run) {
                additional_context->gsm_rx0_radio_tuned = is_radio_tuned;
                if (additional_context->gsm_rx0_radio_tuned)
                    additional_context->gsm_rx0_power = power;
            } else if (!additional_context->umts_run) {
                additional_context->umts_rx0_radio_tuned = is_radio_tuned;
                if (additional_context->umts_rx0_radio_tuned)
                    additional_context->umts_rx0_power = power;
            } else if (!additional_context->lte_run) {
                additional_context->lte_rx0_radio_tuned = is_radio_tuned;
                if (additional_context->lte_rx0_radio_tuned)
                    additional_context->lte_rx0_power = power;
            } else
                g_assert_not_reached ();
        }

        /* RX Channel 1 */
        if (qmi_message_nas_get_tx_rx_info_output_get_rx_chain_1_info (
                output,
                &is_radio_tuned,
                &power,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL)) {
            if (!additional_context->gsm_run) {
                additional_context->gsm_rx1_radio_tuned = is_radio_tuned;
                if (additional_context->gsm_rx1_radio_tuned)
                    additional_context->gsm_rx1_power = power;
            } else if (!additional_context->umts_run) {
                additional_context->umts_rx1_radio_tuned = is_radio_tuned;
                if (additional_context->umts_rx1_radio_tuned)
                    additional_context->umts_rx1_power = power;
            } else if (!additional_context->lte_run) {
                additional_context->lte_rx1_radio_tuned = is_radio_tuned;
                if (additional_context->lte_rx1_radio_tuned)
                    additional_context->lte_rx1_power = power;
            } else
                g_assert_not_reached ();
        }

        /* TX Channel */
        if (qmi_message_nas_get_tx_rx_info_output_get_tx_info (
                output,
                &is_in_traffic,
                &power,
                NULL)) {
            if (!additional_context->gsm_run) {
                additional_context->gsm_in_traffic = is_in_traffic;
                if (additional_context->gsm_in_traffic)
                    additional_context->gsm_tx_power = power;
            } else if (!additional_context->umts_run) {
                additional_context->umts_in_traffic = is_in_traffic;
                if (additional_context->umts_in_traffic)
                    additional_context->umts_tx_power = power;
            } else if (!additional_context->lte_run) {
                additional_context->lte_in_traffic = is_in_traffic;
                if (additional_context->lte_in_traffic)
                    additional_context->lte_tx_power = power;
            } else
                g_assert_not_reached ();
        }
    }

    if (!additional_context->gsm_run)
        additional_context->gsm_run = TRUE;
    else if (!additional_context->umts_run)
        additional_context->umts_run = TRUE;
    else if (!additional_context->lte_run)
        additional_context->lte_run = TRUE;
    else
        g_assert_not_reached ();

    if (output)
        qmi_message_nas_get_tx_rx_info_output_unref (output);

    get_next_power_info (ctx);
}

static void
get_next_power_info (RunContext *ctx)
{
    GetPowerInfoContext *additional_context;
    QmiNasRadioInterface interface;
    guint8 *response;

    additional_context = (GetPowerInfoContext *)ctx->additional_context;

    if (!additional_context->gsm_run)
        interface = QMI_NAS_RADIO_INTERFACE_GSM;
    else if (!additional_context->umts_run)
        interface = QMI_NAS_RADIO_INTERFACE_UMTS;
    else if (!additional_context->lte_run)
        interface = QMI_NAS_RADIO_INTERFACE_LTE;
    else
        interface = QMI_NAS_RADIO_INTERFACE_UNKNOWN;

    /* Request next */
    if (interface != QMI_NAS_RADIO_INTERFACE_UNKNOWN) {
        QmiMessageNasGetTxRxInfoInput *input;

        input = qmi_message_nas_get_tx_rx_info_input_new ();
        qmi_message_nas_get_tx_rx_info_input_set_radio_interface (input, interface, NULL);
        qmi_client_nas_get_tx_rx_info (QMI_CLIENT_NAS (ctx->self->priv->nas),
                                       input,
                                       10,
                                       NULL,
                                       (GAsyncReadyCallback)nas_get_tx_rx_info_ready,
                                       ctx);
        qmi_message_nas_get_tx_rx_info_input_unref (input);
        return;
    }

    /* All done */
    response = (rmf_message_get_power_info_response_new (
                    additional_context->gsm_in_traffic,
                    additional_context->gsm_tx_power,
                    additional_context->gsm_rx0_radio_tuned,
                    additional_context->gsm_rx0_power,
                    additional_context->gsm_rx1_radio_tuned,
                    additional_context->gsm_rx1_power,
                    additional_context->umts_in_traffic,
                    additional_context->umts_tx_power,
                    additional_context->umts_rx0_radio_tuned,
                    additional_context->umts_rx0_power,
                    additional_context->umts_rx1_radio_tuned,
                    additional_context->umts_rx1_power,
                    additional_context->lte_in_traffic,
                    additional_context->lte_tx_power,
                    additional_context->lte_rx0_radio_tuned,
                    additional_context->lte_rx0_power,
                    additional_context->lte_rx1_radio_tuned,
                    additional_context->lte_rx1_power));
    g_simple_async_result_set_op_res_gpointer (ctx->result,
                                               g_byte_array_new_take (response, rmf_message_get_length (response)),
                                               (GDestroyNotify)g_byte_array_unref);
    run_context_complete_and_free (ctx);
}

static void
get_power_info (RunContext *ctx)
{
    GetPowerInfoContext *additional_context;

    additional_context = g_slice_new0 (GetPowerInfoContext);
    run_context_set_additional_context (ctx,
                                        additional_context,
                                        (GDestroyNotify)get_power_info_context_free);

    get_next_power_info (ctx);
}

/**********************/
/* Get signal info */

/* Limit the value betweeen [-113,-51] and scale it to a percentage */
#define STRENGTH_TO_QUALITY(strength)                                   \
    (guint8)(100 - ((CLAMP (strength, -113, -51) + 51) * 100 / (-113 + 51)))

static void
nas_get_signal_info_ready (QmiClientNas *client,
                           GAsyncResult *res,
                           RunContext   *ctx)
{
    QmiMessageNasGetSignalInfoOutput *output;
    GError *error = NULL;
    guint quality = 0;

    output = qmi_client_nas_get_signal_info_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_nas_get_signal_info_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't get signal info: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else {
        guint8 *response;
        gboolean gsm_available = FALSE;
        gint8 gsm_rssi = -125;
        guint32 gsm_quality = 0;
        guint32 umts_available = FALSE;
        gint8 umts_rssi = -125;
        guint32 umts_quality = 0;
        guint32 lte_available = FALSE;
        gint8 lte_rssi = -125;
        guint32 lte_quality = 0;

        if (qmi_message_nas_get_signal_info_output_get_gsm_signal_strength (output, &gsm_rssi, NULL)) {
            gsm_available = TRUE;
            gsm_quality = STRENGTH_TO_QUALITY (gsm_rssi);
        }
        if (qmi_message_nas_get_signal_info_output_get_wcdma_signal_strength (output, &umts_rssi, NULL, NULL)) {
            umts_available = TRUE;
            umts_quality = STRENGTH_TO_QUALITY (umts_rssi);
        }
        if (qmi_message_nas_get_signal_info_output_get_lte_signal_strength (output, &lte_rssi, NULL, NULL, NULL, NULL)) {
            lte_available = TRUE;
            lte_quality = STRENGTH_TO_QUALITY (lte_rssi);
        }

        response = rmf_message_get_signal_info_response_new (gsm_available, gsm_rssi, gsm_quality,
                                                             umts_available, umts_rssi, umts_quality,
                                                             lte_available, lte_rssi, lte_quality);
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    if (output)
        qmi_message_nas_get_signal_info_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
get_signal_info (RunContext *ctx)
{
    qmi_client_nas_get_signal_info (QMI_CLIENT_NAS (ctx->self->priv->nas),
                                    NULL,
                                    10,
                                    NULL,
                                    (GAsyncReadyCallback)nas_get_signal_info_ready,
                                    ctx);
}

/**********************/
/* Get Registration Status */

static void
nas_get_serving_system_ready (QmiClientNas *client,
                              GAsyncResult *res,
                              RunContext   *ctx)
{
    QmiMessageNasGetServingSystemOutput *output;
    GError *error = NULL;

    output = qmi_client_nas_get_serving_system_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_nas_get_serving_system_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't get serving system: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else {
        guint8 *response;
        guint32 rmf_registration_state;
        QmiNasRegistrationState registration_state = QMI_NAS_REGISTRATION_STATE_UNKNOWN;
        QmiNasRoamingIndicatorStatus roaming = QMI_NAS_ROAMING_INDICATOR_STATUS_OFF;
        guint16 mcc = 0;
        guint16 mnc = 0;
        const gchar *description = NULL;
        guint16 lac = 0;
        guint32 cid = 0;

        qmi_message_nas_get_serving_system_output_get_serving_system (
            output, &registration_state, NULL, NULL, NULL, NULL, NULL);
        qmi_message_nas_get_serving_system_output_get_roaming_indicator (
            output, &roaming, NULL);
        qmi_message_nas_get_serving_system_output_get_current_plmn (
            output, &mcc, &mnc, &description, NULL);
        qmi_message_nas_get_serving_system_output_get_lac_3gpp (
            output, &lac, NULL);
        qmi_message_nas_get_serving_system_output_get_cid_3gpp (
            output, &cid, NULL);

        switch (registration_state) {
        case QMI_NAS_REGISTRATION_STATE_REGISTERED:
            rmf_registration_state = (roaming == QMI_NAS_ROAMING_INDICATOR_STATUS_ON ?
                                      RMF_REGISTRATION_STATUS_ROAMING :
                                      RMF_REGISTRATION_STATUS_HOME);
            break;
        case QMI_NAS_REGISTRATION_STATE_NOT_REGISTERED_SEARCHING:
            rmf_registration_state = RMF_REGISTRATION_STATUS_SEARCHING;
            break;
        case QMI_NAS_REGISTRATION_STATE_NOT_REGISTERED:
        case QMI_NAS_REGISTRATION_STATE_REGISTRATION_DENIED:
        case QMI_NAS_REGISTRATION_STATE_UNKNOWN:
        default:
            rmf_registration_state = RMF_REGISTRATION_STATUS_IDLE;
            break;
        }

        response = (rmf_message_get_registration_status_response_new   (
                        rmf_registration_state, description, mcc, mnc, lac, cid));

        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    if (output)
        qmi_message_nas_get_serving_system_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
get_registration_status (RunContext *ctx)
{
    qmi_client_nas_get_serving_system (QMI_CLIENT_NAS (ctx->self->priv->nas),
                                       NULL,
                                       10,
                                       NULL,
                                       (GAsyncReadyCallback)nas_get_serving_system_ready,
                                       ctx);
}

/**********************/
/* Get connection status */

static void
get_connection_status (RunContext *ctx)
{
    guint8 *response;

    response = rmf_message_get_connection_status_response_new (ctx->self->priv->connection_status);
    g_simple_async_result_set_op_res_gpointer (ctx->result,
                                               g_byte_array_new_take (response, rmf_message_get_length (response)),
                                               (GDestroyNotify)g_byte_array_unref);
    run_context_complete_and_free (ctx);
}

/**********************/
/* Get Connection Stats */

static void
get_packet_statistics_ready (QmiClientWds *client,
                             GAsyncResult *res,
                             RunContext   *ctx)
{
    GError *error = NULL;
    QmiMessageWdsGetPacketStatisticsOutput *output;

    output = qmi_client_wds_get_packet_statistics_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_wds_get_packet_statistics_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't get packet statistics: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else {
        guint8 *response;
        guint32 tx_packets_ok = 0xFFFFFFFF;
        guint32 rx_packets_ok = 0xFFFFFFFF;
        guint32 tx_packets_error = 0xFFFFFFFF;
        guint32 rx_packets_error = 0xFFFFFFFF;
        guint32 tx_packets_overflow = 0xFFFFFFFF;
        guint32 rx_packets_overflow = 0xFFFFFFFF;
        guint64 tx_bytes_ok = 0;
        guint64 rx_bytes_ok = 0;

        qmi_message_wds_get_packet_statistics_output_get_tx_packets_ok (output, &tx_packets_ok, NULL);
        qmi_message_wds_get_packet_statistics_output_get_rx_packets_ok (output, &rx_packets_ok, NULL);
        qmi_message_wds_get_packet_statistics_output_get_tx_packets_error (output, &tx_packets_error, NULL);
        qmi_message_wds_get_packet_statistics_output_get_rx_packets_error (output, &rx_packets_error, NULL);
        qmi_message_wds_get_packet_statistics_output_get_tx_overflows (output, &tx_packets_overflow, NULL);
        qmi_message_wds_get_packet_statistics_output_get_rx_overflows (output, &rx_packets_overflow, NULL);
        qmi_message_wds_get_packet_statistics_output_get_tx_bytes_ok (output, &tx_bytes_ok, NULL);
        qmi_message_wds_get_packet_statistics_output_get_rx_bytes_ok (output, &rx_bytes_ok, NULL);

        response = rmf_message_get_connection_stats_response_new (tx_packets_ok,
                                                                  rx_packets_ok,
                                                                  tx_packets_error,
                                                                  rx_packets_error,
                                                                  tx_packets_overflow,
                                                                  rx_packets_overflow,
                                                                  tx_bytes_ok,
                                                                  rx_bytes_ok);
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    if (output)
        qmi_message_wds_get_packet_statistics_output_unref (output);
    run_context_complete_and_free (ctx);
}

static void
get_connection_stats (RunContext *ctx)
{
    QmiMessageWdsGetPacketStatisticsInput *input;

    input = qmi_message_wds_get_packet_statistics_input_new ();
    qmi_message_wds_get_packet_statistics_input_set_mask (
        input,
        (QMI_WDS_PACKET_STATISTICS_MASK_FLAG_TX_PACKETS_OK      |
         QMI_WDS_PACKET_STATISTICS_MASK_FLAG_RX_PACKETS_OK      |
         QMI_WDS_PACKET_STATISTICS_MASK_FLAG_TX_PACKETS_ERROR   |
         QMI_WDS_PACKET_STATISTICS_MASK_FLAG_RX_PACKETS_ERROR   |
         QMI_WDS_PACKET_STATISTICS_MASK_FLAG_TX_OVERFLOWS       |
         QMI_WDS_PACKET_STATISTICS_MASK_FLAG_RX_OVERFLOWS       |
         QMI_WDS_PACKET_STATISTICS_MASK_FLAG_TX_BYTES_OK        |
         QMI_WDS_PACKET_STATISTICS_MASK_FLAG_RX_BYTES_OK        |
         QMI_WDS_PACKET_STATISTICS_MASK_FLAG_TX_PACKETS_DROPPED |
         QMI_WDS_PACKET_STATISTICS_MASK_FLAG_RX_PACKETS_DROPPED),
        NULL);

    g_debug ("Asynchronously getting packet statistics...");
    qmi_client_wds_get_packet_statistics (QMI_CLIENT_WDS (ctx->self->priv->wds),
                                          input,
                                          10,
                                          NULL,
                                          (GAsyncReadyCallback)get_packet_statistics_ready,
                                          ctx);
    qmi_message_wds_get_packet_statistics_input_unref (input);
}

/**********************/
/* Connect */

static void
wds_stop_network_after_start_ready (QmiClientWds *client,
                                    GAsyncResult *res,
                                    RunContext   *ctx)
{
    QmiMessageWdsStopNetworkOutput *output;

    /* Ignore these errors */
    output = qmi_client_wds_stop_network_finish (client, res, NULL);
    if (output)
        qmi_message_wds_stop_network_output_unref (output);

    /* Clear packet data handle */
    ctx->self->priv->packet_data_handle = 0;
    ctx->self->priv->connection_status = RMF_CONNECTION_STATUS_DISCONNECTED;
    g_simple_async_result_set_op_res_gpointer (ctx->result,
                                               rmfd_error_message_new_from_gerror (
                                                   ctx->request,
                                                   (GError *)ctx->additional_context),
                                               (GDestroyNotify)g_byte_array_unref);
    run_context_complete_and_free (ctx);
}

static void
wwan_setup_start_ready (RmfdWwan     *wwan,
                        GAsyncResult *res,
                        RunContext   *ctx)
{
    GError *error = NULL;
    guint8 *response;

    if (!rmfd_wwan_setup_finish (wwan, res, &error)) {
        QmiMessageWdsStopNetworkInput *input;

        /* Abort */
        run_context_set_additional_context (ctx, error, (GDestroyNotify)g_error_free);
        g_warning ("error: couldn't start interface: %s", error->message);
        input = qmi_message_wds_stop_network_input_new ();
        qmi_message_wds_stop_network_input_set_packet_data_handle (input, ctx->self->priv->packet_data_handle, NULL);
        qmi_client_wds_stop_network (QMI_CLIENT_WDS (ctx->self->priv->wds),
                                     input,
                                     30,
                                     NULL,
                                     (GAsyncReadyCallback)wds_stop_network_after_start_ready,
                                     ctx);
        qmi_message_wds_stop_network_input_unref (input);
        return;
    }

    /* Ok! */
    ctx->self->priv->connection_status = RMF_CONNECTION_STATUS_CONNECTED;
    response = rmf_message_connect_response_new ();
    g_simple_async_result_set_op_res_gpointer (ctx->result,
                                               g_byte_array_new_take (response, rmf_message_get_length (response)),
                                               (GDestroyNotify)g_byte_array_unref);
    run_context_complete_and_free (ctx);
}

static void
wds_start_network_ready (QmiClientWds *client,
                         GAsyncResult *res,
                         RunContext   *ctx)
{
    GError *error = NULL;
    QmiMessageWdsStartNetworkOutput *output;

    output = qmi_client_wds_start_network_finish (client, res, &error);
    if (output &&
        !qmi_message_wds_start_network_output_get_result (output, &error)) {
        /* No-effect errors should be ignored. The modem will keep the
         * connection active as long as there is a WDS client which requested
         * to start the network. If we crashed while a connection was
         * active, we would be leaving an unreleased WDS client around and the
         * modem would just keep connected. */
        if (g_error_matches (error,
                             QMI_PROTOCOL_ERROR,
                             QMI_PROTOCOL_ERROR_NO_EFFECT)) {
            g_error_free (error);
            error = NULL;
            ctx->self->priv->packet_data_handle = GLOBAL_PACKET_DATA_HANDLE;

            /* Fall down to a successful connection */
        } else {
            g_warning ("error: couldn't start network: %s", error->message);
            if (g_error_matches (error,
                                 QMI_PROTOCOL_ERROR,
                                 QMI_PROTOCOL_ERROR_CALL_FAILED)) {
                QmiWdsCallEndReason cer;
                QmiWdsVerboseCallEndReasonType verbose_cer_type;
                gint16 verbose_cer_reason;

                if (qmi_message_wds_start_network_output_get_call_end_reason (
                        output,
                        &cer,
                        NULL))
                    g_warning ("call end reason (%u): '%s'",
                               cer,
                               qmi_wds_call_end_reason_get_string (cer));

                if (qmi_message_wds_start_network_output_get_verbose_call_end_reason (
                        output,
                        &verbose_cer_type,
                        &verbose_cer_reason,
                        NULL))
                    g_warning ("verbose call end reason (%u,%d): [%s] %s",
                               verbose_cer_type,
                               verbose_cer_reason,
                               qmi_wds_verbose_call_end_reason_type_get_string (verbose_cer_type),
                               qmi_wds_verbose_call_end_reason_get_string (verbose_cer_type, verbose_cer_reason));
            }
        }
    }

    if (output) {
        if (ctx->self->priv->packet_data_handle != GLOBAL_PACKET_DATA_HANDLE)
            qmi_message_wds_start_network_output_get_packet_data_handle (output, &ctx->self->priv->packet_data_handle, NULL);
        qmi_message_wds_start_network_output_unref (output);
    }

    if (error) {
        ctx->self->priv->connection_status = RMF_CONNECTION_STATUS_DISCONNECTED;
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   rmfd_error_message_new_from_gerror (ctx->request, error),
                                                   (GDestroyNotify)g_byte_array_unref);
        g_error_free (error);
        run_context_complete_and_free (ctx);
        return;
    }

    rmfd_wwan_setup (ctx->wwan,
                     TRUE,
                     (GAsyncReadyCallback)wwan_setup_start_ready,
                     ctx);
}

static void
wds_set_ip_family_ready (QmiClientWds *client,
                         GAsyncResult *res,
                         RunContext   *ctx)
{
    QmiMessageWdsStartNetworkInput *input;
    QmiMessageWdsSetIpFamilyOutput *output;
    gboolean default_ip_family_set = FALSE;
    const gchar *apn;
    const gchar *user;
    const gchar *password;

    /* If there is an error setting default IP family, explicitly add it when
     * starting network */
    output = qmi_client_wds_set_ip_family_finish (client, res, NULL);
    if (output && qmi_message_wds_set_ip_family_output_get_result (output, NULL))
        default_ip_family_set = TRUE;

    if (output)
        qmi_message_wds_set_ip_family_output_unref (output);

    /* Setup start network command */
    rmf_message_connect_request_parse (ctx->request->data, &apn, &user, &password);

    input = qmi_message_wds_start_network_input_new ();
    if (apn)
        qmi_message_wds_start_network_input_set_apn (input, apn, NULL);

    if (user || password) {
        qmi_message_wds_start_network_input_set_authentication_preference (
            input,
            (QMI_WDS_AUTHENTICATION_PAP | QMI_WDS_AUTHENTICATION_CHAP),
            NULL);
        if (user && user[0])
            qmi_message_wds_start_network_input_set_username (input, user, NULL);
        if (password && password[0])
            qmi_message_wds_start_network_input_set_password (input, password, NULL);
    }

    /* Only add the IP family preference TLV if explicitly requested a given
     * family. This TLV may be newer than the Start Network command itself, so
     * we'll just allow the case where none is specified. Also, don't add this
     * TLV if we already set a default IP family preference with "WDS Set IP
     * Family" */
    if (!default_ip_family_set)
        qmi_message_wds_start_network_input_set_ip_family_preference (input, QMI_WDS_IP_FAMILY_IPV4, NULL);

    qmi_client_wds_start_network (QMI_CLIENT_WDS (ctx->self->priv->wds),
                                  input,
                                  45,
                                  NULL,
                                  (GAsyncReadyCallback)wds_start_network_ready,
                                  ctx);
    qmi_message_wds_start_network_input_unref (input);
}

static void
connect (RunContext *ctx)
{
    QmiMessageWdsSetIpFamilyInput *input;

    if (ctx->self->priv->connection_status != RMF_CONNECTION_STATUS_DISCONNECTED) {
        switch (ctx->self->priv->connection_status) {
        case RMF_CONNECTION_STATUS_DISCONNECTING:
            g_warning ("error connecting: currenty disconnecting");
            g_simple_async_result_set_op_res_gpointer (
                ctx->result,
                rmfd_error_message_new_from_error (ctx->request, RMFD_ERROR, RMFD_ERROR_INVALID_STATE),
                (GDestroyNotify)g_byte_array_unref);
            break;
        case RMF_CONNECTION_STATUS_CONNECTING:
            g_warning ("error connecting: already connecting");
            g_simple_async_result_set_op_res_gpointer (
                ctx->result,
                rmfd_error_message_new_from_error (ctx->request, RMFD_ERROR, RMFD_ERROR_INVALID_STATE),
                (GDestroyNotify)g_byte_array_unref);
            break;
        case RMF_CONNECTION_STATUS_CONNECTED: {
            guint8 *response;

            g_debug ("already connected");
            response = rmf_message_connect_response_new ();
            g_simple_async_result_set_op_res_gpointer (
                ctx->result,
                g_byte_array_new_take (response, rmf_message_get_length (response)),
                (GDestroyNotify)g_byte_array_unref);
            break;
        }
        case RMF_CONNECTION_STATUS_DISCONNECTED:
        default:
            g_assert_not_reached ();
        }

        run_context_complete_and_free (ctx);
        return;
    }

    /* Now connecting */
    ctx->self->priv->connection_status = RMF_CONNECTION_STATUS_CONNECTING;

    /* Start by setting IPv4 family */
    input = qmi_message_wds_set_ip_family_input_new ();
    qmi_message_wds_set_ip_family_input_set_preference (input, QMI_WDS_IP_FAMILY_IPV4, NULL);
    qmi_client_wds_set_ip_family (QMI_CLIENT_WDS (ctx->self->priv->wds),
                                  input,
                                  10,
                                  NULL,
                                  (GAsyncReadyCallback)wds_set_ip_family_ready,
                                  ctx);
    qmi_message_wds_set_ip_family_input_unref (input);
}

/**********************/
/* Disconnect */

static void
wwan_setup_stop_ready (RmfdWwan     *wwan,
                       GAsyncResult *res,
                       RunContext   *ctx)
{
    GError *error = NULL;
    guint8 *response;

    if (!rmfd_wwan_setup_finish (wwan, res, &error)) {
        g_warning ("error: couldn't stop interface: %s", error->message);
        g_warning ("error: will assume disconnected");
        ctx->self->priv->connection_status = RMF_CONNECTION_STATUS_DISCONNECTED;
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   rmfd_error_message_new_from_gerror (ctx->request, error),
                                                   (GDestroyNotify)g_byte_array_unref);
        g_error_free (error);
        run_context_complete_and_free (ctx);
        return;
    }

    /* Ok! */
    ctx->self->priv->connection_status = RMF_CONNECTION_STATUS_DISCONNECTED;
    response = rmf_message_disconnect_response_new ();
    g_simple_async_result_set_op_res_gpointer (ctx->result,
                                               g_byte_array_new_take (response, rmf_message_get_length (response)),
                                               (GDestroyNotify)g_byte_array_unref);
    run_context_complete_and_free (ctx);
}

static void
wds_stop_network_ready (QmiClientWds *client,
                        GAsyncResult *res,
                        RunContext   *ctx)
{
    GError *error = NULL;
    QmiMessageWdsStopNetworkOutput *output;

    output = qmi_client_wds_stop_network_finish (client, res, &error);
    if (output) {
        if (!qmi_message_wds_stop_network_output_get_result (output, &error)) {
            /* No effect error, we're already disconnected */
            if (g_error_matches (error,
                                 QMI_PROTOCOL_ERROR,
                                 QMI_PROTOCOL_ERROR_NO_EFFECT)) {
                g_error_free (error);
                error = NULL;
            }
        }

        qmi_message_wds_stop_network_output_unref (output);
    }

    if (error) {
        g_warning ("error: couldn't disconnect: %s", error->message);
        ctx->self->priv->connection_status = RMF_CONNECTION_STATUS_CONNECTED;
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   rmfd_error_message_new_from_gerror (ctx->request, error),
                                                   (GDestroyNotify)g_byte_array_unref);
        g_error_free (error);
        run_context_complete_and_free (ctx);
        return;
    }

    /* Clear packet data handle */
    ctx->self->priv->packet_data_handle = 0;

    rmfd_wwan_setup (ctx->wwan,
                     FALSE,
                     (GAsyncReadyCallback)wwan_setup_stop_ready,
                     ctx);
}

static void
disconnect (RunContext *ctx)
{
    QmiMessageWdsStopNetworkInput *input;

    if (ctx->self->priv->connection_status != RMF_CONNECTION_STATUS_CONNECTED) {
        switch (ctx->self->priv->connection_status) {
        case RMF_CONNECTION_STATUS_DISCONNECTING:
            g_warning ("error: cannot disconnect: already disconnecting");
            g_simple_async_result_set_op_res_gpointer (
                ctx->result,
                rmfd_error_message_new_from_error (ctx->request, RMFD_ERROR, RMFD_ERROR_INVALID_STATE),
                (GDestroyNotify)g_byte_array_unref);
            break;
        case RMF_CONNECTION_STATUS_CONNECTING:
            g_warning ("error: cannot disconnect: currently connecting");
            g_simple_async_result_set_op_res_gpointer (
                ctx->result,
                rmfd_error_message_new_from_error (ctx->request, RMFD_ERROR, RMFD_ERROR_INVALID_STATE),
                (GDestroyNotify)g_byte_array_unref);
            break;
        case RMF_CONNECTION_STATUS_DISCONNECTED: {
            guint8 *response;

            g_debug ("already disconnected");
            response = rmf_message_disconnect_response_new ();
            g_simple_async_result_set_op_res_gpointer (
                ctx->result,
                g_byte_array_new_take (response, rmf_message_get_length (response)),
                (GDestroyNotify)g_byte_array_unref);
            break;
        }
        case RMF_CONNECTION_STATUS_CONNECTED:
        default:
            g_assert_not_reached ();
        }

        run_context_complete_and_free (ctx);
        return;
    }

    /* Now disconnecting */
    ctx->self->priv->connection_status = RMF_CONNECTION_STATUS_DISCONNECTING;

    input = qmi_message_wds_stop_network_input_new ();
    qmi_message_wds_stop_network_input_set_packet_data_handle (input, ctx->self->priv->packet_data_handle, NULL);

    qmi_client_wds_stop_network (QMI_CLIENT_WDS (ctx->self->priv->wds),
                                 input,
                                 30,
                                 NULL,
                                 (GAsyncReadyCallback)wds_stop_network_ready,
                                 ctx);
    qmi_message_wds_stop_network_input_unref (input);
}

/**********************/

void
rmfd_processor_run (RmfdProcessor       *self,
                    GByteArray          *request,
                    RmfdWwan            *wwan,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
    RunContext *ctx;

    ctx = g_slice_new0 (RunContext);
    ctx->self = g_object_ref (self);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             rmfd_processor_run);
    ctx->request = g_byte_array_ref (request);
    ctx->wwan = g_object_ref (wwan);

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
    case RMF_MESSAGE_COMMAND_ENABLE_PIN:
        enable_pin (ctx);
        return;
    case RMF_MESSAGE_COMMAND_CHANGE_PIN:
        change_pin (ctx);
        return;
    case RMF_MESSAGE_COMMAND_GET_POWER_STATUS:
        get_power_status (ctx);
        return;
    case RMF_MESSAGE_COMMAND_SET_POWER_STATUS:
        set_power_status (ctx);
        return;
    case RMF_MESSAGE_COMMAND_GET_POWER_INFO:
        get_power_info (ctx);
        return;
    case RMF_MESSAGE_COMMAND_GET_SIGNAL_INFO:
        get_signal_info (ctx);
        return;
    case RMF_MESSAGE_COMMAND_GET_REGISTRATION_STATUS:
        get_registration_status (ctx);
        return;
    case RMF_MESSAGE_COMMAND_GET_CONNECTION_STATUS:
        get_connection_status (ctx);
        return;
    case RMF_MESSAGE_COMMAND_GET_CONNECTION_STATS:
        get_connection_stats (ctx);
        return;
    case RMF_MESSAGE_COMMAND_CONNECT:
        connect (ctx);
        return;
    case RMF_MESSAGE_COMMAND_DISCONNECT:
        disconnect (ctx);
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
                     (QMI_DEVICE_OPEN_FLAGS_SYNC |
                      QMI_DEVICE_OPEN_FLAGS_VERSION_INFO |
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
    self->priv->connection_status = RMF_CONNECTION_STATUS_DISCONNECTED;
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

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

#include <libqmi-glib.h>
#include <string.h>

#include <rmf-messages.h>

#include "rmfd-syslog.h"
#include "rmfd-port-processor-qmi.h"
#include "rmfd-error.h"
#include "rmfd-error-types.h"
#include "rmfd-sms-part.h"
#include "rmfd-sms-part-3gpp.h"
#include "rmfd-sms-list.h"

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (RmfdPortProcessorQmi, rmfd_port_processor_qmi, RMFD_TYPE_PORT_PROCESSOR, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               async_initable_iface_init))

#define GLOBAL_PACKET_DATA_HANDLE 0xFFFFFFFF

#define DEFAULT_REGISTRATION_TIMEOUT_SECS 60
#define DEFAULT_REGISTRATION_TIMEOUT_LOGGING_SECS 10

struct _RmfdPortProcessorQmiPrivate {
    /* QMI device */
    QmiDevice *qmi_device;

    /* QMI clients */
    QmiClient *dms;
    QmiClient *nas;
    QmiClient *wds;
    QmiClient *uim;
    QmiClient *wms;

    /* Connection related info */
    RmfConnectionStatus connection_status;
    guint32 packet_data_handle;

    /* Registration related info */
    guint32 registration_timeout;
    gpointer *registration_ctx;
    guint serving_system_indication_id;
    RmfRegistrationStatus registration_status;
    guint16 operator_mcc;
    guint16 operator_mnc;
    gchar *operator_description;
    guint16 lac;
    guint32 cid;

    /* Messaging related info */
    guint messaging_event_report_indication_id;
    RmfdSmsList *messaging_sms_list;
};

static void initiate_registration (RmfdPortProcessorQmi *self, gboolean with_timeout);
static void messaging_list        (RmfdPortProcessorQmi *self);

/*****************************************************************************/
/* Registration timeout handling */

typedef struct {
    guint timeout_secs;
    guint ongoing_secs;
    guint timeout_id;
    GCancellable *scanning;
} RegistrationContext;

static void registration_context_step (RmfdPortProcessorQmi *self);

static void
registration_context_cleanup (RmfdPortProcessorQmi *self)
{
    RegistrationContext *ctx = (RegistrationContext *)self->priv->registration_ctx;

    if (!ctx)
        return;

    if (ctx->timeout_id)
        g_source_remove (ctx->timeout_id);

    if (ctx->scanning) {
        if (self->priv->registration_status == RMF_REGISTRATION_STATUS_SCANNING)
            self->priv->registration_status = RMF_REGISTRATION_STATUS_IDLE;
        g_object_unref (ctx->scanning);
    }

    g_slice_free (RegistrationContext, ctx);
    self->priv->registration_ctx = NULL;
}

static void
registration_context_cancel (RmfdPortProcessorQmi *self)
{
    RegistrationContext *ctx = (RegistrationContext *)self->priv->registration_ctx;

    if (!ctx)
        return;

    if (ctx->scanning && !g_cancellable_is_cancelled (ctx->scanning))
        g_cancellable_cancel (ctx->scanning);

    registration_context_cleanup (self);
}

static void
nas_network_scan_ready (QmiClientNas *client,
                        GAsyncResult *res,
                        RmfdPortProcessorQmi *self)
{
    RegistrationContext *ctx = (RegistrationContext *)self->priv->registration_ctx;
    QmiMessageNasNetworkScanOutput *output;
    gboolean needs_registration_again = TRUE;

    /* Ignore the result of the scan */
    output = qmi_client_nas_network_scan_finish (client, res, NULL);
    if (output)
        qmi_message_nas_network_scan_output_unref (output);
    else if (g_cancellable_is_cancelled (ctx->scanning))
        needs_registration_again = FALSE;

    /* Stop registration context */
    registration_context_cleanup (self);

    /* Relaunch automatic registration without timeout */
    if (needs_registration_again)
        initiate_registration (self, FALSE);

    g_object_unref (self);
}

static gboolean
registration_context_timeout_cb (RmfdPortProcessorQmi *self)
{
    RegistrationContext *ctx = (RegistrationContext *)self->priv->registration_ctx;

    g_assert (ctx != NULL);

    ctx->timeout_id = 0;
    registration_context_step (self);
    return FALSE;
}

static void
registration_context_step (RmfdPortProcessorQmi *self)
{
    RegistrationContext *ctx = (RegistrationContext *)self->priv->registration_ctx;

    g_assert (ctx != NULL);
    g_assert (ctx->timeout_id == 0);

    if (ctx->timeout_secs > ctx->ongoing_secs) {
        guint next_timeout_secs;

        g_debug ("Automatic network registration ongoing... (%u seconds elapsed)", ctx->ongoing_secs);

        next_timeout_secs = MIN (DEFAULT_REGISTRATION_TIMEOUT_LOGGING_SECS,
                                 ctx->timeout_secs - ctx->ongoing_secs);
        ctx->ongoing_secs += next_timeout_secs;
        ctx->timeout_id = g_timeout_add_seconds (next_timeout_secs,
                                                 (GSourceFunc)registration_context_timeout_cb,
                                                 self);
        return;
    }

    /* Expired... */
    g_debug ("Automatic network registration timed out... launching network scan");

    /* Explicit network scan... */
    self->priv->registration_status = RMF_REGISTRATION_STATUS_SCANNING;

    g_assert (ctx->scanning == NULL);
    ctx->scanning = g_cancellable_new ();
    qmi_client_nas_network_scan (QMI_CLIENT_NAS (self->priv->nas),
                                 NULL,
                                 120,
                                 ctx->scanning,
                                 (GAsyncReadyCallback)nas_network_scan_ready,
                                 g_object_ref (self));
}

static void
registration_context_start (RmfdPortProcessorQmi *self)
{
    RegistrationContext *ctx;

    g_assert (self->priv->registration_ctx == NULL);
    g_assert (self->priv->registration_timeout >= 10);

    ctx = g_slice_new0 (RegistrationContext);
    ctx->timeout_secs = self->priv->registration_timeout;

    self->priv->registration_ctx = (gpointer)ctx;
    registration_context_step (self);
}

/*****************************************************************************/
/* Explicit registration request */

static gboolean
initiate_registration_idle_cb (RmfdPortProcessorQmi *self)
{
    QmiMessageNasInitiateNetworkRegisterInput *input;

    input = qmi_message_nas_initiate_network_register_input_new ();
    qmi_message_nas_initiate_network_register_input_set_action (
        input,
        QMI_NAS_NETWORK_REGISTER_TYPE_AUTOMATIC,
        NULL);
    qmi_client_nas_initiate_network_register (
        QMI_CLIENT_NAS (self->priv->nas),
        input,
        10,
        NULL,
        NULL,
        NULL);
    qmi_message_nas_initiate_network_register_input_unref (input);

    g_object_unref (self);
    return FALSE;
}

static void
initiate_registration (RmfdPortProcessorQmi *self,
                       gboolean with_timeout)
{
    /* Don't relaunch if already registered */
    if (self->priv->registration_status == RMF_REGISTRATION_STATUS_HOME ||
        self->priv->registration_status == RMF_REGISTRATION_STATUS_ROAMING)
        return;

    if (with_timeout) {
        g_debug ("Launching automatic network registration... (with %u seconds timeout)",
                 self->priv->registration_timeout);
        registration_context_cancel (self);
        registration_context_start (self);
    } else {
        g_debug ("Launching automatic network registration...");
    }

    /* Launch in idle to make sure we cancel the previous registration attempt, if any */
    g_idle_add ((GSourceFunc)initiate_registration_idle_cb, g_object_ref (self));
}

/*****************************************************************************/
/* Registration info gathering via indications */

static void
process_serving_system_info (RmfdPortProcessorQmi *self,
                             QmiMessageNasGetServingSystemOutput *response,
                             QmiIndicationNasServingSystemOutput *indication)
{
    QmiNasRegistrationState registration_state = QMI_NAS_REGISTRATION_STATE_UNKNOWN;
    QmiNasRoamingIndicatorStatus roaming = QMI_NAS_ROAMING_INDICATOR_STATUS_OFF;

    g_assert ((response && !indication) || (!response && indication));

    /* Registration state */
    if (indication) {
        qmi_indication_nas_serving_system_output_get_serving_system (
            indication, &registration_state, NULL, NULL, NULL, NULL, NULL);
        qmi_indication_nas_serving_system_output_get_roaming_indicator (
            indication, &roaming, NULL);
    } else {
        qmi_message_nas_get_serving_system_output_get_serving_system (
            response, &registration_state, NULL, NULL, NULL, NULL, NULL);
        qmi_message_nas_get_serving_system_output_get_roaming_indicator (
            response, &roaming, NULL);
    }

    switch (registration_state) {
    case QMI_NAS_REGISTRATION_STATE_REGISTERED:
        self->priv->registration_status = (roaming == QMI_NAS_ROAMING_INDICATOR_STATUS_ON ?
                                           RMF_REGISTRATION_STATUS_ROAMING :
                                           RMF_REGISTRATION_STATUS_HOME);

        /* If we had a timeout waiting to get registered, remove it */
        registration_context_cancel (self);
        break;
    case QMI_NAS_REGISTRATION_STATE_NOT_REGISTERED_SEARCHING:
        /* Don't overwrite the 'scanning state' */
        if (self->priv->registration_status != RMF_REGISTRATION_STATUS_SCANNING)
            self->priv->registration_status = RMF_REGISTRATION_STATUS_SEARCHING;
        break;
    case QMI_NAS_REGISTRATION_STATE_NOT_REGISTERED:
    case QMI_NAS_REGISTRATION_STATE_REGISTRATION_DENIED:
    case QMI_NAS_REGISTRATION_STATE_UNKNOWN:
    default:
        /* Don't overwrite the 'scanning state' */
        if (self->priv->registration_status != RMF_REGISTRATION_STATUS_SCANNING)
            self->priv->registration_status = RMF_REGISTRATION_STATUS_IDLE;
        break;
    }

    /* Operator info */
    if (self->priv->registration_status == RMF_REGISTRATION_STATUS_HOME ||
        self->priv->registration_status == RMF_REGISTRATION_STATUS_ROAMING) {
        const gchar *description = NULL;

        if (indication)
            qmi_indication_nas_serving_system_output_get_current_plmn (
                indication, &self->priv->operator_mcc, &self->priv->operator_mnc, &description, NULL);
        else
            qmi_message_nas_get_serving_system_output_get_current_plmn (
                response, &self->priv->operator_mcc, &self->priv->operator_mnc, &description, NULL);
        if (description) {
            g_free (self->priv->operator_description);
            self->priv->operator_description = g_strdup (description);
        }
    } else {
        g_free (self->priv->operator_description);
        self->priv->operator_description = NULL;
    }

    /* LAC/CI */
    if (indication) {
        qmi_indication_nas_serving_system_output_get_lac_3gpp (
            indication, &self->priv->lac, NULL);
        qmi_indication_nas_serving_system_output_get_cid_3gpp (
            indication, &self->priv->cid, NULL);
    } else {
        qmi_message_nas_get_serving_system_output_get_lac_3gpp (
            response, &self->priv->lac, NULL);
        qmi_message_nas_get_serving_system_output_get_cid_3gpp (
            response, &self->priv->cid, NULL);
    }
}

static void
serving_system_indication_cb (QmiClientNas *client,
                              QmiIndicationNasServingSystemOutput *output,
                              RmfdPortProcessorQmi *self)
{
    process_serving_system_info (self, NULL, output);
}

static void
serving_system_response_cb (QmiClientNas *client,
                            GAsyncResult *res,
                            RmfdPortProcessorQmi *self)
{
    QmiMessageNasGetServingSystemOutput *output;

    output = qmi_client_nas_get_serving_system_finish (client, res, NULL);
    if (output) {
        if (qmi_message_nas_get_serving_system_output_get_result (output, NULL))
            process_serving_system_info (self, output, NULL);
        qmi_message_nas_get_serving_system_output_unref (output);
    }
    g_object_unref (self);
}

static void
unregister_nas_indications (RmfdPortProcessorQmi *self)
{
    QmiMessageNasRegisterIndicationsInput *input;

    if (self->priv->serving_system_indication_id == 0)
        return;

    g_signal_handler_disconnect (self->priv->nas, self->priv->serving_system_indication_id);
    self->priv->serving_system_indication_id = 0;

    input = qmi_message_nas_register_indications_input_new ();
    qmi_message_nas_register_indications_input_set_serving_system_events (input, FALSE, NULL);
    qmi_client_nas_register_indications (QMI_CLIENT_NAS (self->priv->nas), input, 5, NULL, NULL, NULL);
    qmi_message_nas_register_indications_input_unref (input);
}

static void
register_nas_indications (RmfdPortProcessorQmi *self)
{
    QmiMessageNasRegisterIndicationsInput *input;

    g_assert (self->priv->nas != NULL);
    g_assert (self->priv->serving_system_indication_id == 0);

    self->priv->serving_system_indication_id =
        g_signal_connect (self->priv->nas,
                          "serving-system",
                          G_CALLBACK (serving_system_indication_cb),
                          self);

    input = qmi_message_nas_register_indications_input_new ();
    qmi_message_nas_register_indications_input_set_serving_system_events (input, TRUE, NULL);
    qmi_client_nas_register_indications (QMI_CLIENT_NAS (self->priv->nas), input, 5, NULL, NULL, NULL);
    qmi_message_nas_register_indications_input_unref (input);

    qmi_client_nas_get_serving_system (QMI_CLIENT_NAS (self->priv->nas),
                                       NULL,
                                       10,
                                       NULL,
                                       (GAsyncReadyCallback)serving_system_response_cb,
                                       g_object_ref (self));
}

/*****************************************************************************/
/* Process an action */

typedef struct {
    RmfdPortProcessorQmi *self;
    GSimpleAsyncResult *result;
    GByteArray *request;
    RmfdPortData *data;
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
    g_object_unref (ctx->data);
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

static GByteArray *
run_finish (RmfdPortProcessor *self,
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
/* Get SIM info */

typedef enum {
    GET_SIM_INFO_STEP_FIRST,
    GET_SIM_INFO_STEP_IMSI,
    GET_SIM_INFO_STEP_EFAD,
    GET_SIM_INFO_STEP_EFOPLMNWACT,
    GET_SIM_INFO_STEP_LAST
} GetSimInfoStep;

typedef struct {
    gchar *imsi;
    guint32 mcc;
    guint32 mnc;
    GArray *plmns;
    GetSimInfoStep step;
} GetSimInfoContext;

static void
get_sim_info_context_free (GetSimInfoContext *ctx)
{
    if (ctx->plmns)
        g_array_unref (ctx->plmns);
    g_free (ctx->imsi);
    g_free (ctx);
}

static void get_sim_info_step (RunContext *ctx);

static void
read_bcd_encoded_mccmnc (const guint8 *data,
                         guint32 data_len,
                         guint32 *mcc,
                         guint32 *mnc)
{
    static const gchar bcd_chars[] = "0123456789\0\0\0\0\0\0";
    gchar mcc_str[4];
    gchar mnc_str[4];

    if (data_len < 3) {
        *mcc = 0;
        *mnc = 0;
        return;
    }

    mcc_str[0] = bcd_chars[(data[0] >> 4) & 0xF];
    mcc_str[1] = bcd_chars[data[0] & 0xF];
    mcc_str[2] = bcd_chars[(data[1] >> 4) & 0xF];
    mcc_str[3] = '\0';
    *mcc = atoi (mcc_str);

    mnc_str[0] = bcd_chars[data[1] & 0xF];
    mnc_str[1] = bcd_chars[(data[2] >> 4) & 0xF];
    mnc_str[2] = bcd_chars[data[2] & 0xF];
    /* This one may be '1111' if 2-digit MNC, which will translate to '\0' anyway */
    mnc_str[3] = '\0';
    *mnc = atoi (mnc_str);
}

static void
read_act (const guint8 *data,
          guint32 data_len,
          guint8 *gsm,
          guint8 *umts,
          guint8 *lte)
{
    *gsm = FALSE;
    *umts = FALSE;
    *lte = FALSE;

    if (data_len < 2)
        return;

    if (data[0] & 0x80)
        *umts = TRUE;
    if (data[0] & 0x40)
        *lte = TRUE;
    if (data[1] & 0x80)
        *gsm = TRUE;
}

static void
parse_plmns (GetSimInfoContext *get_sim_info_ctx,
             const guint8 *bytearray,
             guint32 bytearray_size)
{
    guint i;

    if (!bytearray || !bytearray_size)
        return;

    get_sim_info_ctx->plmns = g_array_sized_new (FALSE,
                                                 FALSE,
                                                 sizeof (RmfPlmnInfo),
                                                 (bytearray_size / 5) + 1);

    for (i = 0; (bytearray_size - i) >= 5; i+=5) {
        RmfPlmnInfo plmn;

        read_bcd_encoded_mccmnc (&bytearray[i],
                                 bytearray_size - i,
                                 &plmn.mcc,
                                 &plmn.mnc);

        read_act (&bytearray[i + 3],
                  bytearray_size - i - 3,
                  &plmn.gsm,
                  &plmn.umts,
                  &plmn.lte);

        g_array_append_val (get_sim_info_ctx->plmns, plmn);
    }
}

static void
sim_info_efoplmnwact_uim_read_transparent_ready (QmiClientUim *client,
                                                 GAsyncResult *res,
                                                 RunContext   *ctx)
{
    GetSimInfoContext *get_sim_info_ctx = (GetSimInfoContext *)ctx->additional_context;
    QmiMessageUimReadTransparentOutput *output;

    /* Enable for testing */
#if 0
    {
        static const guint8 example[] = {
            0x21, 0x40, 0x3F, 0x40, 0x00, /* MCC:214, MNC:03, LTE */
            0x21, 0x40, 0x3F, 0x80, 0x80, /* MCC:214, MNC:03, GSM, UMTS */
            0x21, 0x40, 0x3F, 0xC0, 0x80, /* MCC:214, MNC:03, GSM, UMTS, LTE */
        };

        parse_plmns (get_sim_info_ctx, example, G_N_ELEMENTS (example));

        /* Always call finish() for completeness */
        output = qmi_client_uim_read_transparent_finish (client, res, NULL);
    }
#else
    {
        GError *error = NULL;
        GArray *read_result = NULL;

        output = qmi_client_uim_read_transparent_finish (client, res, &error);
        if (!output || !qmi_message_uim_read_transparent_output_get_result (output, &error)) {
            g_error_free (error);
        } else if (qmi_message_uim_read_transparent_output_get_read_result (
                       output,
                       &read_result,
                       NULL)) {
            parse_plmns (get_sim_info_ctx, read_result->data, read_result->len);
        }
    }
#endif

    if (output)
        qmi_message_uim_read_transparent_output_unref (output);

    /* And go on */
    get_sim_info_ctx->step++;
    get_sim_info_step (ctx);
}

static void
sim_info_efad_uim_read_transparent_ready (QmiClientUim *client,
                                          GAsyncResult *res,
                                          RunContext   *ctx)
{
    GetSimInfoContext *get_sim_info_ctx = (GetSimInfoContext *)ctx->additional_context;
    QmiMessageUimReadTransparentOutput *output;
    GArray *read_result = NULL;
    guint8 mnc_length = 0; /* just to mark it invalid */

    output = qmi_client_uim_read_transparent_finish (client, res, NULL);
    if (output &&
        qmi_message_uim_read_transparent_output_get_result (output, NULL) &&
        qmi_message_uim_read_transparent_output_get_read_result (
            output,
            &read_result,
            NULL)) {
        /* MCN length is optional; available in the 4th byte of the EFad field */
        if (read_result->len >= 4) {
            mnc_length = g_array_index (read_result, guint8, 3);
            if (mnc_length != 3 && mnc_length != 2)
                /* It must be either 3 or 2, no other values allowed. */
                mnc_length = 0; /* invalid */
        }
    }

    /* Compute MCC and MNC values */
    if (strlen (get_sim_info_ctx->imsi) >= 3) {
        gchar aux[4];

        memcpy (aux, get_sim_info_ctx->imsi, 3);
        aux[3] = '\0';
        get_sim_info_ctx->mcc = atoi (aux);

        if (mnc_length == 0)
            mnc_length = rmfd_utils_get_mnc_length_for_mcc (aux);

        if (strlen (get_sim_info_ctx->imsi) >= (3 + mnc_length)) {
            memcpy (aux, &get_sim_info_ctx->imsi[3], mnc_length);
            aux[mnc_length] = '\0';
            get_sim_info_ctx->mnc = atoi (aux);
        }
    }

    if (output)
        qmi_message_uim_read_transparent_output_unref (output);

    /* And go on */
    get_sim_info_ctx->step++;
    get_sim_info_step (ctx);
}

static void
sim_info_dms_uim_get_imsi_ready (QmiClientDms *client,
                                 GAsyncResult *res,
                                 RunContext   *ctx)
{
    GetSimInfoContext *get_sim_info_ctx = (GetSimInfoContext *)ctx->additional_context;
    QmiMessageDmsUimGetImsiOutput *output = NULL;
    GError *error = NULL;
    const gchar *str;

    output = qmi_client_dms_uim_get_imsi_finish (client, res, &error);
    if (!output || !qmi_message_dms_uim_get_imsi_output_get_result (output, &error)) {
        /* Ignore these errors; just will set mcc/mnc to 0. And ignore reading
         * EFad, as it won't be needed. */
        g_error_free (error);
        get_sim_info_ctx->step = GET_SIM_INFO_STEP_EFAD + 1;
        get_sim_info_step (ctx);
        return;
    }

    /* Store IMSI temporarily */
    qmi_message_dms_uim_get_imsi_output_get_imsi (output, &str, NULL);
    get_sim_info_ctx->imsi = g_strdup (str);

    if (output)
        qmi_message_dms_uim_get_imsi_output_unref (output);

    /* And go on */
    get_sim_info_ctx->step++;
    get_sim_info_step (ctx);
}

typedef struct {
    gchar *name;
    guint16 path[3];
} SimFile;

static const SimFile sim_files[] = {
    { "EFad",        { 0x3F00, 0x7F20, 0x6FAD } },
    { "EFoplmnwact", { 0x3F00, 0x7F20, 0x6F61 } },
};

static void
get_sim_file_id_and_path (const gchar *file_name,
                          guint16 *file_id,
                          GArray **file_path)
{
    guint i;
    guint8 val;

    for (i = 0; i < G_N_ELEMENTS (sim_files); i++) {
        if (g_str_equal (sim_files[i].name, file_name))
            break;
    }

    g_assert (i != G_N_ELEMENTS (sim_files));

    *file_path = g_array_sized_new (FALSE, FALSE, sizeof (guint8), 4);

    val = sim_files[i].path[0] & 0xFF;
    g_array_append_val (*file_path, val);
    val = (sim_files[i].path[0] >> 8) & 0xFF;
    g_array_append_val (*file_path, val);

    if (sim_files[i].path[2] != 0) {
        val = sim_files[i].path[1] & 0xFF;
        g_array_append_val (*file_path, val);
        val = (sim_files[i].path[1] >> 8) & 0xFF;
        g_array_append_val (*file_path, val);
        *file_id = sim_files[i].path[2];
    } else {
        *file_id = sim_files[i].path[1];
    }
}

static void
get_sim_info_step (RunContext *ctx)
{
    GetSimInfoContext *get_sim_info_ctx = (GetSimInfoContext *)ctx->additional_context;

    switch (get_sim_info_ctx->step) {
    case GET_SIM_INFO_STEP_FIRST:
        /* Fall down */
        get_sim_info_ctx->step++;

    case GET_SIM_INFO_STEP_IMSI:
        qmi_client_dms_uim_get_imsi (QMI_CLIENT_DMS (ctx->self->priv->dms),
                                     NULL,
                                     5,
                                     NULL,
                                     (GAsyncReadyCallback) sim_info_dms_uim_get_imsi_ready,
                                     ctx);
        return;

    case GET_SIM_INFO_STEP_EFAD: {
        QmiMessageUimReadTransparentInput *input;
        guint16 file_id = 0;
        GArray *file_path = NULL;

        get_sim_file_id_and_path ("EFad", &file_id, &file_path);

        input = qmi_message_uim_read_transparent_input_new ();
        qmi_message_uim_read_transparent_input_set_session_information (
            input,
            QMI_UIM_SESSION_TYPE_PRIMARY_GW_PROVISIONING,
            "",
            NULL);
        qmi_message_uim_read_transparent_input_set_file (
            input,
            file_id,
            file_path,
            NULL);
        qmi_message_uim_read_transparent_input_set_read_information (input, 0, 0, NULL);
        g_array_unref (file_path);

        qmi_client_uim_read_transparent (QMI_CLIENT_UIM (ctx->self->priv->uim),
                                         input,
                                         10,
                                         NULL,
                                         (GAsyncReadyCallback)sim_info_efad_uim_read_transparent_ready,
                                         ctx);
        qmi_message_uim_read_transparent_input_unref (input);
        return;
    }

    case GET_SIM_INFO_STEP_EFOPLMNWACT: {
        QmiMessageUimReadTransparentInput *input;
        guint16 file_id = 0;
        GArray *file_path = NULL;

        get_sim_file_id_and_path ("EFoplmnwact", &file_id, &file_path);

        input = qmi_message_uim_read_transparent_input_new ();
        qmi_message_uim_read_transparent_input_set_session_information (
            input,
            QMI_UIM_SESSION_TYPE_PRIMARY_GW_PROVISIONING,
            "",
            NULL);
        qmi_message_uim_read_transparent_input_set_file (
            input,
            file_id,
            file_path,
            NULL);
        qmi_message_uim_read_transparent_input_set_read_information (input, 0, 0, NULL);
        g_array_unref (file_path);

        qmi_client_uim_read_transparent (QMI_CLIENT_UIM (ctx->self->priv->uim),
                                         input,
                                         10,
                                         NULL,
                                         (GAsyncReadyCallback)sim_info_efoplmnwact_uim_read_transparent_ready,
                                         ctx);
        qmi_message_uim_read_transparent_input_unref (input);
        return;
    }

    case GET_SIM_INFO_STEP_LAST: {
        /* Build result */
        guint8 *response;

        response = rmf_message_get_sim_info_response_new (get_sim_info_ctx->mcc,
                                                          get_sim_info_ctx->mnc,
                                                          get_sim_info_ctx->plmns ? get_sim_info_ctx->plmns->len : 0,
                                                          get_sim_info_ctx->plmns ? (const RmfPlmnInfo *)get_sim_info_ctx->plmns->data : NULL);
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
        run_context_complete_and_free (ctx);
        return;
    }

    default:
        break;
    }

    g_assert_not_reached ();
}

static void
get_sim_info (RunContext *ctx)
{
    GetSimInfoContext *get_sim_info_ctx;

    get_sim_info_ctx = g_new0 (GetSimInfoContext, 1);
    get_sim_info_ctx->step = GET_SIM_INFO_STEP_FIRST;
    run_context_set_additional_context (ctx,
                                        get_sim_info_ctx,
                                        (GDestroyNotify)get_sim_info_context_free);

    get_sim_info_step (ctx);
}

/**********************/
/* Common unlock + ready check */

typedef enum {
    COMMON_UNLOCK_CHECK_STEP_FIRST,
    COMMON_UNLOCK_CHECK_STEP_LOCK_STATUS,
    COMMON_UNLOCK_CHECK_STEP_SIM_READY_STATUS,
    COMMON_UNLOCK_CHECK_STEP_LAST,
} CommonUnlockCheckStep;

typedef struct {
    RmfdPortProcessorQmi *self;
    GSimpleAsyncResult *simple;
    CommonUnlockCheckStep step;
    gboolean unlocked;
} CommonUnlockCheckContext;

static void
common_unlock_check_context_complete_and_free (CommonUnlockCheckContext *ctx)
{
    g_simple_async_result_complete (ctx->simple);
    g_object_unref (ctx->simple);
    g_object_unref (ctx->self);
    g_slice_free (CommonUnlockCheckContext, ctx);
}

static gboolean
common_unlock_check_finish (RmfdPortProcessorQmi  *self,
                            GAsyncResult          *res,
                            gboolean              *unlocked,
                            GError               **error)
{
    g_assert (unlocked != NULL);

    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return FALSE;

    *unlocked = g_simple_async_result_get_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (res));
    return TRUE;
}

static void common_unlock_check_context_step (CommonUnlockCheckContext *ctx);

static void
get_card_status_ready (QmiClientUim *client,
                       GAsyncResult *res,
                       CommonUnlockCheckContext *ctx)
{
    QmiMessageUimGetCardStatusOutput *output;
    GError *error = NULL;
    GArray *cards;
    guint i;

    output = qmi_client_uim_get_card_status_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->simple, error);
        common_unlock_check_context_complete_and_free (ctx);
        return;
    }

    if (!qmi_message_uim_get_card_status_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't get card status: ");
        g_simple_async_result_take_error (ctx->simple, error);
        qmi_message_uim_get_card_status_output_unref (output);
        common_unlock_check_context_complete_and_free (ctx);
        return;
    }

    qmi_message_uim_get_card_status_output_get_card_status (
        output,
        NULL, /* index_gw_primary */
        NULL, /* index_1x_primary */
        NULL, /* index_gw_secondary */
        NULL, /* index_1x_secondary */
        &cards,
        NULL);

    if (cards->len == 0) {
        g_simple_async_result_set_error (ctx->simple,
                                         RMFD_ERROR,
                                         RMFD_ERROR_UNKNOWN,
                                         "No cards reported");
        qmi_message_uim_get_card_status_output_unref (output);
        common_unlock_check_context_complete_and_free (ctx);
        return;
    }

    if (cards->len > 1)
        g_debug ("Multiple cards reported: %u", cards->len);

    /* All KNOWN applications in all cards will need to be in READY state for us
     * to consider UNLOCKED */
    for (i = 0; i < cards->len; i++) {
        QmiMessageUimGetCardStatusOutputCardStatusCardsElement *card;

        card = &g_array_index (cards, QmiMessageUimGetCardStatusOutputCardStatusCardsElement, i);

        switch (card->card_state) {
        case QMI_UIM_CARD_STATE_PRESENT: {
            guint j;
            guint sim_or_usim_ready = 0;

            if (card->applications->len == 0) {
                g_simple_async_result_set_error (ctx->simple, RMFD_ERROR, RMFD_ERROR_UNKNOWN,
                                                 "No applications reported in card [%u]", i);
                qmi_message_uim_get_card_status_output_unref (output);
                common_unlock_check_context_complete_and_free (ctx);
                return;
            }

            if (card->applications->len > 1)
                g_debug ("Multiple applications reported in card [%u]: %u", i, card->applications->len);

            for (j = 0; j < card->applications->len; j++) {
                QmiMessageUimGetCardStatusOutputCardStatusCardsElementApplicationsElement *app;

                app = &g_array_index (card->applications, QmiMessageUimGetCardStatusOutputCardStatusCardsElementApplicationsElement, j);

                if (app->type == QMI_UIM_CARD_APPLICATION_TYPE_UNKNOWN) {
                    g_debug ("Unknown application [%u] found in card [%u]: %s. Ignored.",
                             j, i, qmi_uim_card_application_state_get_string (app->state));
                    continue;
                }

                g_debug ("Application '%s' [%u] in card [%u]: %s",
                         qmi_uim_card_application_type_get_string (app->type), j, i, qmi_uim_card_application_state_get_string (app->state));

                if ((app->type == QMI_UIM_CARD_APPLICATION_TYPE_SIM || app->type == QMI_UIM_CARD_APPLICATION_TYPE_USIM) &&
                    (app->state == QMI_UIM_CARD_APPLICATION_STATE_READY))
                    sim_or_usim_ready++;
            }

            if (!sim_or_usim_ready) {
                g_debug ("Neither SIM nor USIM are ready");
                ctx->unlocked = FALSE;
                ctx->step = COMMON_UNLOCK_CHECK_STEP_LAST;
                qmi_message_uim_get_card_status_output_unref (output);
                common_unlock_check_context_step (ctx);
                return;
            }

            break;
        }

        case QMI_UIM_CARD_STATE_ABSENT:
            g_simple_async_result_set_error (ctx->simple, RMFD_ERROR, RMFD_ERROR_UNKNOWN,
                                             "Card '%u' is absent", i);
            qmi_message_uim_get_card_status_output_unref (output);
            common_unlock_check_context_complete_and_free (ctx);
            return;

        case QMI_UIM_CARD_STATE_ERROR:
        default:
            if (qmi_uim_card_error_get_string (card->error_code) != NULL)
                g_simple_async_result_set_error (ctx->simple, RMFD_ERROR, RMFD_ERROR_UNKNOWN,
                                                 "Card '%u' is unusable: %s",
                                                 i, qmi_uim_card_error_get_string (card->error_code));
            else
                g_simple_async_result_set_error (ctx->simple, RMFD_ERROR, RMFD_ERROR_UNKNOWN,
                                                 "Card '%u' is unusable: unknown error (%u)",
                                                 i, card->error_code);

            qmi_message_uim_get_card_status_output_unref (output);
            common_unlock_check_context_complete_and_free (ctx);
            return;
        }

        /* go on to next card */
    }

    /* We're done */
    ctx->unlocked = TRUE;
    ctx->step = COMMON_UNLOCK_CHECK_STEP_LAST;
    common_unlock_check_context_step (ctx);
}

static void
get_pin_status_ready (QmiClientDms             *client,
                      GAsyncResult             *res,
                      CommonUnlockCheckContext *ctx)
{
    QmiMessageDmsUimGetPinStatusOutput *output = NULL;
    GError *error = NULL;
    QmiDmsUimPinStatus current_status;
    guint8 *response;

    output = qmi_client_dms_uim_get_pin_status_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->simple, error);
        common_unlock_check_context_complete_and_free (ctx);
        return;
    }

    if (!qmi_message_dms_uim_get_pin_status_output_get_result (output, &error)) {
        /* QMI error internal when checking PIN status likely means NO SIM */
        if (g_error_matches (error, QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INTERNAL)) {
            g_error_free (error);
            error = g_error_new (QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NO_SIM, "missing SIM");
        }
        g_prefix_error (&error, "couldn't get PIN status: ");
        g_simple_async_result_take_error (ctx->simple, error);
        qmi_message_dms_uim_get_pin_status_output_unref (output);
        common_unlock_check_context_complete_and_free (ctx);
        return;
    }

    if (!qmi_message_dms_uim_get_pin_status_output_get_pin1_status (
            output,
            &current_status,
            NULL, /* verify_retries_left */
            NULL, /* unblock_retries_left */
            &error)) {
        g_prefix_error (&error, "couldn't get PIN1 status: ");
        g_simple_async_result_take_error (ctx->simple, error);
        qmi_message_dms_uim_get_pin_status_output_unref (output);
        common_unlock_check_context_complete_and_free (ctx);
        return;
    }

    g_debug ("Current PIN1 status retrieved: '%s'", qmi_dms_uim_pin_status_get_string (current_status));

    switch (current_status) {
    case QMI_DMS_UIM_PIN_STATUS_NOT_INITIALIZED:
        g_simple_async_result_set_error (ctx->simple, RMFD_ERROR, RMFD_ERROR_INVALID_STATE,
                                         "SIM is not initialized, cannot check lock status");
        break;

    case QMI_DMS_UIM_PIN_STATUS_CHANGED:
        /* This is a temporary state given after an ChangePin() operation has been performed. */
    case QMI_DMS_UIM_PIN_STATUS_UNBLOCKED:
        /* This is a temporary state given after an Unblock() operation has been performed. */
    case QMI_DMS_UIM_PIN_STATUS_DISABLED:
    case QMI_DMS_UIM_PIN_STATUS_ENABLED_VERIFIED:
        /* PIN is now either unlocked or disabled, we can go on */
        ctx->step++;
        common_unlock_check_context_step (ctx);
        qmi_message_dms_uim_get_pin_status_output_unref (output);
        return;

    case QMI_DMS_UIM_PIN_STATUS_BLOCKED:
        g_simple_async_result_set_error (ctx->simple, RMFD_ERROR, RMFD_ERROR_INVALID_STATE,
                                         "SIM is blocked, needs PUK");
        break;

    case QMI_DMS_UIM_PIN_STATUS_PERMANENTLY_BLOCKED:
        g_simple_async_result_set_error (ctx->simple, RMFD_ERROR, RMFD_ERROR_INVALID_STATE,
                                         "SIM is permanently blocked");
        break;

    case QMI_DMS_UIM_PIN_STATUS_ENABLED_NOT_VERIFIED:
        /* PIN is enabled and locked */
        ctx->unlocked = FALSE;
        ctx->step = COMMON_UNLOCK_CHECK_STEP_LAST;
        common_unlock_check_context_step (ctx);
        qmi_message_dms_uim_get_pin_status_output_unref (output);
        return;

    default:
        g_simple_async_result_set_error (ctx->simple, RMFD_ERROR, RMFD_ERROR_UNKNOWN,
                                         "Unknown lock status");
        break;
    }

    qmi_message_dms_uim_get_pin_status_output_unref (output);
    common_unlock_check_context_complete_and_free (ctx);
}

static void
common_unlock_check_context_step (CommonUnlockCheckContext *ctx)
{
    switch (ctx->step) {
    case COMMON_UNLOCK_CHECK_STEP_FIRST:
        ctx->step++;
        /* Fall down */

    case COMMON_UNLOCK_CHECK_STEP_LOCK_STATUS:
        g_debug ("Checking SIM lock status...");
        qmi_client_dms_uim_get_pin_status (QMI_CLIENT_DMS (ctx->self->priv->dms),
                                           NULL,
                                           5,
                                           NULL,
                                           (GAsyncReadyCallback)get_pin_status_ready,
                                           ctx);
        return;

    case COMMON_UNLOCK_CHECK_STEP_SIM_READY_STATUS:
        g_debug ("Checking SIM readiness status...");
        qmi_client_uim_get_card_status (QMI_CLIENT_UIM (ctx->self->priv->uim),
                                        NULL,
                                        5,
                                        NULL,
                                        (GAsyncReadyCallback)get_card_status_ready,
                                        ctx);
        return;

    case COMMON_UNLOCK_CHECK_STEP_LAST:
        g_simple_async_result_set_op_res_gboolean (ctx->simple, ctx->unlocked);
        common_unlock_check_context_complete_and_free (ctx);
        return;
    }

    g_assert_not_reached ();
}

static void
common_unlock_check (RmfdPortProcessorQmi *self,
                     GAsyncReadyCallback   callback,
                     gpointer              user_data)
{
    CommonUnlockCheckContext *ctx;

    ctx = g_slice_new0 (CommonUnlockCheckContext);
    ctx->self = g_object_ref (self);
    ctx->simple = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             common_unlock_check);
    ctx->step = COMMON_UNLOCK_CHECK_STEP_FIRST;
    ctx->unlocked = FALSE;

    common_unlock_check_context_step (ctx);
}

/**********************/
/* Is Locked */

static void
is_sim_locked_unlock_check_ready (RmfdPortProcessorQmi *self,
                                  GAsyncResult         *res,
                                  RunContext           *ctx)
{
    GError *error = NULL;
    gboolean unlocked = FALSE;

    if (!common_unlock_check_finish (self, res, &unlocked, &error))
        g_simple_async_result_take_error (ctx->result, error);
    else {
        guint8 *response;

        response = rmf_message_is_sim_locked_response_new (!unlocked);
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    run_context_complete_and_free (ctx);
}

static void
is_sim_locked (RunContext *ctx)
{
    common_unlock_check (ctx->self,
                         (GAsyncReadyCallback)is_sim_locked_unlock_check_ready,
                         ctx);
}

/**********************/
/* Unlock PIN */

typedef struct {
    guint after_unlock_checks;
} UnlockPinContext;

static void run_after_unlock_checks (RunContext *ctx);

static void
after_unlock_check_ready (RmfdPortProcessorQmi *self,
                          GAsyncResult         *res,
                          RunContext           *ctx)
{
    gboolean unlocked = FALSE;
    guint8 *response;

    if (!common_unlock_check_finish (self, res, &unlocked, NULL) || !unlocked) {
        /* Sleep & retry */
        run_after_unlock_checks (ctx);
        return;
    }

    /* Unlocked! */
    response = rmf_message_unlock_response_new ();
    g_simple_async_result_set_op_res_gpointer (ctx->result,
                                               g_byte_array_new_take (response, rmf_message_get_length (response)),
                                               (GDestroyNotify)g_byte_array_unref);

    /* Launch automatic registration */
    initiate_registration (ctx->self, TRUE);

    /* Launch SMS listing */
    messaging_list (ctx->self);

    run_context_complete_and_free (ctx);
}

static gboolean
after_unlock_check_cb (RunContext *ctx)
{
    common_unlock_check (ctx->self,
                         (GAsyncReadyCallback)after_unlock_check_ready,
                         ctx);
    return FALSE;
}

static void
run_after_unlock_checks (RunContext *ctx)
{
    UnlockPinContext *unlock_ctx = (UnlockPinContext *)ctx->additional_context;

    if (unlock_ctx->after_unlock_checks == 20) {
        g_simple_async_result_set_error (ctx->result,
                                         RMFD_ERROR,
                                         RMFD_ERROR_UNKNOWN,
                                         "PIN unlocked but too many unlock checks afterwards");
        run_context_complete_and_free (ctx);
        return;
    }

    /* Recheck lock status. The change is not immediate */
    unlock_ctx->after_unlock_checks++;
    g_timeout_add (500, (GSourceFunc)after_unlock_check_cb, ctx);
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
before_unlock_check_ready (RmfdPortProcessorQmi *self,
                           GAsyncResult         *res,
                           RunContext           *ctx)
{
    GError *error = NULL;
    gboolean unlocked = FALSE;
    QmiMessageDmsUimVerifyPinInput *input;
    const gchar *pin;

    if (!common_unlock_check_finish (self, res, &unlocked, &error)) {
        g_simple_async_result_take_error (ctx->result, error);
        run_context_complete_and_free (ctx);
        return;
    }

    /* Unlocked already */
    if (unlocked) {
        guint8 *response;

        response = rmf_message_unlock_response_new ();
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
        /* Launch automatic registration */
        initiate_registration (ctx->self, TRUE);

        /* Launch SMS listing */
        messaging_list (ctx->self);

        run_context_complete_and_free (ctx);
        return;
    }

    /* Locked, send pin */
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
}

static void
unlock (RunContext *ctx)
{
    /* First, check current lock status */
    common_unlock_check (ctx->self,
                         (GAsyncReadyCallback)before_unlock_check_ready,
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

typedef struct {
    QmiDmsOperatingMode mode;
} PowerContext;

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
        PowerContext *power_ctx;

        response = rmf_message_set_power_status_response_new ();
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);

        power_ctx = (PowerContext *)ctx->additional_context;
        if (power_ctx->mode == RMF_POWER_STATUS_FULL)
            /* Launch automatic registration */
            initiate_registration (ctx->self, TRUE);
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
    PowerContext *power_ctx;

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

    power_ctx = g_new (PowerContext, 1);
    power_ctx->mode = mode;
    run_context_set_additional_context (ctx, power_ctx, g_free);

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

/***************************/
/* Get Registration Status */

static void
get_registration_status (RunContext *ctx)
{
    guint8 *response;

    response = (rmf_message_get_registration_status_response_new (
                    ctx->self->priv->registration_status,
                    ctx->self->priv->operator_description,
                    ctx->self->priv->operator_mcc,
                    ctx->self->priv->operator_mnc,
                    ctx->self->priv->lac,
                    ctx->self->priv->cid));

    g_simple_async_result_set_op_res_gpointer (ctx->result,
                                               g_byte_array_new_take (response, rmf_message_get_length (response)),
                                               (GDestroyNotify)g_byte_array_unref);
    run_context_complete_and_free (ctx);
}

/****************************/
/* Get registration timeout */

static void
get_registration_timeout (RunContext *ctx)
{
    guint8 *response;

    response = rmf_message_get_registration_timeout_response_new (ctx->self->priv->registration_timeout);
    g_simple_async_result_set_op_res_gpointer (ctx->result,
                                               g_byte_array_new_take (response, rmf_message_get_length (response)),
                                               (GDestroyNotify)g_byte_array_unref);
    run_context_complete_and_free (ctx);
}

/****************************/
/* Set registration timeout */

static void
set_registration_timeout (RunContext *ctx)
{
    guint8 *response;
    guint32 timeout;

    rmf_message_set_registration_timeout_request_parse (ctx->request->data, &timeout);

    if (timeout < 10) {
        g_simple_async_result_set_error (ctx->result,
                                         RMFD_ERROR,
                                         RMFD_ERROR_UNKNOWN,
                                         "Timeout is too short");
    } else {
        ctx->self->priv->registration_timeout = timeout;

        response = rmf_message_set_registration_timeout_response_new ();
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    run_context_complete_and_free (ctx);
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
data_setup_start_ready (RmfdPortData *data,
                        GAsyncResult *res,
                        RunContext   *ctx)
{
    GError *error = NULL;
    guint8 *response;

    if (!rmfd_port_data_setup_finish (data, res, &error)) {
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
    GByteArray *error_message = NULL;
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
                GString *error_str;

                error_str = g_string_new ("");
                if (qmi_message_wds_start_network_output_get_call_end_reason (
                        output,
                        &cer,
                        NULL)) {
                    const gchar *str;

                    str = qmi_wds_call_end_reason_get_string (cer);
                    g_warning ("call end reason (%u): '%s'",
                               cer, str ? str : "unknown error");
                    if (str)
                        g_string_append_printf (error_str,
                                                "%s (%u)",
                                                str,
                                                (guint)cer);
                }

                if (qmi_message_wds_start_network_output_get_verbose_call_end_reason (
                        output,
                        &verbose_cer_type,
                        &verbose_cer_reason,
                        NULL)) {
                    const gchar *str;
                    const gchar *domain_str;

                    domain_str = qmi_wds_verbose_call_end_reason_type_get_string (verbose_cer_type),
                    str = qmi_wds_verbose_call_end_reason_get_string (verbose_cer_type, verbose_cer_reason);
                    g_warning ("verbose call end reason (%u,%d): [%s] %s",
                               verbose_cer_type,
                               verbose_cer_reason,
                               domain_str,
                               str);

                    if (domain_str || str)
                        g_string_append_printf (error_str,
                                                "%s[%s (%u)] %s (%d)",
                                                error_str->len > 0 ? ": " : "",
                                                domain_str ? domain_str : "unknown domain",
                                                (guint)verbose_cer_type,
                                                str ? str : "unknown error",
                                                verbose_cer_reason);
                }

                error_message = rmfd_error_message_new_from_error (ctx->request,
                                                                   error->domain,
                                                                   error->code,
                                                                   error_str->len > 0 ? error_str->str : "unknown error");
                g_string_free (error_str, TRUE);
            }
        }
    }

    if (output) {
        if (ctx->self->priv->packet_data_handle != GLOBAL_PACKET_DATA_HANDLE)
            qmi_message_wds_start_network_output_get_packet_data_handle (output, &ctx->self->priv->packet_data_handle, NULL);
        qmi_message_wds_start_network_output_unref (output);
    }

    if (error) {
        if (!error_message)
            error_message = rmfd_error_message_new_from_gerror (ctx->request, error);

        ctx->self->priv->connection_status = RMF_CONNECTION_STATUS_DISCONNECTED;
        g_simple_async_result_set_op_res_gpointer (ctx->result, error_message,
                                                   (GDestroyNotify)g_byte_array_unref);
        g_error_free (error);
        run_context_complete_and_free (ctx);
        return;
    }

    rmfd_port_data_setup (ctx->data,
                          TRUE,
                          (GAsyncReadyCallback)data_setup_start_ready,
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
                rmfd_error_message_new_from_error (ctx->request, RMFD_ERROR, RMFD_ERROR_INVALID_STATE, "Currently disconnecting"),
                (GDestroyNotify)g_byte_array_unref);
            break;
        case RMF_CONNECTION_STATUS_CONNECTING:
            g_warning ("error connecting: already connecting");
            g_simple_async_result_set_op_res_gpointer (
                ctx->result,
                rmfd_error_message_new_from_error (ctx->request, RMFD_ERROR, RMFD_ERROR_INVALID_STATE, "Already connecting"),
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
data_setup_stop_ready (RmfdPortData *data,
                       GAsyncResult *res,
                       RunContext   *ctx)
{
    GError *error = NULL;
    guint8 *response;

    if (!rmfd_port_data_setup_finish (data, res, &error)) {
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

    rmfd_port_data_setup (ctx->data,
                          FALSE,
                          (GAsyncReadyCallback)data_setup_stop_ready,
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
                rmfd_error_message_new_from_error (ctx->request, RMFD_ERROR, RMFD_ERROR_INVALID_STATE, "Already disconnecting"),
                (GDestroyNotify)g_byte_array_unref);
            break;
        case RMF_CONNECTION_STATUS_CONNECTING:
            g_warning ("error: cannot disconnect: currently connecting");
            g_simple_async_result_set_op_res_gpointer (
                ctx->result,
                rmfd_error_message_new_from_error (ctx->request, RMFD_ERROR, RMFD_ERROR_INVALID_STATE, "Currently connecting"),
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

static void
run (RmfdPortProcessor   *self,
     GByteArray          *request,
     RmfdPortData        *data,
     GAsyncReadyCallback  callback,
     gpointer             user_data)
{
    RunContext *ctx;

    ctx = g_slice_new0 (RunContext);
    ctx->self = g_object_ref (self);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             rmfd_port_processor_run);
    ctx->request = g_byte_array_ref (request);
    ctx->data = g_object_ref (data);

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
    case RMF_MESSAGE_COMMAND_GET_SIM_INFO:
        get_sim_info (ctx);
        return;
    case RMF_MESSAGE_COMMAND_IS_SIM_LOCKED:
        is_sim_locked (ctx);
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
    case RMF_MESSAGE_COMMAND_GET_REGISTRATION_TIMEOUT:
        get_registration_timeout (ctx);
        return;
    case RMF_MESSAGE_COMMAND_SET_REGISTRATION_TIMEOUT:
        set_registration_timeout (ctx);
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
/* Built SMS */

static void
sms_added_cb (RmfdSmsList          *sms_list,
              RmfdSms              *sms,
              RmfdPortProcessorQmi *self)
{
    const gchar *text_str = NULL;
    const gchar *number_str;
    const gchar *timestamp_str;
    GString *text;
    GList *l;
    gboolean no_delete = FALSE;

    text = rmfd_sms_get_text (sms);
    if (text)
        text_str = text->str;
    number_str = rmfd_sms_get_number (sms);
    timestamp_str = rmfd_sms_get_timestamp (sms);

    rmfd_syslog (LOG_INFO, "[%s] [%s] %s",
                 timestamp_str ? timestamp_str : "",
                 number_str    ? number_str    : "",
                 text_str      ? text_str      : "");

    /* For testing, allow to run without actually removing the already read parts */
    if (getenv ("RMFD_NO_DELETE_SMS"))
        no_delete = TRUE;

    /* Now, remove all parts */
    for (l = rmfd_sms_peek_parts (sms); l; l = g_list_next (l)) {
        g_debug ("[messaging] %sremoving SMS part (%s/%u)",
                 no_delete ? "(fake) " : "",
                 qmi_wms_storage_type_get_string (rmfd_sms_get_storage (sms)),
                 (guint32) rmfd_sms_part_get_index ((RmfdSmsPart *)l->data));
        if (!no_delete) {
            QmiMessageWmsDeleteInput *input;

            input = qmi_message_wms_delete_input_new ();
            qmi_message_wms_delete_input_set_memory_storage (input, rmfd_sms_get_storage (sms), NULL);
            qmi_message_wms_delete_input_set_memory_index   (input, (guint32) rmfd_sms_part_get_index ((RmfdSmsPart *)l->data), NULL);
            qmi_message_wms_delete_input_set_message_mode   (input, QMI_WMS_MESSAGE_MODE_GSM_WCDMA, NULL);
            qmi_client_wms_delete (QMI_CLIENT_WMS (self->priv->wms), input, 5, NULL, NULL, NULL);
            qmi_message_wms_delete_input_unref (input);
        }
    }
}

/*****************************************************************************/
/* Messaging SMS part processing */

static void
process_read_sms_part (RmfdPortProcessorQmi *self,
                       QmiWmsStorageType     storage,
                       guint32               index,
                       QmiWmsMessageTagType  tag,
                       QmiWmsMessageFormat   format,
                       GArray               *data)
{
    RmfdSmsPart *part;
    GError      *error = NULL;

    if (format != QMI_WMS_MESSAGE_FORMAT_GSM_WCDMA_POINT_TO_POINT &&
        format != QMI_WMS_MESSAGE_FORMAT_GSM_WCDMA_BROADCAST) {
        g_debug ("[messaging] ignoring SMS part (%s)", qmi_wms_message_format_get_string (format));
        return;
    }

    g_debug ("[messaging] received 3GPP SMS part (%s,%u)", qmi_wms_storage_type_get_string (storage), index);
    part = rmfd_sms_part_3gpp_new_from_binary_pdu (index, (guint8 *)data->data, data->len, &error);
    if (!part) {
        g_warning ("[messaging] error creating SMS part from PDU: %s", error->message);
        g_error_free (error);
        return;
    }

    if (!rmfd_sms_list_take_part (self->priv->messaging_sms_list,
                                  part,
                                  storage,
                                  tag,
                                  &error)) {
        g_warning ("[messaging] error processing PDU: %s", error->message);
        g_error_free (error);
    }

    rmfd_sms_part_unref (part);
}

/*****************************************************************************/
/* Messaging event report */

typedef struct {
    RmfdPortProcessorQmi *self;
    QmiClientWms         *client;
    QmiWmsStorageType     storage;
    guint32               memory_index;
    QmiWmsMessageMode     message_mode;
} IndicationRawReadContext;

static void
indication_raw_read_context_free (IndicationRawReadContext *ctx)
{
    g_object_unref (ctx->client);
    g_object_unref (ctx->self);
    g_slice_free (IndicationRawReadContext, ctx);
}

static void
wms_indication_raw_read_ready (QmiClientWms             *client,
                               GAsyncResult             *res,
                               IndicationRawReadContext *ctx)
{
    QmiMessageWmsRawReadOutput *output = NULL;
    GError *error = NULL;

    /* Ignore errors */

    output = qmi_client_wms_raw_read_finish (client, res, &error);
    if (!output || !qmi_message_wms_raw_read_output_get_result (output, &error)) {
        g_warning ("[messaging] error reading raw message: %s", error->message);
        g_error_free (error);
    } else {
        QmiWmsMessageTagType  tag;
        QmiWmsMessageFormat   format;
        GArray               *data;

        qmi_message_wms_raw_read_output_get_raw_message_data (output, &tag, &format, &data, NULL);
        process_read_sms_part (ctx->self, ctx->storage, ctx->memory_index, tag, format, data);
    }

    if (output)
        qmi_message_wms_raw_read_output_unref (output);

    indication_raw_read_context_free (ctx);
}

static void
messaging_event_report_indication_cb (QmiClientNas                      *client,
                                      QmiIndicationWmsEventReportOutput *output,
                                      RmfdPortProcessorQmi              *self)
{
    QmiWmsStorageType storage;
    guint32            memory_index;

    /* Currently ignoring transfer-route MT messages */

    if (qmi_indication_wms_event_report_output_get_mt_message (output, &storage, &memory_index, NULL)) {
        IndicationRawReadContext  *ctx;
        QmiMessageWmsRawReadInput *input;

        ctx = g_slice_new (IndicationRawReadContext);
        ctx->self = g_object_ref (self);
        ctx->client = g_object_ref (client);
        ctx->storage = storage;
        ctx->memory_index = memory_index;

        input = qmi_message_wms_raw_read_input_new ();
        qmi_message_wms_raw_read_input_set_message_memory_storage_id (input, storage, memory_index, NULL);
        if (!qmi_indication_wms_event_report_output_get_message_mode (output, &ctx->message_mode, NULL))
            ctx->message_mode = QMI_WMS_MESSAGE_MODE_GSM_WCDMA;
        qmi_message_wms_raw_read_input_set_message_mode (input, ctx->message_mode, NULL);
        qmi_client_wms_raw_read (QMI_CLIENT_WMS (client),
                                 input,
                                 3,
                                 NULL,
                                 (GAsyncReadyCallback)wms_indication_raw_read_ready,
                                 ctx);
        qmi_message_wms_raw_read_input_unref (input);
    }
}

/*****************************************************************************/
/* Messaging shutdown */

static void
unregister_wms_indications (RmfdPortProcessorQmi *self)
{
    QmiMessageWmsSetEventReportInput *input;

    if (self->priv->messaging_event_report_indication_id == 0)
        return;

    g_signal_handler_disconnect (self->priv->wms, self->priv->messaging_event_report_indication_id);
    self->priv->messaging_event_report_indication_id = 0;

    input = qmi_message_wms_set_event_report_input_new ();
    qmi_message_wms_set_event_report_input_set_new_mt_message_indicator (input, FALSE, NULL);
    qmi_client_wms_set_event_report (QMI_CLIENT_WMS (self->priv->wms), input, 5, NULL, NULL, NULL);
    qmi_message_wms_set_event_report_input_unref (input);
}

/*****************************************************************************/
/* Messaging list parts */

typedef enum {
    MESSAGING_LIST_PARTS_CONTEXT_STEP_FIRST,
    MESSAGING_LIST_PARTS_CONTEXT_STEP_LIST_READ,
    MESSAGING_LIST_PARTS_CONTEXT_STEP_LIST_NOT_READ,
    MESSAGING_LIST_PARTS_CONTEXT_STEP_LAST
} MessagingListPartsContextStep;

typedef struct {
    RmfdPortProcessorQmi *self;
    MessagingListPartsContextStep step;
    QmiWmsStorageType storage;
    QmiWmsMessageTagType tag;
    GArray *message_array;
    guint i;
} MessagingListPartsContext;

static void
messaging_list_parts_context_free (MessagingListPartsContext *ctx)
{
    if (ctx->message_array)
        g_array_unref (ctx->message_array);
    g_object_unref (ctx->self);
    g_slice_free (MessagingListPartsContext, ctx);
}

static void messaging_list_parts_context_step (MessagingListPartsContext *ctx);
static void read_next_sms_part                (MessagingListPartsContext *ctx);

static void
wms_raw_read_ready (QmiClientWms              *client,
                    GAsyncResult              *res,
                    MessagingListPartsContext *ctx)
{
    QmiMessageWmsRawReadOutput *output = NULL;
    GError *error = NULL;

    /* Ignore errors */

    output = qmi_client_wms_raw_read_finish (client, res, &error);
    if (!output || !qmi_message_wms_raw_read_output_get_result (output, &error)) {
        g_warning ("[messaging] error reading raw message: %s", error->message);
        g_error_free (error);
    } else {
        QmiWmsMessageTagType  tag;
        QmiWmsMessageFormat   format;
        GArray               *data;
        QmiMessageWmsListMessagesOutputMessageListElement *message;

        message = &g_array_index (ctx->message_array,
                                  QmiMessageWmsListMessagesOutputMessageListElement,
                                  ctx->i);

        qmi_message_wms_raw_read_output_get_raw_message_data (output, &tag, &format, &data, NULL);
        process_read_sms_part (ctx->self, ctx->storage, message->memory_index, tag, format, data);
    }

    if (output)
        qmi_message_wms_raw_read_output_unref (output);

    /* Keep on reading parts */
    ctx->i++;
    read_next_sms_part (ctx);
}

static void
read_next_sms_part (MessagingListPartsContext *ctx)
{
    QmiMessageWmsListMessagesOutputMessageListElement *message;
    QmiMessageWmsRawReadInput *input;

    if (ctx->i >= ctx->message_array->len || !ctx->message_array) {
        ctx->step++;
        messaging_list_parts_context_step (ctx);
        return;
    }

    message = &g_array_index (ctx->message_array,
                              QmiMessageWmsListMessagesOutputMessageListElement,
                              ctx->i);

    input = qmi_message_wms_raw_read_input_new ();
    qmi_message_wms_raw_read_input_set_message_memory_storage_id (
        input,
        ctx->storage,
        message->memory_index,
        NULL);

    /* set message mode */
    qmi_message_wms_raw_read_input_set_message_mode (input, QMI_WMS_MESSAGE_MODE_GSM_WCDMA, NULL);

    qmi_client_wms_raw_read (QMI_CLIENT_WMS (ctx->self->priv->wms),
                             input,
                             3,
                             NULL,
                             (GAsyncReadyCallback)wms_raw_read_ready,
                             ctx);
    qmi_message_wms_raw_read_input_unref (input);
}

static void
wms_list_messages_ready (QmiClientWms              *client,
                         GAsyncResult              *res,
                         MessagingListPartsContext *ctx)
{
    QmiMessageWmsListMessagesOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_wms_list_messages_finish (client, res, &error);
    if (!output || !qmi_message_wms_list_messages_output_get_result (output, &error)) {
        g_debug ("[messaging] couldn't list messages in storage '%s' (%s): %s",
                 qmi_wms_storage_type_get_string (ctx->storage),
                 qmi_wms_message_tag_type_get_string (ctx->tag),
                 error->message);
        g_error_free (error);

        /* Go on to next step */
        ctx->step++;
        messaging_list_parts_context_step (ctx);
    } else {
        GArray *message_array;

        qmi_message_wms_list_messages_output_get_message_list (output, &message_array, NULL);

        /* Keep a reference to the array ourselves */
        if (ctx->message_array)
            g_array_unref (ctx->message_array);
        ctx->message_array = g_array_ref (message_array);

        /* Start reading parts */
        ctx->i = 0;
        read_next_sms_part (ctx);
    }

    if (output)
        qmi_message_wms_list_messages_output_unref (output);
}

static void
messaging_list_parts_context_list (MessagingListPartsContext *ctx)
{
    QmiMessageWmsListMessagesInput *input;

    input = qmi_message_wms_list_messages_input_new ();
    qmi_message_wms_list_messages_input_set_storage_type (input, ctx->storage, NULL);
    qmi_message_wms_list_messages_input_set_message_mode (input, QMI_WMS_MESSAGE_MODE_GSM_WCDMA, NULL);
    qmi_message_wms_list_messages_input_set_message_tag (input, ctx->tag, NULL);

    qmi_client_wms_list_messages (QMI_CLIENT_WMS (ctx->self->priv->wms),
                                  input,
                                  5,
                                  NULL,
                                  (GAsyncReadyCallback) wms_list_messages_ready,
                                  ctx);
    qmi_message_wms_list_messages_input_unref (input);
}

static void
messaging_list_parts_context_step (MessagingListPartsContext *ctx)
{
    switch (ctx->step) {
    case MESSAGING_LIST_PARTS_CONTEXT_STEP_FIRST:
        /* Fall down to next step */
        g_debug ("[messaging] listing parts in storage '%s'...",
                 qmi_wms_storage_type_get_string (ctx->storage));
        ctx->step++;

    case MESSAGING_LIST_PARTS_CONTEXT_STEP_LIST_READ:
        ctx->tag = QMI_WMS_MESSAGE_TAG_TYPE_MT_READ;
        messaging_list_parts_context_list (ctx);
        return;

    case MESSAGING_LIST_PARTS_CONTEXT_STEP_LIST_NOT_READ:
        ctx->tag = QMI_WMS_MESSAGE_TAG_TYPE_MT_NOT_READ;
        messaging_list_parts_context_list (ctx);
        return;

    case MESSAGING_LIST_PARTS_CONTEXT_STEP_LAST:
        /* Done! */
        g_debug ("[messaging] listing parts in storage '%s' finished...",
                 qmi_wms_storage_type_get_string (ctx->storage));
        messaging_list_parts_context_free (ctx);
        return;
    }
}

static void
messaging_list_parts (RmfdPortProcessorQmi *self,
                      QmiWmsStorageType     storage)
{
    MessagingListPartsContext *ctx;

    ctx = g_slice_new0 (MessagingListPartsContext);
    ctx->self = g_object_ref (self);
    ctx->storage = storage;
    ctx->step = MESSAGING_LIST_PARTS_CONTEXT_STEP_FIRST;

    messaging_list_parts_context_step (ctx);
}

static void
messaging_list (RmfdPortProcessorQmi *self)
{
    messaging_list_parts (self, QMI_WMS_STORAGE_TYPE_UIM);
    messaging_list_parts (self, QMI_WMS_STORAGE_TYPE_NV);
}

/*****************************************************************************/
/* Messaging setup and init */

typedef enum {
    MESSAGING_INIT_CONTEXT_STEP_FIRST,
    MESSAGING_INIT_CONTEXT_STEP_ROUTES,
    MESSAGING_INIT_CONTEXT_STEP_EVENT_REPORT,
    MESSAGING_INIT_CONTEXT_STEP_LAST
} MessagingInitContextStep;

typedef struct {
    RmfdPortProcessorQmi *self;
    GSimpleAsyncResult *result;
    MessagingInitContextStep step;
} MessagingInitContext;

static void
messaging_init_context_complete_and_free (MessagingInitContext *ctx)
{
    g_simple_async_result_complete (ctx->result);
    g_object_unref (ctx->result);
    g_object_unref (ctx->self);
    g_slice_free (MessagingInitContext, ctx);
}

static gboolean
messaging_init_finish (RmfdPortProcessorQmi  *self,
                       GAsyncResult          *res,
                       GError               **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

static void messaging_init_context_step (MessagingInitContext *ctx);

static void
ser_messaging_indicator_ready (QmiClientWms         *client,
                               GAsyncResult         *res,
                               MessagingInitContext *ctx)
{
    QmiMessageWmsSetEventReportOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_wms_set_event_report_finish (client, res, &error);
    if (!output || !qmi_message_wms_set_event_report_output_get_result (output, &error)) {
        g_simple_async_result_take_error (ctx->result, error);
        messaging_init_context_complete_and_free (ctx);
    } else {
        /* Go on */
        ctx->step++;
        messaging_init_context_step (ctx);
    }

    if (output)
        qmi_message_wms_set_event_report_output_unref (output);
}

static void
wms_set_routes_ready (QmiClientWms         *client,
                      GAsyncResult         *res,
                      MessagingInitContext *ctx)
{
    QmiMessageWmsSetRoutesOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_wms_set_routes_finish (client, res, &error);
    if (!output || !qmi_message_wms_set_routes_output_get_result (output, &error)) {
        g_simple_async_result_take_error (ctx->result, error);
        messaging_init_context_complete_and_free (ctx);
    } else {
        /* Go on */
        ctx->step++;
        messaging_init_context_step (ctx);
    }

    if (output)
        qmi_message_wms_set_routes_output_unref (output);
}

static void
messaging_init_context_step (MessagingInitContext *ctx)
{
    switch (ctx->step) {
    case MESSAGING_INIT_CONTEXT_STEP_FIRST:
        /* Fall down to next step */
        g_debug ("[messaging] initializing...");
        ctx->step++;

    case MESSAGING_INIT_CONTEXT_STEP_ROUTES: {
        QmiMessageWmsSetRoutesInputRouteListElement route;
        QmiMessageWmsSetRoutesInput *input;
        GArray *routes_array;

        /* Build routes array and add it as input
         * Just worry about Class 0 and Class 1 messages for now */
        input = qmi_message_wms_set_routes_input_new ();
        routes_array = g_array_sized_new (FALSE, FALSE, sizeof (route), 2);
        route.message_type = QMI_WMS_MESSAGE_TYPE_POINT_TO_POINT;
        route.message_class = QMI_WMS_MESSAGE_CLASS_0;
        route.storage = QMI_WMS_STORAGE_TYPE_NV; /* Store in modem, not UIM */
        route.receipt_action = QMI_WMS_RECEIPT_ACTION_STORE_AND_NOTIFY;
        g_array_append_val (routes_array, route);
        route.message_class = QMI_WMS_MESSAGE_CLASS_1;
        g_array_append_val (routes_array, route);
        qmi_message_wms_set_routes_input_set_route_list (input, routes_array, NULL);

        g_debug ("[messaging] setting default routes...");
        qmi_client_wms_set_routes (QMI_CLIENT_WMS (ctx->self->priv->wms),
                                   input,
                                   5,
                                   NULL,
                                   (GAsyncReadyCallback)wms_set_routes_ready,
                                   ctx);
        qmi_message_wms_set_routes_input_unref (input);
        g_array_unref (routes_array);
        return;
    }

    case MESSAGING_INIT_CONTEXT_STEP_EVENT_REPORT: {
        QmiMessageWmsSetEventReportInput *input;

        g_assert (ctx->self->priv->messaging_event_report_indication_id == 0);

        ctx->self->priv->messaging_event_report_indication_id =
            g_signal_connect (ctx->self->priv->wms,
                              "event-report",
                              G_CALLBACK (messaging_event_report_indication_cb),
                              ctx->self);

        input = qmi_message_wms_set_event_report_input_new ();
        qmi_message_wms_set_event_report_input_set_new_mt_message_indicator (input, TRUE, NULL);
        qmi_client_wms_set_event_report (QMI_CLIENT_WMS (ctx->self->priv->wms),
                                         input,
                                         5,
                                         NULL,
                                         (GAsyncReadyCallback)ser_messaging_indicator_ready,
                                         ctx);
        qmi_message_wms_set_event_report_input_unref (input);
        return;
    }

    case MESSAGING_INIT_CONTEXT_STEP_LAST:
        /* Done! */
        g_debug ("[messaging] setup finished...");
        g_simple_async_result_set_op_res_gboolean (ctx->result, TRUE);
        messaging_init_context_complete_and_free (ctx);
        return;
    }
}

static void
messaging_init (RmfdPortProcessorQmi *self,
                GAsyncReadyCallback   callback,
                gpointer              user_data)
{
    MessagingInitContext *ctx;

    g_assert (self->priv->wms != NULL);

    ctx = g_slice_new (MessagingInitContext);
    ctx->self = g_object_ref (self);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             messaging_init);
    ctx->step = MESSAGING_INIT_CONTEXT_STEP_FIRST;

    messaging_init_context_step (ctx);
}

/*****************************************************************************/
/* Processor init */

typedef struct {
    RmfdPortProcessorQmi *self;
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
messaging_init_ready (RmfdPortProcessorQmi *self,
                      GAsyncResult         *res,
                      InitContext          *ctx)
{
    GError *error = NULL;

    if (!messaging_init_finish (self, res, &error)) {
        g_simple_async_result_take_error (ctx->result, error);
        init_context_complete_and_free (ctx);
        return;
    }

    g_debug ("SMS messaging support initialized");

    /* Last step, launch automatic network registration explicitly */
    initiate_registration (ctx->self, TRUE);

    /* And launch SMS listing, which will succeed here only if PIN unlocked or disabled */
    messaging_list (ctx->self);

    g_simple_async_result_set_op_res_gboolean (ctx->result, TRUE);
    init_context_complete_and_free (ctx);
}

static void
allocate_wms_client_ready (QmiDevice    *qmi_device,
                           GAsyncResult *res,
                           InitContext  *ctx)
{
    GError *error = NULL;


    ctx->self->priv->wms = qmi_device_allocate_client_finish (qmi_device, res, &error);
    if (!ctx->self->priv->wms) {
        g_simple_async_result_take_error (ctx->result, error);
        init_context_complete_and_free (ctx);
        return;
    }

    g_debug ("QMI WMS client created");
    messaging_init (ctx->self,
                    (GAsyncReadyCallback) messaging_init_ready,
                    ctx);
}

static void
allocate_uim_client_ready (QmiDevice    *qmi_device,
                           GAsyncResult *res,
                           InitContext  *ctx)
{
    GError *error = NULL;

    ctx->self->priv->uim = qmi_device_allocate_client_finish (qmi_device, res, &error);
    if (!ctx->self->priv->uim) {
        g_simple_async_result_take_error (ctx->result, error);
        init_context_complete_and_free (ctx);
        return;
    }

    g_debug ("QMI UIM client created");
    qmi_device_allocate_client (ctx->self->priv->qmi_device,
                                QMI_SERVICE_WMS,
                                QMI_CID_NONE,
                                10,
                                NULL,
                                (GAsyncReadyCallback)allocate_wms_client_ready,
                                ctx);
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
    qmi_device_allocate_client (ctx->self->priv->qmi_device,
                                QMI_SERVICE_UIM,
                                QMI_CID_NONE,
                                10,
                                NULL,
                                (GAsyncReadyCallback)allocate_uim_client_ready,
                                ctx);
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
    register_nas_indications (ctx->self);

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
    GFile *file;

    ctx = g_slice_new (InitContext);
    ctx->self = g_object_ref (initable);
    ctx->result = g_simple_async_result_new (G_OBJECT (initable),
                                             callback,
                                             user_data,
                                             initable_init_async);
    /* Launch device creation */
    file = g_file_new_for_path (rmfd_port_get_interface (RMFD_PORT (ctx->self)));
    qmi_device_new (file,
                    NULL, /* cancellable */
                    (GAsyncReadyCallback) device_new_ready,
                    ctx);
    g_object_unref (file);
}

/*****************************************************************************/

RmfdPortProcessor *
rmfd_port_processor_qmi_new_finish (GAsyncResult  *res,
                                    GError       **error)
{
    GObject *source;
    GObject *self;

    source = g_async_result_get_source_object (res);
    self = g_async_initable_new_finish (G_ASYNC_INITABLE (source), res, error);
    g_object_unref (source);

    if (!self)
        return NULL;

    return RMFD_PORT_PROCESSOR (self);
}

void
rmfd_port_processor_qmi_new (const gchar         *interface,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
    g_async_initable_new_async (RMFD_TYPE_PORT_PROCESSOR_QMI,
                                G_PRIORITY_DEFAULT,
                                NULL,
                                callback,
                                user_data,
                                RMFD_PORT_INTERFACE, interface,
                                NULL);
}

/*****************************************************************************/

static void
rmfd_port_processor_qmi_init (RmfdPortProcessorQmi *self)
{
    /* Setup private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, RMFD_TYPE_PORT_PROCESSOR_QMI, RmfdPortProcessorQmiPrivate);
    self->priv->connection_status = RMF_CONNECTION_STATUS_DISCONNECTED;
    self->priv->registration_timeout = DEFAULT_REGISTRATION_TIMEOUT_SECS;
    self->priv->registration_status = RMF_REGISTRATION_STATUS_IDLE;

    /* Setup SMS list handler */
    self->priv->messaging_sms_list = rmfd_sms_list_new ();
    g_signal_connect (self->priv->messaging_sms_list, "sms-added", G_CALLBACK (sms_added_cb), self);
}

static void
dispose (GObject *object)
{
    RmfdPortProcessorQmi *self = RMFD_PORT_PROCESSOR_QMI (object);

    registration_context_cancel (self);
    unregister_nas_indications (self);
    unregister_wms_indications (self);

    if (self->priv->qmi_device && qmi_device_is_open (self->priv->qmi_device)) {
        GError *error = NULL;

        if (self->priv->dms)
            qmi_device_release_client (self->priv->qmi_device,
                                       self->priv->dms,
                                       QMI_DEVICE_RELEASE_CLIENT_FLAGS_RELEASE_CID,
                                       3, NULL, NULL, NULL);
        if (self->priv->nas)
            qmi_device_release_client (self->priv->qmi_device,
                                       self->priv->nas,
                                       QMI_DEVICE_RELEASE_CLIENT_FLAGS_RELEASE_CID,
                                       3, NULL, NULL, NULL);
        if (self->priv->wds)
            qmi_device_release_client (self->priv->qmi_device,
                                       self->priv->wds,
                                       QMI_DEVICE_RELEASE_CLIENT_FLAGS_RELEASE_CID,
                                       3, NULL, NULL, NULL);
        if (self->priv->uim)
            qmi_device_release_client (self->priv->qmi_device,
                                       self->priv->uim,
                                       QMI_DEVICE_RELEASE_CLIENT_FLAGS_RELEASE_CID,
                                       3, NULL, NULL, NULL);
        if (self->priv->wms)
            qmi_device_release_client (self->priv->qmi_device,
                                       self->priv->wms,
                                       QMI_DEVICE_RELEASE_CLIENT_FLAGS_RELEASE_CID,
                                       3, NULL, NULL, NULL);

        if (!qmi_device_close (self->priv->qmi_device, &error)) {
            g_warning ("error closing QMI device: %s", error->message);
            g_error_free (error);
        } else
            g_debug ("QmiDevice closed: %s", qmi_device_get_path (self->priv->qmi_device));
    }

    if (self->priv->messaging_sms_list)
        g_signal_handlers_disconnect_by_func (self->priv->messaging_sms_list, sms_added_cb, self);
    g_clear_object (&self->priv->messaging_sms_list);

    g_clear_object (&self->priv->dms);
    g_clear_object (&self->priv->nas);
    g_clear_object (&self->priv->wds);
    g_clear_object (&self->priv->uim);
    g_clear_object (&self->priv->wms);
    g_clear_object (&self->priv->qmi_device);

    G_OBJECT_CLASS (rmfd_port_processor_qmi_parent_class)->dispose (object);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
    iface->init_async = initable_init_async;
    iface->init_finish = initable_init_finish;
}

static void
rmfd_port_processor_qmi_class_init (RmfdPortProcessorQmiClass *processor_qmi_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (processor_qmi_class);
    RmfdPortProcessorClass *processor_class = RMFD_PORT_PROCESSOR_CLASS (processor_qmi_class);

    g_type_class_add_private (object_class, sizeof (RmfdPortProcessorQmiPrivate));

    /* Virtual methods */
    object_class->dispose = dispose;
    processor_class->run = run;
    processor_class->run_finish = run_finish;
}

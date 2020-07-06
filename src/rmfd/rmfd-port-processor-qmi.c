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
 * Copyright (C) 2013-2016 Safran Passenger Innovations
 *
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <libqmi-glib.h>

#include <rmf-messages.h>

#include "rmfd-syslog.h"
#include "rmfd-utils.h"
#include "rmfd-stats.h"
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

#define MESSAGING_LIST_MAX_RETRIES        3
#define MESSAGING_LIST_RETRY_TIMEOUT_SECS 5

#define DEFAULT_STATS_TIMEOUT_SECS 10

#define MAX_CONNECT_ITERATIONS 3

#define STATS_FILE_PATH "/var/log/rmfd.stats"

struct _RmfdPortProcessorQmiPrivate {
    /* QMI device and clients */
    QmiDevice *qmi_device;
    GList     *services; /* ServiceInfo */

    /* Connection related info */
    RmfConnectionStatus connection_status;
    guint32 packet_data_handle;
    guint stats_timeout_id;
    gboolean stats_enabled;

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
    GArray *messaging_sms_contexts;

    /* Stats */
    RmfdStatsContext *stats;

    /* WWAN settings */
    gboolean llp_is_raw_ip;
};

static void initiate_registration (RmfdPortProcessorQmi *self, gboolean with_timeout);
static void messaging_list        (RmfdPortProcessorQmi *self);

/*****************************************************************************/
/* QMI services */

typedef struct {
    QmiService service;
    gboolean   mandatory;
} QmiServiceItem;

static const QmiServiceItem service_items[] = {
    { QMI_SERVICE_DMS, TRUE },
    { QMI_SERVICE_NAS, TRUE },
    { QMI_SERVICE_WDS, TRUE },
    { QMI_SERVICE_UIM, TRUE },
    { QMI_SERVICE_WMS, TRUE },
};

typedef struct {
    QmiService service;
    QmiClient *client;
} ServiceInfo;

static ServiceInfo *
find_qmi_service (RmfdPortProcessorQmi *self,
                  QmiService            service)
{
    GList *l;

    for (l = self->priv->services; l; l = g_list_next (l)) {
        ServiceInfo *info = l->data;

        if (info->service == service)
            return info;
    }

    return NULL;
}

static void
untrack_qmi_service (RmfdPortProcessorQmi *self,
                     QmiService            service)
{
    ServiceInfo *info;

    info = find_qmi_service (self, service);
    if (!info)
        return;

    /* Remove from services list */
    self->priv->services = g_list_remove (self->priv->services, info);

    /* Cleanup client */
    if (info->client) {
        /* If device open, release client id */
        if (self->priv->qmi_device && qmi_device_is_open (self->priv->qmi_device))
            qmi_device_release_client (self->priv->qmi_device,
                                       info->client,
                                       QMI_DEVICE_RELEASE_CLIENT_FLAGS_RELEASE_CID,
                                       3, NULL, NULL, NULL);
        g_object_unref (info->client);
    }
    g_slice_free (ServiceInfo, info);
}

static void
track_qmi_service (RmfdPortProcessorQmi *self,
                   QmiService            service,
                   QmiClient            *client)
{
    ServiceInfo *info;

    info = g_slice_new0 (ServiceInfo);
    info->service = service;
    info->client  = g_object_ref (client);

    self->priv->services = g_list_prepend (self->priv->services, info);
}

static QmiClient *
peek_qmi_client (RmfdPortProcessorQmi *self,
                 QmiService            service)
{
    GList *l;

    for (l = self->priv->services; l; l = g_list_next (l)) {
        ServiceInfo *info = l->data;

        if (info->service == service)
            return info->client;
    }

    return NULL;
}

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
    qmi_client_nas_network_scan (QMI_CLIENT_NAS (peek_qmi_client (self, QMI_SERVICE_NAS)),
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
        QMI_CLIENT_NAS (peek_qmi_client (self, QMI_SERVICE_NAS)),
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
    QmiClientNas                          *nas;

    if (self->priv->serving_system_indication_id == 0)
        return;

    nas = QMI_CLIENT_NAS (peek_qmi_client (self, QMI_SERVICE_NAS));
    g_assert (QMI_IS_CLIENT_NAS (nas));

    g_signal_handler_disconnect (nas, self->priv->serving_system_indication_id);
    self->priv->serving_system_indication_id = 0;

    input = qmi_message_nas_register_indications_input_new ();
    qmi_message_nas_register_indications_input_set_serving_system_events (input, FALSE, NULL);
    qmi_client_nas_register_indications (nas, input, 5, NULL, NULL, NULL);
    qmi_message_nas_register_indications_input_unref (input);
}

static void
register_nas_indications (RmfdPortProcessorQmi *self)
{
    QmiMessageNasRegisterIndicationsInput *input;
    QmiClientNas                          *nas;

    g_assert (self->priv->serving_system_indication_id == 0);

    nas = QMI_CLIENT_NAS (peek_qmi_client (self, QMI_SERVICE_NAS));
    g_assert (QMI_IS_CLIENT_NAS (nas));

    self->priv->serving_system_indication_id =
        g_signal_connect (nas,
                          "serving-system",
                          G_CALLBACK (serving_system_indication_cb),
                          self);

    input = qmi_message_nas_register_indications_input_new ();
    qmi_message_nas_register_indications_input_set_serving_system_events (input, TRUE, NULL);
    qmi_client_nas_register_indications (nas, input, 5, NULL, NULL, NULL);
    qmi_message_nas_register_indications_input_unref (input);

    qmi_client_nas_get_serving_system (nas,
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
    qmi_client_dms_get_manufacturer (QMI_CLIENT_DMS (peek_qmi_client (ctx->self, QMI_SERVICE_DMS)),
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
    qmi_client_dms_get_model (QMI_CLIENT_DMS (peek_qmi_client (ctx->self, QMI_SERVICE_DMS)),
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
    qmi_client_dms_get_revision (QMI_CLIENT_DMS (peek_qmi_client (ctx->self, QMI_SERVICE_DMS)),
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
    qmi_client_dms_get_hardware_revision (QMI_CLIENT_DMS (peek_qmi_client (ctx->self, QMI_SERVICE_DMS)),
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
    qmi_client_dms_get_ids (QMI_CLIENT_DMS (peek_qmi_client (ctx->self, QMI_SERVICE_DMS)),
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
    qmi_client_dms_uim_get_imsi (QMI_CLIENT_DMS (peek_qmi_client (ctx->self, QMI_SERVICE_DMS)),
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
    qmi_client_dms_uim_get_iccid (QMI_CLIENT_DMS (peek_qmi_client (ctx->self, QMI_SERVICE_DMS)),
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
            parse_plmns (get_sim_info_ctx, (const guint8 *) read_result->data, read_result->len);
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
        get_sim_info_ctx->step++;
        /* fall through */

    case GET_SIM_INFO_STEP_IMSI:
        qmi_client_dms_uim_get_imsi (QMI_CLIENT_DMS (peek_qmi_client (ctx->self, QMI_SERVICE_DMS)),
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

        qmi_client_uim_read_transparent (QMI_CLIENT_UIM (peek_qmi_client (ctx->self, QMI_SERVICE_UIM)),
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

        qmi_client_uim_read_transparent (QMI_CLIENT_UIM (peek_qmi_client (ctx->self, QMI_SERVICE_UIM)),
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

typedef struct {
    RmfdPortProcessorQmi *self;
    GSimpleAsyncResult *simple;
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

static void
get_card_status_ready (QmiClientUim *client,
                       GAsyncResult *res,
                       CommonUnlockCheckContext *ctx)
{
    QmiMessageUimGetCardStatusOutputCardStatusCardsElement                    *card;
    QmiMessageUimGetCardStatusOutputCardStatusCardsElementApplicationsElement *app;
    QmiMessageUimGetCardStatusOutput *output;
    GError   *error = NULL;
    GArray   *cards;
    gint      card_i = -1;
    gint      application_j = -1;
    guint     n_absent = 0;
    guint     n_error = 0;
    guint     n_invalid = 0;
    guint     i;
    gboolean  unlocked = FALSE;

    output = qmi_client_uim_get_card_status_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        goto out;
    }

    if (!qmi_message_uim_get_card_status_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't get card status: ");
        goto out;
    }

    if (!qmi_message_uim_get_card_status_output_get_card_status (
            output,
            NULL, /* index_gw_primary */
            NULL, /* index_1x_primary */
            NULL, /* index_gw_secondary */
            NULL, /* index_1x_secondary */
            &cards,
            &error))
        goto out;

    if (cards->len == 0) {
        error = g_error_new (RMFD_ERROR, RMFD_ERROR_UNKNOWN, "No cards reported");
        goto out;
    }

    if (cards->len > 1)
        g_debug ("Multiple cards reported: %u", cards->len);

    /* All KNOWN applications in all cards will need to be in READY state for us
     * to consider UNLOCKED */
    for (i = 0; i < cards->len; i++) {
        card = &g_array_index (cards, QmiMessageUimGetCardStatusOutputCardStatusCardsElement, i);
        switch (card->card_state) {
        case QMI_UIM_CARD_STATE_PRESENT: {
            guint j;
            gboolean sim_usim_found = FALSE;

            if (card->applications->len == 0) {
                g_debug ("[card %u] no applications in card", i);
                n_invalid++;
                break;
            }

            if (card->applications->len > 1)
                g_debug ("[card %u] multiple applications in card: %u", i, card->applications->len);

            for (j = 0; j < card->applications->len; j++) {
                app = &g_array_index (card->applications, QmiMessageUimGetCardStatusOutputCardStatusCardsElementApplicationsElement, j);
                if (app->type == QMI_UIM_CARD_APPLICATION_TYPE_UNKNOWN) {
                    g_debug ("[card %u, application %u] type: 'unknown', state '%s' (ignored)",
                             i, j, qmi_uim_card_application_state_get_string (app->state));
                    continue;
                }

                g_debug ("[card %u, application %u] type '%s', state '%s'",
                         i, j, qmi_uim_card_application_type_get_string (app->type), qmi_uim_card_application_state_get_string (app->state));

                if (app->type == QMI_UIM_CARD_APPLICATION_TYPE_SIM || app->type == QMI_UIM_CARD_APPLICATION_TYPE_USIM) {
                    /* We found the card/app pair to use! Only keep the first found,
                     * but still, keep on looping to log about the remaining ones */
                    if (card_i < 0 && application_j < 0) {
                        card_i = i;
                        application_j = j;
                    }

                    sim_usim_found = TRUE;
                }
            }

            if (!sim_usim_found) {
                g_debug ("[card %u] no SIM/USIM application found in card", i);
                n_invalid++;
            }

            break;
        }

        case QMI_UIM_CARD_STATE_ABSENT:
            g_debug ("[card %u] card is absent", i);
            n_absent++;
            break;

        case QMI_UIM_CARD_STATE_ERROR:
        default:
            n_error++;
            if (qmi_uim_card_error_get_string (card->error_code) != NULL)
                g_warning ("[card %u] card is unusable: %s",
                           i, qmi_uim_card_error_get_string (card->error_code));
            else
                g_warning ("[card %u] is unusable: unknown error (%u)",
                           i, card->error_code);
            break;
        }

        /* go on to next card */
    }

    /* If we found no card/app to use, we need to report an error */
    if (card_i < 0 || application_j < 0) {
        /* If not a single card found, report SIM not inserted */
        if (n_absent > 0 && !n_error && !n_invalid)
            error = g_error_new (RMFD_ERROR, RMFD_ERROR_INVALID_STATE,
                                 "No card found");
        else
            error = g_error_new (RMFD_ERROR, RMFD_ERROR_INVALID_STATE,
                                 "Card failure: %u absent, %u errors, %u invalid",
                                 n_absent, n_error, n_invalid);
        goto out;
    }

    /* Get card/app to use */
    card = &g_array_index (cards, QmiMessageUimGetCardStatusOutputCardStatusCardsElement, card_i);
    app  = &g_array_index (card->applications, QmiMessageUimGetCardStatusOutputCardStatusCardsElementApplicationsElement, application_j);

    /* If card not ready yet, return error.
     * If the application state reports needing PIN/PUK, consider that ready as
     * well, and let the logic fall down to check PIN1/PIN2. */
    if (app->state != QMI_UIM_CARD_APPLICATION_STATE_READY &&
        app->state != QMI_UIM_CARD_APPLICATION_STATE_PIN1_OR_UPIN_PIN_REQUIRED &&
        app->state != QMI_UIM_CARD_APPLICATION_STATE_PUK1_OR_UPIN_PUK_REQUIRED &&
        app->state != QMI_UIM_CARD_APPLICATION_STATE_PIN1_BLOCKED) {
        error = g_error_new (RMFD_ERROR, RMFD_ERROR_UNKNOWN, "UIM not ready");
        goto out;
    }

    /* Card is ready, what's the lock status? */

    /* PIN1 */
    switch (app->pin1_state) {
    case QMI_UIM_PIN_STATE_PERMANENTLY_BLOCKED:
        error = g_error_new (RMFD_ERROR, RMFD_ERROR_INVALID_STATE,
                             "UIM permanently blocked");
        break;

    case QMI_UIM_PIN_STATE_ENABLED_NOT_VERIFIED:
        g_message ("UIM is PIN locked: %u retries left", app->pin1_retries);
        break;

    case QMI_UIM_PIN_STATE_BLOCKED:
        error = g_error_new (RMFD_ERROR, RMFD_ERROR_INVALID_STATE,
                             "UIM is PUK locked: %u retries left", app->puk1_retries);
        break;

    case QMI_UIM_PIN_STATE_DISABLED:
    case QMI_UIM_PIN_STATE_ENABLED_VERIFIED:
        g_message ("UIM is ready");
        unlocked = TRUE;
        break;

    default:
        error = g_error_new (RMFD_ERROR, RMFD_ERROR_INVALID_STATE,
                             "Unknown UIM PIN/PUK status");
        break;
    }

out:
    if (error)
        g_simple_async_result_take_error (ctx->simple, error);
    else
        g_simple_async_result_set_op_res_gboolean (ctx->simple, unlocked);
    if (output)
        qmi_message_uim_get_card_status_output_unref (output);
    common_unlock_check_context_complete_and_free (ctx);
}

static void
common_unlock_check (RmfdPortProcessorQmi *self,
                     GAsyncReadyCallback   callback,
                     gpointer              user_data)
{
    CommonUnlockCheckContext *ctx;

    ctx = g_slice_new0 (CommonUnlockCheckContext);
    ctx->self = g_object_ref (self);
    ctx->simple = g_simple_async_result_new (G_OBJECT (self), callback, user_data, common_unlock_check);
    qmi_client_uim_get_card_status (QMI_CLIENT_UIM (peek_qmi_client (ctx->self, QMI_SERVICE_UIM)),
                                    NULL,
                                    5,
                                    NULL,
                                    (GAsyncReadyCallback) get_card_status_ready,
                                    ctx);
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
uim_verify_pin_ready (QmiClientUim *client,
                      GAsyncResult *res,
                      RunContext   *ctx)
{
    QmiMessageUimVerifyPinOutput *output = NULL;
    GError *error = NULL;
    UnlockPinContext *unlock_ctx;

    output = qmi_client_uim_verify_pin_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
        run_context_complete_and_free (ctx);
        return;
    }

    if (!qmi_message_uim_verify_pin_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't verify PIN: ");
        g_simple_async_result_take_error (ctx->result, error);
        qmi_message_uim_verify_pin_output_unref (output);
        run_context_complete_and_free (ctx);
        return;
    }

    qmi_message_uim_verify_pin_output_unref (output);
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
    QmiMessageUimVerifyPinInput *input;
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

    input = qmi_message_uim_verify_pin_input_new ();
    qmi_message_uim_verify_pin_input_set_info (
        input, QMI_UIM_PIN_ID_PIN1, pin, NULL);
    qmi_message_uim_verify_pin_input_set_session_information (
        input, QMI_UIM_SESSION_TYPE_CARD_SLOT_1, "", NULL);
    qmi_client_uim_verify_pin (QMI_CLIENT_UIM (peek_qmi_client (ctx->self, QMI_SERVICE_UIM)),
                               input,
                               5,
                               NULL,
                               (GAsyncReadyCallback) uim_verify_pin_ready,
                               ctx);
    qmi_message_uim_verify_pin_input_unref (input);
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
uim_set_pin_protection_ready (QmiClientUim *client,
                              GAsyncResult *res,
                              RunContext *ctx)
{
    QmiMessageUimSetPinProtectionOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_uim_set_pin_protection_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_uim_set_pin_protection_output_get_result (output, &error)) {
        /* QMI error internal when checking PIN status likely means NO SIM */
        if (g_error_matches (error, QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INTERNAL)) {
            g_error_free (error);
            error = g_error_new (QMI_PROTOCOL_ERROR,
                                 QMI_PROTOCOL_ERROR_NO_SIM,
                                 "missing SIM");
        }
        /* 'No effect' error means we're already either enabled or disabled */
        else if (g_error_matches (error, QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NO_EFFECT))
            g_clear_error (&error);

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
        qmi_message_uim_set_pin_protection_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
enable_pin (RunContext *ctx)
{
    QmiMessageUimSetPinProtectionInput *input;
    guint32 enable;
    const gchar *pin;

    rmf_message_enable_pin_request_parse (ctx->request->data, &enable, &pin);

    input = qmi_message_uim_set_pin_protection_input_new ();
    qmi_message_uim_set_pin_protection_input_set_info (
        input,
        QMI_UIM_PIN_ID_PIN1,
        !!enable,
        pin,
        NULL);
    qmi_message_uim_set_pin_protection_input_set_session_information (
        input,
        QMI_UIM_SESSION_TYPE_CARD_SLOT_1,
        "", /* ignored */
        NULL);
    qmi_client_uim_set_pin_protection (QMI_CLIENT_UIM (peek_qmi_client (ctx->self, QMI_SERVICE_UIM)),
                                       input,
                                       5,
                                       NULL,
                                       (GAsyncReadyCallback)uim_set_pin_protection_ready,
                                       ctx);
    qmi_message_uim_set_pin_protection_input_unref (input);
}

/**********************/
/* Change PIN */

static void
uim_change_pin_ready (QmiClientUim *client,
                      GAsyncResult *res,
                      RunContext *ctx)
{
    QmiMessageUimChangePinOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_uim_change_pin_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
    } else if (!qmi_message_uim_change_pin_output_get_result (output, &error)) {
        /* QMI error internal when checking PIN status likely means NO SIM */
        if (g_error_matches (error, QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_INTERNAL)) {
            g_error_free (error);
            error = g_error_new (QMI_PROTOCOL_ERROR,
                                 QMI_PROTOCOL_ERROR_NO_SIM,
                                 "missing SIM");
        }
        /* 'No effect' error means we already have that PIN */
        else if (g_error_matches (error, QMI_PROTOCOL_ERROR, QMI_PROTOCOL_ERROR_NO_EFFECT))
            g_clear_error (&error);

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
        qmi_message_uim_change_pin_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
change_pin (RunContext *ctx)
{
    QmiMessageUimChangePinInput *input;
    const gchar *old_pin;
    const gchar *new_pin;

    rmf_message_change_pin_request_parse (ctx->request->data, &old_pin, &new_pin);

    input = qmi_message_uim_change_pin_input_new ();
    qmi_message_uim_change_pin_input_set_info (
        input,
        QMI_UIM_PIN_ID_PIN1,
        old_pin,
        new_pin,
        NULL);
    qmi_message_uim_change_pin_input_set_session_information (
        input,
        QMI_UIM_SESSION_TYPE_CARD_SLOT_1,
        "", /* ignored */
        NULL);
    qmi_client_uim_change_pin (QMI_CLIENT_UIM (peek_qmi_client (ctx->self, QMI_SERVICE_UIM)),
                               input,
                               5,
                               NULL,
                               (GAsyncReadyCallback)uim_change_pin_ready,
                               ctx);
    qmi_message_uim_change_pin_input_unref (input);
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
    qmi_client_dms_get_operating_mode (QMI_CLIENT_DMS (peek_qmi_client (ctx->self, QMI_SERVICE_DMS)),
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
        if (power_ctx->mode == QMI_DMS_OPERATING_MODE_ONLINE)
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
    qmi_client_dms_set_operating_mode (QMI_CLIENT_DMS (peek_qmi_client (ctx->self, QMI_SERVICE_DMS)),
                                       input,
                                       20,
                                       NULL,
                                       (GAsyncReadyCallback)dms_set_operating_mode_ready,
                                       ctx);
    qmi_message_dms_set_operating_mode_input_unref (input);
}

/**********************/
/* Power cycle */

static void
dms_power_cycle_set_operating_mode_reset_ready (QmiClientDms *client,
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

        response = rmf_message_power_cycle_response_new ();
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
    }

    if (output)
        qmi_message_dms_set_operating_mode_output_unref (output);

    run_context_complete_and_free (ctx);
}

static void
dms_power_cycle_set_operating_mode_offline_ready (QmiClientDms *client,
                                                  GAsyncResult *res,
                                                  RunContext   *ctx)
{
    QmiMessageDmsSetOperatingModeOutput *output = NULL;
    QmiMessageDmsSetOperatingModeInput  *input;
    GError *error = NULL;

    output = qmi_client_dms_set_operating_mode_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
        run_context_complete_and_free (ctx);
        return;
    }

    if (!qmi_message_dms_set_operating_mode_output_get_result (output, &error)) {
        g_prefix_error (&error, "couldn't set operating mode: ");
        g_simple_async_result_take_error (ctx->result, error);
        qmi_message_dms_set_operating_mode_output_unref (output);
        run_context_complete_and_free (ctx);
        return;
    }

    qmi_message_dms_set_operating_mode_output_unref (output);

    /* Then, reset */
    input = qmi_message_dms_set_operating_mode_input_new ();
    qmi_message_dms_set_operating_mode_input_set_mode (input, QMI_DMS_OPERATING_MODE_RESET, NULL);
    qmi_client_dms_set_operating_mode (QMI_CLIENT_DMS (peek_qmi_client (ctx->self, QMI_SERVICE_DMS)),
                                       input,
                                       20,
                                       NULL,
                                       (GAsyncReadyCallback)dms_power_cycle_set_operating_mode_reset_ready,
                                       ctx);
    qmi_message_dms_set_operating_mode_input_unref (input);
}

static void
power_cycle (RunContext *ctx)
{
    QmiMessageDmsSetOperatingModeInput *input;

    /* First, offline */
    input = qmi_message_dms_set_operating_mode_input_new ();
    qmi_message_dms_set_operating_mode_input_set_mode (input, QMI_DMS_OPERATING_MODE_OFFLINE, NULL);
    qmi_client_dms_set_operating_mode (QMI_CLIENT_DMS (peek_qmi_client (ctx->self, QMI_SERVICE_DMS)),
                                       input,
                                       20,
                                       NULL,
                                       (GAsyncReadyCallback)dms_power_cycle_set_operating_mode_offline_ready,
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
        qmi_client_nas_get_tx_rx_info (QMI_CLIENT_NAS (peek_qmi_client (ctx->self, QMI_SERVICE_NAS)),
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
    qmi_client_nas_get_signal_info (QMI_CLIENT_NAS (peek_qmi_client (ctx->self, QMI_SERVICE_NAS)),
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
    qmi_client_wds_get_packet_statistics (QMI_CLIENT_WDS (peek_qmi_client (ctx->self, QMI_SERVICE_WDS)),
                                          input,
                                          10,
                                          NULL,
                                          (GAsyncReadyCallback)get_packet_statistics_ready,
                                          ctx);
    qmi_message_wds_get_packet_statistics_input_unref (input);
}

/***********************************************/
/* Ongoing connection stats and info gathering */

typedef enum {
    WRITE_CONNECTION_STATS_STEP_FIRST,
    WRITE_CONNECTION_STATS_STEP_TIMESTAMP,
    WRITE_CONNECTION_STATS_STEP_STATS,
    WRITE_CONNECTION_STATS_STEP_SIGNAL_STRENGTH,
    WRITE_CONNECTION_STATS_STEP_SERVING_SYSTEM,
    WRITE_CONNECTION_STATS_STEP_LAST
} WriteConnectionStatsStep;

typedef struct {
    RmfdPortProcessorQmi     *self;
    RmfdStatsRecordType       type;
    GSimpleAsyncResult       *result;
    WriteConnectionStatsStep  step;
    guint64                   rx_bytes;
    guint64                   tx_bytes;
    GDateTime                *system_time;
    gint8                     rssi;
    QmiNasRadioInterface      radio_interface;
    guint16                   mcc;
    guint16                   mnc;
    guint16                   lac;
    guint32                   cid;
} WriteConnectionStatsContext;

static void
write_connection_stats_context_complete_and_free (WriteConnectionStatsContext *ctx)
{
    g_simple_async_result_complete (ctx->result);
    g_object_unref (ctx->result);
    if (ctx->system_time)
        g_date_time_unref (ctx->system_time);
    g_object_unref (ctx->self);
    g_slice_free (WriteConnectionStatsContext, ctx);
}

static gboolean
write_connection_stats_finish (RmfdPortProcessorQmi  *self,
                               GAsyncResult          *res,
                               GError               **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

static void write_connection_stats_context_step (WriteConnectionStatsContext *ctx);

static void
get_serving_system_stats_ready (QmiClientNas                *client,
                                GAsyncResult                *res,
                                WriteConnectionStatsContext *ctx)
{
    QmiMessageNasGetServingSystemOutput *output;

    if ((output = qmi_client_nas_get_serving_system_finish (client, res, NULL)) &&
        qmi_message_nas_get_serving_system_output_get_result (output, NULL)) {
        /* MCC/MNC */
        qmi_message_nas_get_serving_system_output_get_current_plmn (output, &ctx->mcc, &ctx->mnc, NULL, NULL);
        /* LAC/CID */
        qmi_message_nas_get_serving_system_output_get_lac_3gpp (output, &ctx->lac, NULL);
        qmi_message_nas_get_serving_system_output_get_cid_3gpp (output, &ctx->cid, NULL);
    }

    if (output)
        qmi_message_nas_get_serving_system_output_unref (output);

    /* Continue */
    ctx->step++;
    write_connection_stats_context_step (ctx);
}

static void
get_signal_strength_stats_ready (QmiClientNas                *client,
                                 GAsyncResult                *res,
                                 WriteConnectionStatsContext *ctx)
{
    QmiMessageNasGetSignalStrengthOutput *output;

    if ((output = qmi_client_nas_get_signal_strength_finish (client, res, NULL)) &&
        qmi_message_nas_get_signal_strength_output_get_result (output, NULL)) {
        qmi_message_nas_get_signal_strength_output_get_signal_strength (output, &ctx->rssi, &ctx->radio_interface, NULL);
    }

    if (output)
        qmi_message_nas_get_signal_strength_output_unref (output);

    /* Continue */
    ctx->step++;
    write_connection_stats_context_step (ctx);
}

static void
get_packet_statistics_stats_ready (QmiClientWds                *client,
                                   GAsyncResult                *res,
                                   WriteConnectionStatsContext *ctx)
{
    QmiMessageWdsGetPacketStatisticsOutput *output;
    GError *error = NULL;

    if (!(output = qmi_client_wds_get_packet_statistics_finish (client, res, &error))) {
        /* Loading packet statistics failed (e.g. timeout error); we'll fully
         * skip writing a record. */
        /* Complete and finish */
        g_simple_async_result_take_error (ctx->result, error);
        write_connection_stats_context_complete_and_free (ctx);
        return;
    }

    /* Get TX and RX bytes */
    if (ctx->type == RMFD_STATS_RECORD_TYPE_FINAL) {
        /* Note: don't check result, as we'll get an out-of-call error, so just read last-call TLVs */
        qmi_message_wds_get_packet_statistics_output_get_last_call_tx_bytes_ok (output, &ctx->tx_bytes, &error);
        if (error) {
            g_debug ("cannot get last call tx bytes: %s", error->message);
            g_clear_error (&error);
        }
        qmi_message_wds_get_packet_statistics_output_get_last_call_rx_bytes_ok (output, &ctx->rx_bytes, &error);
        if (error) {
            g_debug ("cannot get last call rx bytes: %s", error->message);
            g_clear_error (&error);
        }
    } else if (ctx->type == RMFD_STATS_RECORD_TYPE_PARTIAL) {
        if (qmi_message_wds_get_packet_statistics_output_get_result (output, NULL)) {
            qmi_message_wds_get_packet_statistics_output_get_tx_bytes_ok (output, &ctx->tx_bytes, NULL);
            qmi_message_wds_get_packet_statistics_output_get_rx_bytes_ok (output, &ctx->rx_bytes, NULL);
        }
    } else
        g_assert_not_reached ();

    qmi_message_wds_get_packet_statistics_output_unref (output);

    /* Continue */
    ctx->step++;
    write_connection_stats_context_step (ctx);
}

static void
dms_get_time_stats_ready (QmiClientDms                *client,
                          GAsyncResult                *res,
                          WriteConnectionStatsContext *ctx)
{
    QmiMessageDmsGetTimeOutput *output;
    guint64                     time_count;

    /* Ignore errors */
    if ((output = qmi_client_dms_get_time_finish (client, res, NULL)) &&
        qmi_message_dms_get_time_output_get_result (output, NULL) &&
        qmi_message_dms_get_time_output_get_system_time (
            output,
            &time_count,
            NULL)) {
        GTimeZone *tz;
        GDateTime *gpstime_epoch;

        /* January 6th 1980 */
        tz = g_time_zone_new_utc ();
        gpstime_epoch = g_date_time_new (tz, 1980, 1, 6, 0, 0, 0.0);
        ctx->system_time = g_date_time_add_seconds (gpstime_epoch, ((gdouble) time_count / 1000.0));
        g_date_time_unref (gpstime_epoch);
        g_time_zone_unref (tz);
    }

    if (output)
        qmi_message_dms_get_time_output_unref (output);

    /* Continue */
    ctx->step++;
    write_connection_stats_context_step (ctx);
}

static void
write_connection_stats_context_step (WriteConnectionStatsContext *ctx)
{
    switch (ctx->step) {
    case WRITE_CONNECTION_STATS_STEP_FIRST:
        ctx->step++;
        /* fall through */

    case WRITE_CONNECTION_STATS_STEP_TIMESTAMP:
        qmi_client_dms_get_time (QMI_CLIENT_DMS (peek_qmi_client (ctx->self, QMI_SERVICE_DMS)),
                                 NULL,
                                 5,
                                 NULL,
                                 (GAsyncReadyCallback)dms_get_time_stats_ready,
                                 ctx);
        return;

    case WRITE_CONNECTION_STATS_STEP_STATS:
        /* For the START record, just assume 0 bytes TX/RX */
        if (ctx->type != RMFD_STATS_RECORD_TYPE_START) {
            QmiMessageWdsGetPacketStatisticsInput *input;

            input = qmi_message_wds_get_packet_statistics_input_new ();
            qmi_message_wds_get_packet_statistics_input_set_mask (input,
                                                                  (QMI_WDS_PACKET_STATISTICS_MASK_FLAG_TX_BYTES_OK |
                                                                   QMI_WDS_PACKET_STATISTICS_MASK_FLAG_RX_BYTES_OK),
                                                                  NULL);
            qmi_client_wds_get_packet_statistics (QMI_CLIENT_WDS (peek_qmi_client (ctx->self, QMI_SERVICE_WDS)),
                                                  input,
                                                  5,
                                                  NULL,
                                                  (GAsyncReadyCallback)get_packet_statistics_stats_ready,
                                                  ctx);
            qmi_message_wds_get_packet_statistics_input_unref (input);
            return;
        }
        ctx->step++;
        /* fall through */

    case WRITE_CONNECTION_STATS_STEP_SIGNAL_STRENGTH:
        qmi_client_nas_get_signal_strength (QMI_CLIENT_NAS (peek_qmi_client (ctx->self, QMI_SERVICE_NAS)),
                                            NULL,
                                            5,
                                            NULL,
                                            (GAsyncReadyCallback)get_signal_strength_stats_ready,
                                            ctx);
        return;

    case WRITE_CONNECTION_STATS_STEP_SERVING_SYSTEM:
        qmi_client_nas_get_serving_system (QMI_CLIENT_NAS (peek_qmi_client (ctx->self, QMI_SERVICE_NAS)),
                                           NULL,
                                           5,
                                           NULL,
                                           (GAsyncReadyCallback)get_serving_system_stats_ready,
                                           ctx);
        return;

    case WRITE_CONNECTION_STATS_STEP_LAST:
        /* Issue stats record */
        rmfd_stats_record (ctx->self->priv->stats,
                           ctx->type,
                           ctx->system_time,
                           ctx->rx_bytes,
                           ctx->tx_bytes,
                           qmi_nas_radio_interface_get_string (ctx->radio_interface),
                           ctx->rssi,
                           ctx->mcc,
                           ctx->mnc,
                           ctx->lac,
                           ctx->cid);

        /* Complete and finish */
        g_simple_async_result_set_op_res_gboolean (ctx->result, TRUE);
        write_connection_stats_context_complete_and_free (ctx);
        return;
    }

    g_assert_not_reached ();
}


static void
write_connection_stats (RmfdPortProcessorQmi *self,
                        RmfdStatsRecordType   type,
                        GAsyncReadyCallback   callback,
                        gpointer              user_data)
{
    WriteConnectionStatsContext *ctx;

    ctx = g_slice_new0 (WriteConnectionStatsContext);

    ctx->self            = g_object_ref (self);
    ctx->result          = g_simple_async_result_new (G_OBJECT (self), callback, user_data, write_connection_stats);
    ctx->type            = type;
    ctx->step            = WRITE_CONNECTION_STATS_STEP_FIRST;
    ctx->radio_interface = QMI_NAS_RADIO_INTERFACE_UNKNOWN;

    write_connection_stats_context_step (ctx);
}

/**********************/

static void schedule_stats (RmfdPortProcessorQmi *self);

static void
write_connection_stats_timeout_ready (RmfdPortProcessorQmi *self,
                                      GAsyncResult         *res)
{
    write_connection_stats_finish (self, res, NULL);
    schedule_stats (self);
}

static gboolean
stats_cb (RmfdPortProcessorQmi *self)
{
    self->priv->stats_timeout_id = 0;

    write_connection_stats (self,
                            RMFD_STATS_RECORD_TYPE_PARTIAL,
                            (GAsyncReadyCallback) write_connection_stats_timeout_ready,
                            NULL);

    return FALSE;
}

static void
schedule_stats (RmfdPortProcessorQmi *self)
{
    if (self->priv->stats_timeout_id) {
        g_source_remove (self->priv->stats_timeout_id);
        self->priv->stats_timeout_id = 0;
    }

    if (self->priv->stats_enabled)
        self->priv->stats_timeout_id = g_timeout_add_seconds (DEFAULT_STATS_TIMEOUT_SECS, (GSourceFunc) stats_cb, self);
}

/**********************/
/* Connect */

typedef enum {
    CONNECT_STEP_FIRST,
    CONNECT_STEP_IP_FAMILY,
    CONNECT_STEP_START_NETWORK,
    CONNECT_STEP_IP_SETTINGS,
    CONNECT_STEP_WWAN_SETUP,
    CONNECT_STEP_STATS,
    CONNECT_STEP_LAST,
} ConnectStep;

typedef struct {
    ConnectStep  step;
    guint        iteration;
    gboolean     default_ip_family_set;
    GError      *error;
    gchar       *ip_str;
    gchar       *subnet_str;
    gchar       *gw_str;
    gchar       *dns1_str;
    gchar       *dns2_str;
    guint32      mtu;
} ConnectContext;

static void
connect_context_free (ConnectContext *ctx)
{
    g_free (ctx->ip_str);
    g_free (ctx->subnet_str);
    g_free (ctx->gw_str);
    g_free (ctx->dns1_str);
    g_free (ctx->dns2_str);
    if (ctx->error)
        g_error_free (ctx->error);
    g_slice_free (ConnectContext, ctx);
}

static void connect_step (RunContext *ctx);

static gboolean
connect_step_scheduled (RunContext *ctx)
{
    connect_step (ctx);
    return G_SOURCE_REMOVE;
}

static void
connect_step_schedule (RunContext *ctx,
                       guint       n_seconds)
{
    g_timeout_add_seconds (n_seconds, (GSourceFunc) connect_step_scheduled, ctx);
}

static void
connect_step_restart_iteration (RunContext *ctx)
{
    ConnectContext *connect_ctx = (ConnectContext *)ctx->additional_context;

    g_assert (connect_ctx->error);
    connect_ctx->iteration++;

    if (connect_ctx->iteration > MAX_CONNECT_ITERATIONS) {
        GByteArray *error_message;

        ctx->self->priv->connection_status = RMF_CONNECTION_STATUS_DISCONNECTED;

        g_warning ("error: no more connection attempts left");

        error_message = rmfd_error_message_new_from_gerror (ctx->request, connect_ctx->error);
        g_simple_async_result_set_op_res_gpointer (ctx->result, error_message,
                                                   (GDestroyNotify)g_byte_array_unref);
        run_context_complete_and_free (ctx);
        return;
    }

    g_warning ("error: restarting connection iteration");

    /* From the very beginning */
    g_clear_error (&connect_ctx->error);
    connect_ctx->step = CONNECT_STEP_FIRST;
    connect_ctx->default_ip_family_set = FALSE;

    connect_step_schedule (ctx, 5);
}

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

    connect_step_restart_iteration (ctx);
}

static void
write_connection_stats_start_ready (RmfdPortProcessorQmi *self,
                                    GAsyncResult         *res,
                                    RunContext           *ctx)
{
    ConnectContext *connect_ctx = (ConnectContext *)ctx->additional_context;
    GError *error = NULL;

    if (!write_connection_stats_finish (self, res, &error)) {
        g_debug ("couldn't write initial connection stats: %s", error->message);
        g_clear_error (&error);
    }

    self->priv->stats_enabled = TRUE;
    schedule_stats (self);

    /* Go on to next step */
    connect_ctx->step++;
    connect_step (ctx);
}

static void
connect_step_stats (RunContext *ctx)
{
    /* Report start of stats */
    write_connection_stats (ctx->self,
                            RMFD_STATS_RECORD_TYPE_START,
                            (GAsyncReadyCallback) write_connection_stats_start_ready,
                            ctx);
}

static void
data_setup_start_ready (RmfdPortData *data,
                        GAsyncResult *res,
                        RunContext   *ctx)
{
    ConnectContext *connect_ctx = (ConnectContext *)ctx->additional_context;
    GError *error = NULL;

    if (!rmfd_port_data_setup_finish (data, res, &error)) {
        QmiMessageWdsStopNetworkInput *input;

        g_assert (!connect_ctx->error);
        connect_ctx->error = error;

        input = qmi_message_wds_stop_network_input_new ();
        qmi_message_wds_stop_network_input_set_packet_data_handle (input, ctx->self->priv->packet_data_handle, NULL);
        qmi_client_wds_stop_network (QMI_CLIENT_WDS (peek_qmi_client (ctx->self, QMI_SERVICE_WDS)),
                                     input,
                                     30,
                                     NULL,
                                     (GAsyncReadyCallback)wds_stop_network_after_start_ready,
                                     ctx);
        qmi_message_wds_stop_network_input_unref (input);
        return;
    }

    /* Go on to next step */
    connect_ctx->step++;
    connect_step (ctx);
}

static void
connect_step_wwan_setup (RunContext *ctx)
{
    ConnectContext *connect_ctx = (ConnectContext *)ctx->additional_context;

    /* If we have a device running in 802.3 mode, we must run DHCP because some
     * devices require that to actually initialize the data flow in the WWAN.
     */
    if (!ctx->self->priv->llp_is_raw_ip) {
        rmfd_port_data_setup (ctx->data,
                              TRUE,
                              NULL, NULL, NULL, NULL, NULL, 0,
                              (GAsyncReadyCallback)data_setup_start_ready,
                              ctx);
        return;
    }

    /* If we have a device running in raw-ip mode, run with static IP
     * configuration because not all DHCP clients know how to handle the raw-ip
     * network interface. But we do require IP and subnet at least!
     */
    if (connect_ctx->ip_str && connect_ctx->subnet_str) {
        rmfd_port_data_setup (ctx->data,
                              TRUE,
                              connect_ctx->ip_str,
                              connect_ctx->subnet_str,
                              connect_ctx->gw_str,
                              connect_ctx->dns1_str,
                              connect_ctx->dns2_str,
                              connect_ctx->mtu,
                              (GAsyncReadyCallback)data_setup_start_ready,
                              ctx);
        return;
    }


}

static void
get_current_settings_ready (QmiClientWds *client,
                            GAsyncResult *res,
                            RunContext   *ctx)
{
    ConnectContext *connect_ctx = (ConnectContext *)ctx->additional_context;
    GError *error = NULL;
    QmiMessageWdsGetCurrentSettingsOutput *output;
    guint32 addr = 0;
    struct in_addr in_addr_val;
    gchar buf4[INET_ADDRSTRLEN];

    g_assert (!connect_ctx->ip_str);
    g_assert (!connect_ctx->subnet_str);
    g_assert (!connect_ctx->gw_str);
    g_assert (!connect_ctx->dns1_str);
    g_assert (!connect_ctx->dns2_str);
    g_assert (!connect_ctx->mtu);

    output = qmi_client_wds_get_current_settings_finish (client, res, &error);
    if (output && qmi_message_wds_get_current_settings_output_get_result (output, &error)) {
        g_debug ("Current IP settings retrieved:");

        if (qmi_message_wds_get_current_settings_output_get_ipv4_address (output, &addr, NULL)) {
            in_addr_val.s_addr = GUINT32_TO_BE (addr);
            memset (buf4, 0, sizeof (buf4));
            inet_ntop (AF_INET, &in_addr_val, buf4, sizeof (buf4));
            g_debug ("  IPv4 address: %s", buf4);
            connect_ctx->ip_str = g_strdup (buf4);
        }

        if (qmi_message_wds_get_current_settings_output_get_ipv4_gateway_subnet_mask (output, &addr, NULL)) {
            in_addr_val.s_addr = GUINT32_TO_BE (addr);
            memset (buf4, 0, sizeof (buf4));
            inet_ntop (AF_INET, &in_addr_val, buf4, sizeof (buf4));
            g_debug ("  IPv4 subnet mask: %s", buf4);
            connect_ctx->subnet_str = g_strdup (buf4);
        }

        if (qmi_message_wds_get_current_settings_output_get_ipv4_gateway_address (output, &addr, NULL)) {
            in_addr_val.s_addr = GUINT32_TO_BE (addr);
            memset (buf4, 0, sizeof (buf4));
            inet_ntop (AF_INET, &in_addr_val, buf4, sizeof (buf4));
            g_debug ("  IPv4 gateway address: %s", buf4);
            connect_ctx->gw_str = g_strdup (buf4);
        }

        if (qmi_message_wds_get_current_settings_output_get_primary_ipv4_dns_address (output, &addr, NULL)) {
            in_addr_val.s_addr = GUINT32_TO_BE (addr);
            memset (buf4, 0, sizeof (buf4));
            inet_ntop (AF_INET, &in_addr_val, buf4, sizeof (buf4));
            g_debug ("  IPv4 primary DNS: %s", buf4);
            connect_ctx->dns1_str = g_strdup (buf4);
        }

        if (qmi_message_wds_get_current_settings_output_get_secondary_ipv4_dns_address (output, &addr, NULL)) {
            in_addr_val.s_addr = GUINT32_TO_BE (addr);
            memset (buf4, 0, sizeof (buf4));
            inet_ntop (AF_INET, &in_addr_val, buf4, sizeof (buf4));
            g_debug ("  IPv4 secondary DNS: %s", buf4);
            connect_ctx->dns2_str = g_strdup (buf4);
        }

        if (qmi_message_wds_get_current_settings_output_get_mtu (output, &connect_ctx->mtu, NULL))
            g_debug ("  MTU: %u", connect_ctx->mtu);
    }

    if (output)
        qmi_message_wds_get_current_settings_output_unref (output);

    if (error) {
        connect_ctx->error = error;
        connect_step_restart_iteration (ctx);
        return;
    }

    /* Go on to next step */
    connect_ctx->step++;
    connect_step (ctx);
}

static void
connect_step_ip_settings (RunContext *ctx)
{
    QmiMessageWdsGetCurrentSettingsInput *input;
    ConnectContext *connect_ctx = (ConnectContext *)ctx->additional_context;

    input = qmi_message_wds_get_current_settings_input_new ();
    qmi_message_wds_get_current_settings_input_set_requested_settings (
        input,
        (QMI_WDS_GET_CURRENT_SETTINGS_REQUESTED_SETTINGS_IP_ADDRESS   |
         QMI_WDS_GET_CURRENT_SETTINGS_REQUESTED_SETTINGS_DNS_ADDRESS  |
         QMI_WDS_GET_CURRENT_SETTINGS_REQUESTED_SETTINGS_GATEWAY_INFO |
         QMI_WDS_GET_CURRENT_SETTINGS_REQUESTED_SETTINGS_MTU),
        NULL);

    g_free (connect_ctx->ip_str);
    g_free (connect_ctx->subnet_str);
    g_free (connect_ctx->gw_str);
    g_free (connect_ctx->dns1_str);
    g_free (connect_ctx->dns2_str);

    connect_ctx->ip_str = NULL;
    connect_ctx->subnet_str = NULL;
    connect_ctx->gw_str = NULL;
    connect_ctx->dns1_str = NULL;
    connect_ctx->dns2_str = NULL;
    connect_ctx->mtu = 0;

    g_debug ("Asynchronously getting current settings...");
    qmi_client_wds_get_current_settings (QMI_CLIENT_WDS (peek_qmi_client (ctx->self, QMI_SERVICE_WDS)),
                                         input,
                                         10,
                                         NULL,
                                         (GAsyncReadyCallback)get_current_settings_ready,
                                         ctx);
    qmi_message_wds_get_current_settings_input_unref (input);
}

static void
wds_start_network_ready (QmiClientWds *client,
                         GAsyncResult *res,
                         RunContext   *ctx)
{
    ConnectContext *connect_ctx = (ConnectContext *)ctx->additional_context;
    GError *error = NULL;
    QmiMessageWdsStartNetworkOutput *output;
    GString *error_str = NULL;

    g_assert (!connect_ctx->error);

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
            }
        }
    }

    if (output) {
        if (ctx->self->priv->packet_data_handle != GLOBAL_PACKET_DATA_HANDLE)
            qmi_message_wds_start_network_output_get_packet_data_handle (output, &ctx->self->priv->packet_data_handle, NULL);
        qmi_message_wds_start_network_output_unref (output);
    }

    if (error) {
        if (error_str) {
            connect_ctx->error = g_error_new (error->domain,
                                              error->code,
                                              "%s", error_str->len > 0 ? error_str->str : "unknown error");
            g_error_free (error);
            g_string_free (error_str, TRUE);
        } else
            connect_ctx->error = error;
        connect_step_restart_iteration (ctx);
        return;
    }

    /* Go on to next step */
    connect_ctx->step++;
    connect_step_schedule (ctx, 1);
}

static void
connect_step_start_network (RunContext *ctx)
{
    ConnectContext *connect_ctx = (ConnectContext *)ctx->additional_context;
    QmiMessageWdsStartNetworkInput *input;
    const gchar *apn;
    const gchar *user;
    const gchar *password;

    /* Setup start network command */
    rmf_message_connect_request_parse (ctx->request->data, &apn, &user, &password);

    input = qmi_message_wds_start_network_input_new ();
    if (apn && apn[0])
        qmi_message_wds_start_network_input_set_apn (input, apn, NULL);

    if ((user && user[0]) || (password && password[0])) {
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
    if (!connect_ctx->default_ip_family_set)
        qmi_message_wds_start_network_input_set_ip_family_preference (input, QMI_WDS_IP_FAMILY_IPV4, NULL);

    qmi_client_wds_start_network (QMI_CLIENT_WDS (peek_qmi_client (ctx->self, QMI_SERVICE_WDS)),
                                  input,
                                  45,
                                  NULL,
                                  (GAsyncReadyCallback)wds_start_network_ready,
                                  ctx);
    qmi_message_wds_start_network_input_unref (input);
}

static void
wds_set_ip_family_ready (QmiClientWds *client,
                         GAsyncResult *res,
                         RunContext   *ctx)
{
    ConnectContext *connect_ctx = (ConnectContext *)ctx->additional_context;
    QmiMessageWdsSetIpFamilyOutput *output;

    /* If there is an error setting default IP family, explicitly add it when
     * starting network */
    output = qmi_client_wds_set_ip_family_finish (client, res, NULL);
    if (output && qmi_message_wds_set_ip_family_output_get_result (output, NULL))
        connect_ctx->default_ip_family_set = TRUE;

    if (output)
        qmi_message_wds_set_ip_family_output_unref (output);

    /* Go on to next step */
    connect_ctx->step++;
    connect_step (ctx);
}

static void
connect_step_ip_family (RunContext *ctx)
{
    QmiMessageWdsSetIpFamilyInput *input;

    /* Start by setting IPv4 family */
    input = qmi_message_wds_set_ip_family_input_new ();
    qmi_message_wds_set_ip_family_input_set_preference (input, QMI_WDS_IP_FAMILY_IPV4, NULL);
    qmi_client_wds_set_ip_family (QMI_CLIENT_WDS (peek_qmi_client (ctx->self, QMI_SERVICE_WDS)),
                                  input,
                                  10,
                                  NULL,
                                  (GAsyncReadyCallback)wds_set_ip_family_ready,
                                  ctx);
    qmi_message_wds_set_ip_family_input_unref (input);
}

static void
connect_step (RunContext *ctx)
{
    ConnectContext *connect_ctx = (ConnectContext *)ctx->additional_context;
    guint8 *response;

    switch (connect_ctx->step) {
    case CONNECT_STEP_FIRST:
        g_warning ("connection: new connection attempt (%u/%u)...", connect_ctx->iteration, MAX_CONNECT_ITERATIONS);
        connect_ctx->step++;
        /* fall through */

    case CONNECT_STEP_IP_FAMILY:
        g_message ("connection %u/%u step %u/%u: setting IPv4 family...",
                   connect_ctx->iteration, MAX_CONNECT_ITERATIONS, connect_ctx->step, CONNECT_STEP_LAST);
        connect_step_ip_family (ctx);
        return;

    case CONNECT_STEP_START_NETWORK:
        g_message ("connection %u/%u step %u/%u: starting network...",
                   connect_ctx->iteration, MAX_CONNECT_ITERATIONS, connect_ctx->step, CONNECT_STEP_LAST);
        connect_step_start_network (ctx);
        return;

    case CONNECT_STEP_IP_SETTINGS:
        g_message ("connection %u/%u step %u/%u: retrieving IPv4 settings...",
                   connect_ctx->iteration, MAX_CONNECT_ITERATIONS, connect_ctx->step, CONNECT_STEP_LAST);
        connect_step_ip_settings (ctx);
        return;

    case CONNECT_STEP_WWAN_SETUP:
        g_message ("connection %u/%u step %u/%u: wwan interface setup...",
                   connect_ctx->iteration, MAX_CONNECT_ITERATIONS, connect_ctx->step, CONNECT_STEP_LAST);
        connect_step_wwan_setup (ctx);
        return;

    case CONNECT_STEP_STATS:
        g_message ("connection %u/%u step %u/%u: reseting stats...",
                   connect_ctx->iteration, MAX_CONNECT_ITERATIONS, connect_ctx->step, CONNECT_STEP_LAST);
        connect_step_stats (ctx);
        return;

    case CONNECT_STEP_LAST:
        /* Ok! */
        g_message ("connection %u/%u step %u/%u: successfully connected",
                   connect_ctx->iteration, MAX_CONNECT_ITERATIONS, connect_ctx->step, CONNECT_STEP_LAST);
        ctx->self->priv->connection_status = RMF_CONNECTION_STATUS_CONNECTED;

        response = rmf_message_connect_response_new ();
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   g_byte_array_new_take (response, rmf_message_get_length (response)),
                                                   (GDestroyNotify)g_byte_array_unref);
        run_context_complete_and_free (ctx);
        return;
    }
}

static void
run_connect (RunContext *ctx)
{
    ConnectContext *connect_ctx;

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

    /* Setup connect context */
    connect_ctx = g_slice_new0 (ConnectContext);
    connect_ctx->step = CONNECT_STEP_FIRST;
    connect_ctx->iteration = 1;
    run_context_set_additional_context (ctx, connect_ctx, (GDestroyNotify)connect_context_free);
    connect_step (ctx);
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
write_connection_stats_stop_ready (RmfdPortProcessorQmi *self,
                                   GAsyncResult         *res,
                                   RunContext           *ctx)
{
    GError *error = NULL;

    if (!write_connection_stats_finish (self, res, &error)) {
        g_debug ("couldn't write final connection stats: %s", error->message);
        g_clear_error (&error);
    }

    /* Remove ongoing stats timeout */
    self->priv->stats_enabled = FALSE;
    schedule_stats (self);

    rmfd_port_data_setup (ctx->data,
                          FALSE,
                          NULL, NULL, NULL, NULL, NULL, 0,
                          (GAsyncReadyCallback)data_setup_stop_ready,
                          ctx);
}

static void
wds_stop_network_ready (QmiClientWds *client,
                        GAsyncResult *res,
                        RunContext   *ctx)
{
    GError                         *error = NULL;
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

    /* Then, query stats */
    write_connection_stats (ctx->self,
                            RMFD_STATS_RECORD_TYPE_FINAL,
                            (GAsyncReadyCallback)write_connection_stats_stop_ready,
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
    qmi_client_wds_stop_network (QMI_CLIENT_WDS (peek_qmi_client (ctx->self, QMI_SERVICE_WDS)),
                                 input,
                                 30,
                                 NULL,
                                 (GAsyncReadyCallback)wds_stop_network_ready,
                                 ctx);
    qmi_message_wds_stop_network_input_unref (input);
}

/**********************/
/* Get manufacturer */

static gboolean
get_data_port_cb (RunContext *ctx)
{
    guint8 *response;

    response = rmf_message_get_data_port_response_new (rmfd_port_get_interface (RMFD_PORT (ctx->data)));
    g_simple_async_result_set_op_res_gpointer (ctx->result,
                                               g_byte_array_new_take (response, rmf_message_get_length (response)),
                                               (GDestroyNotify)g_byte_array_unref);
    run_context_complete_and_free (ctx);
    return FALSE;
}

static void
get_data_port (RunContext *ctx)
{
    g_idle_add ((GSourceFunc) get_data_port_cb, ctx);
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
    ctx->self = RMFD_PORT_PROCESSOR_QMI (g_object_ref (self));
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
    case RMF_MESSAGE_COMMAND_POWER_CYCLE:
        power_cycle (ctx);
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
        run_connect (ctx);
        return;
    case RMF_MESSAGE_COMMAND_DISCONNECT:
        disconnect (ctx);
        return;
    case RMF_MESSAGE_COMMAND_GET_DATA_PORT:
        get_data_port (ctx);
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
    QmiClientWms *wms;

    wms = QMI_CLIENT_WMS (peek_qmi_client (self, QMI_SERVICE_WMS));

    text = rmfd_sms_get_text (sms);
    if (text)
        text_str = text->str;
    number_str = rmfd_sms_get_number (sms);
    timestamp_str = rmfd_sms_get_timestamp (sms);

    rmfd_syslog (LOG_INFO, "SMS [Timestamp: %s] [From: %s] %s",
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
            qmi_client_wms_delete (wms, input, 5, NULL, NULL, NULL);
            qmi_message_wms_delete_input_unref (input);
        }
    }
}

/*****************************************************************************/
/* Messaging SMS part processing */

static void
process_read_sms_part (RmfdPortProcessorQmi *self,
                       QmiWmsStorageType     storage,
                       guint32               ind,
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

    g_debug ("[messaging] received 3GPP SMS part (%s,%u)", qmi_wms_storage_type_get_string (storage), ind);
    part = rmfd_sms_part_3gpp_new_from_binary_pdu (ind, (guint8 *)data->data, data->len, &error);
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
        ctx->client = QMI_CLIENT_WMS (g_object_ref (client));
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
    QmiClientWms                     *wms;

    if (self->priv->messaging_event_report_indication_id == 0)
        return;

    wms = QMI_CLIENT_WMS (peek_qmi_client (self, QMI_SERVICE_WMS));

    g_signal_handler_disconnect (wms, self->priv->messaging_event_report_indication_id);
    self->priv->messaging_event_report_indication_id = 0;

    input = qmi_message_wms_set_event_report_input_new ();
    qmi_message_wms_set_event_report_input_set_new_mt_message_indicator (input, FALSE, NULL);
    qmi_client_wms_set_event_report (wms, input, 5, NULL, NULL, NULL);
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
    gboolean need_retry_read;
    gboolean need_retry_not_read;
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

    qmi_client_wms_raw_read (QMI_CLIENT_WMS (peek_qmi_client (ctx->self, QMI_SERVICE_WMS)),
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

        /* Flag as needing retry */
        if (ctx->step == MESSAGING_LIST_PARTS_CONTEXT_STEP_LIST_READ)
            ctx->need_retry_read = TRUE;
        else if (ctx->step == MESSAGING_LIST_PARTS_CONTEXT_STEP_LIST_NOT_READ)
            ctx->need_retry_not_read = TRUE;

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

    qmi_client_wms_list_messages (QMI_CLIENT_WMS (peek_qmi_client (ctx->self, QMI_SERVICE_WMS)),
                                  input,
                                  5,
                                  NULL,
                                  (GAsyncReadyCallback) wms_list_messages_ready,
                                  ctx);
    qmi_message_wms_list_messages_input_unref (input);
}

static void messaging_list_context_done       (RmfdPortProcessorQmi *self,
                                               QmiWmsStorageType     storage);
static void messaging_list_context_reschedule (RmfdPortProcessorQmi *self,
                                               QmiWmsStorageType     storage);

static void
messaging_list_parts_context_step (MessagingListPartsContext *ctx)
{
    switch (ctx->step) {
    case MESSAGING_LIST_PARTS_CONTEXT_STEP_FIRST:
        ctx->step++;
        /* fall through */

    case MESSAGING_LIST_PARTS_CONTEXT_STEP_LIST_READ:
        ctx->tag = QMI_WMS_MESSAGE_TAG_TYPE_MT_READ;
        messaging_list_parts_context_list (ctx);
        return;

    case MESSAGING_LIST_PARTS_CONTEXT_STEP_LIST_NOT_READ:
        ctx->tag = QMI_WMS_MESSAGE_TAG_TYPE_MT_NOT_READ;
        messaging_list_parts_context_list (ctx);
        return;

    case MESSAGING_LIST_PARTS_CONTEXT_STEP_LAST:
        /* Only re-schedule when both couldn't be read. Some modems may return a
         * DeviceNotReady error when listing read messages, but succeed with an empty
         * list when listing not-read messages. We'll just assume that the list
         * operation can be finished as soon as either one or the other works. */
        if (ctx->need_retry_read && ctx->need_retry_not_read)
            messaging_list_context_reschedule (ctx->self, ctx->storage);
        else
            messaging_list_context_done (ctx->self, ctx->storage);
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

    g_debug ("[messaging] listing parts in storage '%s'...",
             qmi_wms_storage_type_get_string (ctx->storage));

    messaging_list_parts_context_step (ctx);
}

/*****************************************************************************/

typedef enum {
    MESSAGING_LIST_STATUS_NONE,
    MESSAGING_LIST_STATUS_ONGOING,
    MESSAGING_LIST_STATUS_DONE,
    MESSAGING_LIST_STATUS_ABORTED,
} MessagingListStatus;

typedef struct {
    RmfdPortProcessorQmi *self; /* not full ref */
    QmiWmsStorageType     storage;
    MessagingListStatus   status;
    guint                 id;
    guint                 retries;
} MessagingListContext;

static void
messaging_list_context_clear (MessagingListContext *ctx)
{
    if (ctx->id)
        g_source_remove (ctx->id);
}

static void
messaging_list_contexts_cancel (RmfdPortProcessorQmi *self)
{
    if (self->priv->messaging_sms_contexts) {
        g_array_unref (self->priv->messaging_sms_contexts);
        self->priv->messaging_sms_contexts = NULL;
    }
}

static MessagingListContext *
messaging_list_context_find (RmfdPortProcessorQmi *self,
                             QmiWmsStorageType     storage)
{
    guint i;

    for (i = 0; i < self->priv->messaging_sms_contexts->len; i++) {
        MessagingListContext *ctx;

        ctx = &g_array_index (self->priv->messaging_sms_contexts, MessagingListContext, i);
        if (ctx->storage == storage)
            return ctx;
    }

    g_assert_not_reached ();
    return NULL;
}

static gboolean
messaging_list_context_reschedule_cb (MessagingListContext *ctx)
{
    ctx->id = 0;
    messaging_list_parts (ctx->self, ctx->storage);
    return FALSE;
}

static void
messaging_list_context_reschedule (RmfdPortProcessorQmi *self,
                                   QmiWmsStorageType     storage)
{
    MessagingListContext *ctx;

    ctx = messaging_list_context_find (self, storage);
    if (++ctx->retries == MESSAGING_LIST_MAX_RETRIES) {
        g_debug ("[messaging] listing parts in storage '%s' aborted (too many retries)...",
                 qmi_wms_storage_type_get_string (storage));
        ctx->status = MESSAGING_LIST_STATUS_ABORTED;
        return;
    }

    g_assert (ctx->id == 0);

    g_debug ("[messaging] re-scheduling listing parts in storage '%s'...",
             qmi_wms_storage_type_get_string (storage));
    ctx->id = g_timeout_add_seconds (MESSAGING_LIST_RETRY_TIMEOUT_SECS,
                                     (GSourceFunc) messaging_list_context_reschedule_cb,
                                     ctx);
}

static void
messaging_list_context_done (RmfdPortProcessorQmi *self,
                             QmiWmsStorageType     storage)
{
    MessagingListContext *ctx;

    g_debug ("[messaging] listing parts in storage '%s' finished",
             qmi_wms_storage_type_get_string (storage));

    ctx = messaging_list_context_find (self, storage);
    ctx->status = MESSAGING_LIST_STATUS_DONE;
}

static void
messaging_list_context_init (RmfdPortProcessorQmi *self,
                             QmiWmsStorageType     storage)
{
    MessagingListContext *ctx;

    g_debug ("[messaging] request to list parts in storage '%s'",
             qmi_wms_storage_type_get_string (storage));

    ctx = messaging_list_context_find (self, storage);
    switch (ctx->status) {
    case MESSAGING_LIST_STATUS_NONE:
        messaging_list_context_reschedule_cb (ctx);
        return;
    case MESSAGING_LIST_STATUS_ONGOING:
        ctx->retries = 0;
        return;
    case MESSAGING_LIST_STATUS_DONE:
        return;
    case MESSAGING_LIST_STATUS_ABORTED:
        ctx->retries = 0;
        messaging_list_context_reschedule_cb (ctx);
        return;
    }
}

static void
messaging_list (RmfdPortProcessorQmi *self)
{
    if (!G_LIKELY (self->priv->messaging_sms_contexts)) {
        MessagingListContext ctx;

        self->priv->messaging_sms_contexts = g_array_sized_new (FALSE, FALSE, sizeof (MessagingListContext), 2);
        g_array_set_clear_func (self->priv->messaging_sms_contexts, (GDestroyNotify) messaging_list_context_clear);

        ctx.self    = self;
        ctx.status  = MESSAGING_LIST_STATUS_NONE;
        ctx.id      = 0;
        ctx.retries = 0;

        ctx.storage = QMI_WMS_STORAGE_TYPE_UIM;
        g_array_append_val (self->priv->messaging_sms_contexts, ctx);

        ctx.storage = QMI_WMS_STORAGE_TYPE_NV;
        g_array_append_val (self->priv->messaging_sms_contexts, ctx);
    }

    messaging_list_context_init (self, QMI_WMS_STORAGE_TYPE_UIM);
    messaging_list_context_init (self, QMI_WMS_STORAGE_TYPE_NV);
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
    RmfdPortProcessorQmi     *self;
    QmiClientWms             *wms;
    GSimpleAsyncResult       *result;
    MessagingInitContextStep  step;
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
        g_debug ("[messaging] initializing...");
        ctx->step++;
        /* fall through */

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
        qmi_client_wms_set_routes (ctx->wms,
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
            g_signal_connect (ctx->wms,
                              "event-report",
                              G_CALLBACK (messaging_event_report_indication_cb),
                              ctx->self);

        input = qmi_message_wms_set_event_report_input_new ();
        qmi_message_wms_set_event_report_input_set_new_mt_message_indicator (input, TRUE, NULL);
        qmi_client_wms_set_event_report (ctx->wms,
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

    ctx = g_slice_new (MessagingInitContext);
    ctx->self = g_object_ref (self);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             messaging_init);
    ctx->step = MESSAGING_INIT_CONTEXT_STEP_FIRST;
    ctx->wms = QMI_CLIENT_WMS (peek_qmi_client (ctx->self, QMI_SERVICE_WMS));
    g_assert (QMI_IS_CLIENT_WMS (ctx->wms));

    messaging_init_context_step (ctx);
}

/*****************************************************************************/
/* Data format init */

typedef enum {
    DATA_FORMAT_INIT_CONTEXT_STEP_FIRST,
    DATA_FORMAT_INIT_CONTEXT_STEP_KERNEL_DATA_FORMAT,
    DATA_FORMAT_INIT_CONTEXT_STEP_CLIENT_WDA,
    DATA_FORMAT_INIT_CONTEXT_STEP_CLIENT_DATA_FORMAT,
    DATA_FORMAT_INIT_CONTEXT_STEP_CHECK,
    DATA_FORMAT_INIT_CONTEXT_STEP_SET_KERNEL_DATA_FORMAT,
    DATA_FORMAT_INIT_CONTEXT_STEP_LAST,
} DataFormatInitContextStep;

typedef struct {
    RmfdPortProcessorQmi        *self;
    GSimpleAsyncResult          *result;
    GCancellable                *cancellable;
    DataFormatInitContextStep    step;
    QmiDeviceExpectedDataFormat  kernel_data_format;
    QmiWdaLinkLayerProtocol      llp;
    QmiClientWda                *wda;
} DataFormatInitContext;

static void
data_format_init_context_complete_and_free (DataFormatInitContext *ctx)
{
    g_simple_async_result_complete_in_idle (ctx->result);
    if (ctx->wda) {
        qmi_device_release_client (ctx->self->priv->qmi_device,
                                   QMI_CLIENT (ctx->wda),
                                   QMI_DEVICE_RELEASE_CLIENT_FLAGS_RELEASE_CID,
                                   3, NULL, NULL, NULL);
        g_object_unref (ctx->wda);
    }
    if (ctx->cancellable)
        g_object_unref (ctx->cancellable);
    g_object_unref (ctx->result);
    g_object_unref (ctx->self);
    g_slice_free (DataFormatInitContext, ctx);
}

static gboolean
data_format_init_finish (RmfdPortProcessorQmi  *self,
                         GAsyncResult          *res,
                         GError               **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

static void data_format_init_context_step (DataFormatInitContext *ctx);

static void
get_data_format_ready (QmiClientWda          *client,
                       GAsyncResult          *res,
                       DataFormatInitContext *ctx)
{
    QmiMessageWdaGetDataFormatOutput *output;
    GError                           *error = NULL;

    output = qmi_client_wda_get_data_format_finish (client, res, &error);
    if (!output ||
        !qmi_message_wda_get_data_format_output_get_result (output, &error) ||
        !qmi_message_wda_get_data_format_output_get_link_layer_protocol (output, &ctx->llp, &error)) {
        g_prefix_error (&error, "error retrieving data format with WDA client: ");
        g_simple_async_result_take_error (ctx->result, error);
        data_format_init_context_complete_and_free (ctx);
        goto out;
    }

    /* Go on to next step */
    ctx->step++;
    data_format_init_context_step (ctx);

out:
    if (output)
        qmi_message_wda_get_data_format_output_unref (output);
}

static void
wda_allocate_client_ready (QmiDevice             *qmi_device,
                           GAsyncResult          *res,
                           DataFormatInitContext *ctx)
{
    GError *error = NULL;

    ctx->wda = QMI_CLIENT_WDA (qmi_device_allocate_client_finish (qmi_device, res, &error));
    if (!ctx->wda) {
        g_prefix_error (&error, "device doesn't support WDA service: ");
        g_simple_async_result_take_error (ctx->result, error);
        data_format_init_context_complete_and_free (ctx);
        return;
    }

    /* Go on to next step */
    ctx->step++;
    data_format_init_context_step (ctx);
}

static void
data_format_init_context_step (DataFormatInitContext *ctx)
{
    switch (ctx->step) {
    case DATA_FORMAT_INIT_CONTEXT_STEP_FIRST:
        ctx->step++;
        /* fall through */

    case DATA_FORMAT_INIT_CONTEXT_STEP_KERNEL_DATA_FORMAT:
        /* Try to gather expected data format from the sysfs file */
        ctx->kernel_data_format = qmi_device_get_expected_data_format (ctx->self->priv->qmi_device, NULL);
        if (ctx->kernel_data_format == QMI_DEVICE_EXPECTED_DATA_FORMAT_UNKNOWN) {
            g_simple_async_result_set_error (ctx->result,
                                             RMFD_ERROR,
                                             RMFD_ERROR_NOT_SUPPORTED,
                                             "kernel doesn't support data format setting");
            data_format_init_context_complete_and_free (ctx);
            return;
        }
        ctx->step++;
        /* fall through */

    case DATA_FORMAT_INIT_CONTEXT_STEP_CLIENT_WDA:
        qmi_device_allocate_client (ctx->self->priv->qmi_device,
                                    QMI_SERVICE_WDA,
                                    QMI_CID_NONE,
                                    10,
                                    ctx->cancellable,
                                    (GAsyncReadyCallback) wda_allocate_client_ready,
                                    ctx);
        return;

    case DATA_FORMAT_INIT_CONTEXT_STEP_CLIENT_DATA_FORMAT:
        qmi_client_wda_get_data_format (ctx->wda,
                                        NULL,
                                        10,
                                        ctx->cancellable,
                                        (GAsyncReadyCallback) get_data_format_ready,
                                        ctx);
        return;

    case DATA_FORMAT_INIT_CONTEXT_STEP_CHECK:
        g_debug ("Checking data format: kernel %s, device %s",
                 qmi_device_expected_data_format_get_string (ctx->kernel_data_format),
                 qmi_wda_link_layer_protocol_get_string (ctx->llp));

        if (ctx->kernel_data_format == QMI_DEVICE_EXPECTED_DATA_FORMAT_802_3 &&
            ctx->llp == QMI_WDA_LINK_LAYER_PROTOCOL_802_3) {
            ctx->self->priv->llp_is_raw_ip = FALSE;
            ctx->step = DATA_FORMAT_INIT_CONTEXT_STEP_LAST;
            data_format_init_context_complete_and_free (ctx);
            return;
        }

        if (ctx->kernel_data_format == QMI_DEVICE_EXPECTED_DATA_FORMAT_RAW_IP &&
            ctx->llp == QMI_WDA_LINK_LAYER_PROTOCOL_RAW_IP) {
            ctx->self->priv->llp_is_raw_ip = TRUE;
            ctx->step = DATA_FORMAT_INIT_CONTEXT_STEP_LAST;
            data_format_init_context_complete_and_free (ctx);
            return;
        }

        ctx->step++;
        /* fall through */

    case DATA_FORMAT_INIT_CONTEXT_STEP_SET_KERNEL_DATA_FORMAT: {
        GError *error = NULL;

        /* Update the data format to be expected by the kernel */
        g_debug ("Updating kernel data format: %s", qmi_wda_link_layer_protocol_get_string (ctx->llp));
        if (ctx->llp == QMI_WDA_LINK_LAYER_PROTOCOL_802_3) {
            ctx->kernel_data_format = QMI_DEVICE_EXPECTED_DATA_FORMAT_802_3;
            ctx->self->priv->llp_is_raw_ip = FALSE;
        } else if (ctx->llp == QMI_WDA_LINK_LAYER_PROTOCOL_RAW_IP) {
            ctx->kernel_data_format = QMI_DEVICE_EXPECTED_DATA_FORMAT_RAW_IP;
            ctx->self->priv->llp_is_raw_ip = TRUE;
        } else
            g_assert_not_reached ();

        /* Regardless of the output, we're done after this action */
        if (!qmi_device_set_expected_data_format (ctx->self->priv->qmi_device,
                                                  ctx->kernel_data_format,
                                                  &error)) {
            g_simple_async_result_take_error (ctx->result, error);
            data_format_init_context_complete_and_free (ctx);
            return;
        }

        ctx->step++;
    }
    /* fall through */

    case DATA_FORMAT_INIT_CONTEXT_STEP_LAST:
        g_simple_async_result_set_op_res_gboolean (ctx->result, TRUE);
        data_format_init_context_complete_and_free (ctx);
        return;
    }
}

static void
data_format_init (RmfdPortProcessorQmi *self,
                  GCancellable         *cancellable,
                  GAsyncReadyCallback   callback,
                  gpointer              user_data)
{
    DataFormatInitContext *ctx;

    ctx = g_slice_new0 (DataFormatInitContext);
    ctx->self   = g_object_ref (self);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             data_format_init);
    ctx->step = DATA_FORMAT_INIT_CONTEXT_STEP_FIRST;
    ctx->kernel_data_format = QMI_DEVICE_EXPECTED_DATA_FORMAT_UNKNOWN;
    ctx->llp = QMI_WDA_LINK_LAYER_PROTOCOL_UNKNOWN;

    data_format_init_context_step (ctx);
}

/*****************************************************************************/
/* Processor init */

typedef enum {
    INIT_CONTEXT_STEP_FIRST,
    INIT_CONTEXT_STEP_DEVICE_NEW,
    INIT_CONTEXT_STEP_DEVICE_OPEN,
    INIT_CONTEXT_STEP_DATA_FORMAT_INIT,
    INIT_CONTEXT_STEP_DEVICE_REOPEN_802_3,
    INIT_CONTEXT_STEP_CLIENTS,
    INIT_CONTEXT_STEP_MESSAGING_INIT,
    INIT_CONTEXT_STEP_LAST,
} InitContextStep;

typedef struct {
    RmfdPortProcessorQmi        *self;
    GSimpleAsyncResult          *result;
    GCancellable                *cancellable;
    InitContextStep              step;
    guint                        clients_i;
} InitContext;

static void
init_context_complete_and_free (InitContext *ctx)
{
    g_simple_async_result_complete (ctx->result);
    if (ctx->cancellable)
        g_object_unref (ctx->cancellable);
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

static void init_context_step (InitContext *ctx);

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

    /* Go on to next step */
    ctx->step++;
    init_context_step (ctx);
}

static void
allocate_client_ready (QmiDevice    *qmi_device,
                       GAsyncResult *res,
                       InitContext  *ctx)
{
    GError    *error = NULL;
    QmiClient *client;

    client = qmi_device_allocate_client_finish (qmi_device, res, &error);
    if (!client) {
        g_prefix_error (&error, "couldn't allocate client for service '%s': ",
                        qmi_service_get_string (service_items[ctx->clients_i].service));
        g_simple_async_result_take_error (ctx->result, error);
        init_context_complete_and_free (ctx);
        return;
    }

    g_debug ("QMI client for service '%s' created",
             qmi_service_get_string (service_items[ctx->clients_i].service));
    track_qmi_service (ctx->self,
                       service_items[ctx->clients_i].service,
                       client);
    g_object_unref (client);

    /* Update client index and re-run the same step */
    ctx->clients_i++;
    init_context_step (ctx);
}

static void
data_format_init_ready (RmfdPortProcessorQmi *self,
                        GAsyncResult         *res,
                        InitContext          *ctx)
{
    GError *error = NULL;

    if (!data_format_init_finish (self, res, &error)) {
        g_debug ("Data format not initialized: %s", error->message);
        g_error_free (error);
        /* Go on to next step */
        ctx->step++;
    } else {
        g_debug ("Data format initialized");
        /* Skip next step */
        ctx->step = INIT_CONTEXT_STEP_CLIENTS;
    }

    init_context_step (ctx);
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

    /* Go on to next step */
    ctx->step++;
    init_context_step (ctx);
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

    /* Go on to next step */
    ctx->step++;
    init_context_step (ctx);
}

static void
init_context_step (InitContext *ctx)
{
    switch (ctx->step) {
    case INIT_CONTEXT_STEP_FIRST:
        ctx->step++;
        /* fall through */

    case INIT_CONTEXT_STEP_DEVICE_NEW: {
        GFile *file;

        /* Launch device creation */
        g_debug ("creating QMI device...");
        file = g_file_new_for_path (rmfd_port_get_interface (RMFD_PORT (ctx->self)));
        qmi_device_new (file,
                        ctx->cancellable,
                        (GAsyncReadyCallback) device_new_ready,
                        ctx);
        g_object_unref (file);
        return;
    }

    case INIT_CONTEXT_STEP_DEVICE_OPEN: {
        QmiDeviceOpenFlags flags;

        flags = (QMI_DEVICE_OPEN_FLAGS_SYNC |
                 QMI_DEVICE_OPEN_FLAGS_VERSION_INFO);

        if (g_getenv ("RMFD_QMI_PROXY"))
            flags |= QMI_DEVICE_OPEN_FLAGS_PROXY;

        /* Open the QMI port */
        g_debug ("opening QMI device...");
        qmi_device_open (ctx->self->priv->qmi_device,
                         flags,
                         10,
                         ctx->cancellable,
                         (GAsyncReadyCallback) device_open_ready,
                         ctx);
        return;
    }

    case INIT_CONTEXT_STEP_DATA_FORMAT_INIT:
        g_debug ("running data format initialization...");
        data_format_init (ctx->self,
                          ctx->cancellable,
                         (GAsyncReadyCallback) data_format_init_ready,
                         ctx);
        return;

    case INIT_CONTEXT_STEP_DEVICE_REOPEN_802_3: {
        QmiDeviceOpenFlags  flags;
        GError             *error = NULL;

        flags = (QMI_DEVICE_OPEN_FLAGS_SYNC |
                 QMI_DEVICE_OPEN_FLAGS_VERSION_INFO |
                 QMI_DEVICE_OPEN_FLAGS_NET_802_3 |
                 QMI_DEVICE_OPEN_FLAGS_NET_NO_QOS_HEADER);

        if (g_getenv ("RMFD_QMI_PROXY"))
            flags |= QMI_DEVICE_OPEN_FLAGS_PROXY;

        /* Close the QMI port */
        if (!qmi_device_close (ctx->self->priv->qmi_device, &error)) {
            g_warning ("error closing QMI device: %s", error->message);
            g_error_free (error);
        }

        /* Open the QMI port */
        g_debug ("(re)opening QMI device with 802.3 requested...");
        qmi_device_open (ctx->self->priv->qmi_device,
                         flags,
                         10,
                         ctx->cancellable,
                         (GAsyncReadyCallback) device_open_ready,
                         ctx);
        return;
    }

    case INIT_CONTEXT_STEP_CLIENTS:
        /* Allocate next client */
        if (ctx->clients_i < G_N_ELEMENTS (service_items)) {
            g_debug ("allocating QMI client for service '%s'", qmi_service_get_string (service_items[ctx->clients_i].service));
            qmi_device_allocate_client (ctx->self->priv->qmi_device,
                                        service_items[ctx->clients_i].service,
                                        QMI_CID_NONE,
                                        10,
                                        ctx->cancellable,
                                        (GAsyncReadyCallback) allocate_client_ready,
                                        ctx);
            return;
        }

        g_debug ("All QMI clients created");

        ctx->step++;
        /* fall through */

    case INIT_CONTEXT_STEP_MESSAGING_INIT:
        g_debug ("initializing messaging support...");
        messaging_init (ctx->self,
                        (GAsyncReadyCallback) messaging_init_ready,
                        ctx);
        return;

    case INIT_CONTEXT_STEP_LAST:
        /* Register NAS indications */
        register_nas_indications (ctx->self);
        /* Launch automatic network registration explicitly */
        initiate_registration (ctx->self, TRUE);
        /* And launch SMS listing, which will succeed here only if PIN unlocked or disabled */
        messaging_list (ctx->self);

        /* And complete with success */
        g_debug ("processor successfully initialized");
        g_simple_async_result_set_op_res_gboolean (ctx->result, TRUE);
        init_context_complete_and_free (ctx);
        return;
    }
}

static void
initable_init_async (GAsyncInitable      *initable,
                     int                  io_priority,
                     GCancellable        *cancellable,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
    InitContext *ctx;

    ctx = g_slice_new0 (InitContext);
    ctx->self = RMFD_PORT_PROCESSOR_QMI (g_object_ref (initable));
    ctx->result = g_simple_async_result_new (G_OBJECT (initable),
                                             callback,
                                             user_data,
                                             initable_init_async);
    ctx->cancellable = (cancellable ? g_object_ref (cancellable) : NULL);
    ctx->step = INIT_CONTEXT_STEP_FIRST;
    ctx->clients_i = 0;

    init_context_step (ctx);
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

    /* Initialize stats */
    self->priv->stats = rmfd_stats_setup (STATS_FILE_PATH);
}

static void
dispose (GObject *object)
{
    RmfdPortProcessorQmi *self = RMFD_PORT_PROCESSOR_QMI (object);
    guint                 i;

    if (self->priv->stats) {
        rmfd_stats_teardown (self->priv->stats);
        self->priv->stats = NULL;
    }

    messaging_list_contexts_cancel (self);
    registration_context_cancel (self);
    unregister_nas_indications (self);
    unregister_wms_indications (self);

    if (self->priv->messaging_sms_list)
        g_signal_handlers_disconnect_by_func (self->priv->messaging_sms_list, sms_added_cb, self);
    g_clear_object (&self->priv->messaging_sms_list);

    if (self->priv->stats_timeout_id) {
        g_source_remove (self->priv->stats_timeout_id);
        self->priv->stats_timeout_id = 0;
    }

    for (i = 0; i < G_N_ELEMENTS (service_items); i++)
        untrack_qmi_service (self, service_items[i].service);

    if (self->priv->qmi_device && qmi_device_is_open (self->priv->qmi_device)) {
        GError *error = NULL;

        if (!qmi_device_close (self->priv->qmi_device, &error)) {
            g_warning ("error closing QMI device: %s", error->message);
            g_error_free (error);
        } else
            g_debug ("QmiDevice closed: %s", qmi_device_get_path (self->priv->qmi_device));
    }
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

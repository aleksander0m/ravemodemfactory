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
#include <ctype.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gudev/gudev.h>
#include <gio/gunixsocketaddress.h>

#include <rmf-messages.h>

#include "rmfd-manager.h"
#include "rmfd-port-processor-qmi.h"
#include "rmfd-port-data-wwan.h"
#include "rmfd-error.h"
#include "rmfd-error-types.h"
#include "rmfd-utils.h"

G_DEFINE_TYPE (RmfdManager, rmfd_manager, G_TYPE_OBJECT)

struct _RmfdManagerPrivate {
    /* The UDev client */
    GUdevClient *udev_client;
    guint initial_scan_id;

    /* Current modem */
    RmfdModemType type;
    GUdevDevice *parent;
    gboolean processor_probing;
    RmfdPortProcessor *processor;
    RmfdPortData *data;
    GList *processor_ports;
    GList *data_ports;

    /* Unix socket service */
    GSocketService *socket_service;
    GByteArray *socket_buffer;

    /* Pending requests to process */
    GList *requests;
    guint requests_idle_id;
};

/*****************************************************************************/

static void
cleanup_current_device (RmfdManager *self)
{
    if (self->priv->processor) {
        g_debug ("    removing processor port at '%s'",
                 rmfd_port_get_interface (RMFD_PORT (self->priv->processor)));
        g_clear_object (&self->priv->processor);
    }

    if (self->priv->data) {
        g_debug ("    removing data port at '%s'",
                 rmfd_port_get_interface (RMFD_PORT (self->priv->data)));
        rmfd_port_data_setup (self->priv->data, FALSE, NULL, NULL);
        g_clear_object (&self->priv->data);
    }

    g_clear_object (&self->priv->parent);

    self->priv->type = RMFD_MODEM_TYPE_UNKNOWN;
}

static GList *
find_port (GList **list,
           GUdevDevice *device)
{
    GList *l;

    g_assert (list != NULL);

    for (l = *list; l; l = g_list_next (l)) {
        GUdevDevice *aux;

        aux = G_UDEV_DEVICE (l->data);
        if (g_str_equal (g_udev_device_get_subsystem (device), g_udev_device_get_subsystem (aux)) &&
            g_str_equal (g_udev_device_get_name (device), g_udev_device_get_name (aux))) {
            return l;
        }
    }
    return NULL;
}

static void
track_port (GList **list,
            GUdevDevice *device)
{
    g_assert (list != NULL);

    if (find_port (list, device))
        return;

    *list = g_list_prepend (*list, g_object_ref (device));
}

static void
untrack_port (GList **list,
              GUdevDevice *device)
{
    GList *l;

    g_assert (list != NULL);

    if ((l = find_port (list, device)) == NULL)
        return;

    g_object_unref (l->data);
    *list = g_list_delete_link (*list, l);
}

static GUdevDevice *
peek_data_for_qmi (RmfdManager *self,
                   GUdevDevice *device)
{
    GUdevDevice *qmi_device_parent;
    GUdevDevice *found;
    GList *l;

    /* Get parent of the data device */
    qmi_device_parent = g_udev_device_get_parent (device);
    if (!qmi_device_parent) {
        g_warning ("cannot get parent device for QMI port '%s'", g_udev_device_get_name (device));
        return NULL;
    }

    /* Now walk the list of net ports looking for a match */
    found = NULL;
    for (l = self->priv->data_ports; l && !found; l = g_list_next (l)) {
        GUdevDevice *data_device_parent;

        /* Get parent of the data device */
        data_device_parent = g_udev_device_get_parent (G_UDEV_DEVICE (l->data));
        if (!data_device_parent) {
            g_warning ("cannot get parent device for data port '%s'",
                       g_udev_device_get_name (G_UDEV_DEVICE (l->data)));
            continue;
        }

        if (g_str_equal (g_udev_device_get_sysfs_path (data_device_parent),
                         g_udev_device_get_sysfs_path (qmi_device_parent)))
            found = G_UDEV_DEVICE (l->data);

        g_object_unref (data_device_parent);
    }

    if (!found)
        g_warning ("cannot get associated data port for qmi port '%s'",
                   g_udev_device_get_name (device));
    g_object_unref (qmi_device_parent);

    return found;
}

typedef struct {
    RmfdManager *self;
    GUdevDevice *device;
} ProbingPortContext;

static void
probing_port_context_free (ProbingPortContext *ctx)
{
    g_object_unref (ctx->self);
    g_object_unref (ctx->device);
    g_slice_free (ProbingPortContext, ctx);
}

static void
processor_qmi_new_ready (GObject      *source,
                         GAsyncResult *res,
                         ProbingPortContext *ctx)
{
    GError *error = NULL;

    ctx->self->priv->processor_probing = FALSE;
    ctx->self->priv->processor = rmfd_port_processor_qmi_new_finish (res, &error);

    if (ctx->self->priv->processor) {
        GUdevDevice *data;

        /* Processor correctly created for a QMI port, now look for corresponding WWAN */
        data = peek_data_for_qmi (ctx->self, ctx->device);
        if (data) {
            gchar *interface;

            interface = rmfd_utils_build_interface_name (data);
            g_assert (ctx->self->priv->data == NULL);
            ctx->self->priv->data = rmfd_port_data_wwan_new (interface);
            g_free (interface);

            /* All ready! */
            g_message ("modem ready at QMI (%s) and WWAN (%s)",
                       rmfd_port_get_interface (RMFD_PORT (ctx->self->priv->processor)),
                       rmfd_port_get_interface (RMFD_PORT (ctx->self->priv->data)));

            g_list_free_full (ctx->self->priv->processor_ports, g_object_unref);
            ctx->self->priv->processor_ports = NULL;
            g_list_free_full (ctx->self->priv->data_ports, g_object_unref);
            ctx->self->priv->data_ports = NULL;
            probing_port_context_free (ctx);
            return;
        }

        /* Couldn't get data port for QMI; so let's try with another QMI port */
        g_clear_object (&ctx->self->priv->processor);
    } else {
        g_message ("couldn't create processor for port '%s': %s",
                   g_udev_device_get_name (ctx->device),
                   error->message);
        g_error_free (error);
    }

    /* Retry with another port in the same device */
    if (ctx->self->priv->processor_ports) {
        gchar *interface = NULL;

        g_object_unref (ctx->device);
        ctx->device = ctx->self->priv->processor_ports->data;
        ctx->self->priv->processor_ports = g_list_delete_link (ctx->self->priv->processor_ports,
                                                               ctx->self->priv->processor_ports);

        /* Build interface name */
        interface = rmfd_utils_build_interface_name (ctx->device);
        ctx->self->priv->processor_probing = TRUE;
        rmfd_port_processor_qmi_new (interface,
                                     (GAsyncReadyCallback) processor_qmi_new_ready,
                                     ctx);
        g_free (interface);
        return;
    }

    /* No more QMI ports to try! */
    g_list_free_full (ctx->self->priv->data_ports, g_object_unref);
    ctx->self->priv->data_ports = NULL;
    cleanup_current_device (ctx->self);
    probing_port_context_free (ctx);
}

static void
port_added (RmfdManager *self,
            GUdevDevice *device)
{
    RmfdModemType type;
    gchar *interface = NULL;
    GUdevDevice *parent;

    /* Get modem type */
    type = rmfd_utils_get_modem_type (device);
    if (type == RMFD_MODEM_TYPE_UNKNOWN)
        goto out;

    /* Build interface name */
    interface = rmfd_utils_build_interface_name (device);

    /* Ignore event if port already added */
    if ((self->priv->processor && g_str_equal (interface, rmfd_port_get_interface (RMFD_PORT (self->priv->processor)))) ||
        (self->priv->data && g_str_equal (interface, rmfd_port_get_interface (RMFD_PORT (self->priv->data)))))
        goto out;

    /* Find physical device in the incoming port */
    parent = rmfd_utils_get_physical_device (device);
    if (!parent)
        goto out;

    /* If different device, clear ports */
    if (self->priv->parent &&
        !g_str_equal (g_udev_device_get_name (parent), g_udev_device_get_name (self->priv->parent))) {
        g_debug ("Removing modem '%s'", g_udev_device_get_name (self->priv->parent));
        cleanup_current_device (self);
    }

    /* If first port, setup device */
    if (!self->priv->parent) {
        g_debug ("Adding modem '%s'", g_udev_device_get_name (parent));
        self->priv->parent = g_object_ref (parent);
        if (type == RMFD_MODEM_TYPE_QMI) {
            g_debug ("    new modem is QMI capable");
            self->priv->type = RMFD_MODEM_TYPE_QMI;
        }
    }

    g_object_unref (parent);

    /* QMI modem? */
    if (self->priv->type == RMFD_MODEM_TYPE_QMI) {
        /* Add as processor? */
        if (g_str_has_prefix (g_udev_device_get_subsystem (device), "usb")) {
            g_debug ("    added port '%s' as possible QMI processor port", interface);
            /* If this is the first port being added, probe it. */
            if (!self->priv->processor_probing) {
                ProbingPortContext *ctx;

                ctx = g_slice_new (ProbingPortContext);
                ctx->self = g_object_ref (self);
                ctx->device = g_object_ref (device);

                self->priv->processor_probing = TRUE;
                track_port (&self->priv->processor_ports, device);
                rmfd_port_processor_qmi_new (interface,
                                             (GAsyncReadyCallback) processor_qmi_new_ready,
                                             ctx);
            } else {
                /* Probing of a port already ongoing, just add it to our tmp list */
                track_port (&self->priv->processor_ports, device);
            }
        }
        /* Add as net port? */
        else if (g_str_has_prefix (g_udev_device_get_subsystem (device), "net")) {
            g_debug ("    added port '%s' as possible NET data port", interface);
            track_port (&self->priv->data_ports, device);
        }
        /* Ignore */
        else
            g_debug ("    ignoring port '%s'", interface);
    }

out:
    g_free (interface);
}

static void
port_removed (RmfdManager *self,
              GUdevDevice *device)
{
    gchar *interface = NULL;

    interface = rmfd_utils_build_interface_name (device);

    /* Check if we're removing pending net or QMI ports */
    untrack_port (&self->priv->data_ports, device);
    untrack_port (&self->priv->processor_ports, device);

    /* If we remove either of the ports we use for processor or data, cleanup device */
    if ((self->priv->processor && g_str_equal (interface, rmfd_port_get_interface (RMFD_PORT (self->priv->processor)))) ||
        (self->priv->data && g_str_equal (interface, rmfd_port_get_interface (RMFD_PORT (self->priv->data))))) {
        g_debug ("Removing modem '%s'", g_udev_device_get_name (self->priv->parent));
        cleanup_current_device (self);
    }

    g_free (interface);
}

static void
uevent_cb (GUdevClient *client,
           const gchar *action,
           GUdevDevice *device,
           RmfdManager *self)
{
    /* Port added */
    if (g_str_equal (action, "add") ||
        g_str_equal (action, "move") ||
        g_str_equal (action, "change")) {
        port_added (self, device);
        return;
    }

    /* Port removed */
    if (g_str_equal (action, "remove")) {
        port_removed (self, device);
        return;
    }

    /* Ignore other actions */
}

/*****************************************************************************/

typedef struct {
    GSocketConnection *connection;
    GByteArray *message;
    GByteArray *response;
} Request;

static void
request_free (Request *request)
{
    if (request->message)
        g_byte_array_unref (request->message);
    if (request->response)
        g_byte_array_unref (request->response);
    g_output_stream_close (g_io_stream_get_output_stream (G_IO_STREAM (request->connection)), NULL, NULL);
    g_object_unref (request->connection);
    g_slice_free (Request, request);
}

static void
request_complete (const Request *request)
{
    GError *error = NULL;

    g_assert (request->response != NULL);
    if (!g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (request->connection)),
                                    request->response->data,
                                    request->response->len,
                                    NULL,
                                    NULL, /* cancellable */
                                    &error)) {
        g_warning ("error writing to output stream: %s", error->message);
        g_error_free (error);
    }
}

static void
processor_run_ready (RmfdPortProcessor *processor,
                     GAsyncResult      *result,
                     Request           *request)
{
    GError *error = NULL;

    request->response = rmfd_port_processor_run_finish (processor, result, &error);
    if (!request->response) {
        g_message ("couldn't process the request: %s", error->message);
        request->response = rmfd_error_message_new_from_gerror (request->message, error);
        g_error_free (error);
    }

    request_complete (request);
    request_free (request);
}

static void
request_process (RmfdManager *self,
                 Request     *request)
{
    if (rmf_message_get_command (request->message->data) == RMF_MESSAGE_COMMAND_IS_MODEM_AVAILABLE) {
        uint8_t modem_available;
        uint8_t *response_buffer;

        modem_available = self->priv->processor && self->priv->data;
        response_buffer = rmf_message_is_modem_available_response_new (modem_available);
        request->response = g_byte_array_new_take (response_buffer, rmf_message_get_length (response_buffer));
        request_complete (request);
        request_free (request);
        return;
    }

    if (!self->priv->processor || !self->priv->data) {
        request->response = rmfd_error_message_new_from_error (request->message, RMFD_ERROR, RMFD_ERROR_NO_MODEM, "No modem");
        request_complete (request);
        request_free (request);
        return;
    }

    rmfd_port_processor_run (self->priv->processor,
                             request->message,
                             self->priv->data,
                             (GAsyncReadyCallback)processor_run_ready,
                             request);
}

static void requests_schedule (RmfdManager *self);

static gboolean
requests_idle_cb (RmfdManager *self)
{
    Request *request;

    g_assert (self->priv->requests != NULL);

    self->priv->requests_idle_id = 0;

    /* Get the first request to process */
    request = self->priv->requests->data;
    self->priv->requests = g_list_remove (self->priv->requests, request);

    /* Process (takes ownership) */
    request_process (self, request);

    /* Re-schedule if needed */
    requests_schedule (self);

    return FALSE;
}

static void
requests_schedule (RmfdManager *self)
{
    if (!self->priv->requests)
        return;

    if (self->priv->requests_idle_id)
        return;

    self->priv->requests_idle_id = g_idle_add ((GSourceFunc)requests_idle_cb, self);
}

/*****************************************************************************/

static void
incoming_cb (GSocketService    *service,
             GSocketConnection *connection,
             RmfdManager       *self)
{
    guint32 message_size;
    GError *error = NULL;
    Request *request;
    guint8 *buffer;

    /* First, read message size (first 4 bytes) */
    if (!g_input_stream_read_all (g_io_stream_get_input_stream (G_IO_STREAM (connection)),
                                  &message_size,
                                  4,
                                  NULL,
                                  NULL, /* cancellable */
                                  &error)) {
        g_warning ("error reading from input stream: %s", error->message);
        g_error_free (error);
        return;
    }

    /* Create request */
    request = g_slice_new0 (Request);
    request->connection = g_object_ref (connection);

    buffer = g_malloc (message_size);
    memcpy (buffer, &message_size, 4);

    /* Read into buffer */
    if (!g_input_stream_read_all (g_io_stream_get_input_stream (G_IO_STREAM (connection)),
                                  &buffer[4],
                                  message_size - 4,
                                  NULL,
                                  NULL, /* cancellable */
                                  &error)) {
        g_warning ("error reading from input stream: %s", error->message);
        g_error_free (error);
        g_free (buffer);
        request_free (request);
        return;
    }

    /* Store the request message */
    request->message = g_byte_array_new_take (buffer, message_size);

    /* Push request */
    self->priv->requests = g_list_append (self->priv->requests, request);

    /* Schedule request */
    requests_schedule (self);
}

static void
setup_socket_service (RmfdManager *self)
{
    GSocketAddress *socket_address;
    GError *error = NULL;

    g_debug ("creating UNIX socket service...");

    /* Remove any previously existing socket file */
    g_unlink (RMFD_SOCKET_PATH);

    /* Create socket address */
    socket_address = g_unix_socket_address_new (RMFD_SOCKET_PATH);
    if (!g_socket_listener_add_address (G_SOCKET_LISTENER (self->priv->socket_service),
                                        socket_address,
                                        G_SOCKET_TYPE_STREAM,
                                        G_SOCKET_PROTOCOL_DEFAULT,
                                        G_OBJECT (self),
                                        NULL, /* effective_address */
                                        &error)) {
        g_warning ("error adding address to socket service: %s", error->message);
        g_error_free (error);
    } else {
        g_debug ("starting UNIX socket service...");
        g_socket_service_start (self->priv->socket_service);
    }

    g_object_unref (socket_address);
}

static gboolean
initial_scan_cb (RmfdManager *self)
{
    GList *devices, *iter;

    self->priv->initial_scan_id = 0;

    g_debug ("scanning usb subsystems...");

    devices = g_udev_client_query_by_subsystem (self->priv->udev_client, "usb");
    for (iter = devices; iter; iter = g_list_next (iter)) {
        port_added (self, G_UDEV_DEVICE (iter->data));
        g_object_unref (G_OBJECT (iter->data));
    }
    g_list_free (devices);

    devices = g_udev_client_query_by_subsystem (self->priv->udev_client, "usbmisc");
    for (iter = devices; iter; iter = g_list_next (iter)) {
        port_added (self, G_UDEV_DEVICE (iter->data));
        g_object_unref (G_OBJECT (iter->data));
    }
    g_list_free (devices);

    devices = g_udev_client_query_by_subsystem (self->priv->udev_client, "net");
    for (iter = devices; iter; iter = g_list_next (iter)) {
        port_added (self, G_UDEV_DEVICE (iter->data));
        g_object_unref (G_OBJECT (iter->data));
    }
    g_list_free (devices);

    /* Setup socket service after the initial scan */
    setup_socket_service (self);

    return FALSE;
}

/*****************************************************************************/

RmfdManager *
rmfd_manager_new (void)
{
    return g_object_new (RMFD_TYPE_MANAGER, NULL);
}

static void
rmfd_manager_init (RmfdManager *self)
{
    const gchar *subsys[] = { "net", "usb", "usbmisc", NULL };

    /* Setup private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                              RMFD_TYPE_MANAGER,
                                              RmfdManagerPrivate);

    /* Setup UDev client */
    self->priv->udev_client = g_udev_client_new (subsys);
    g_signal_connect (self->priv->udev_client, "uevent", G_CALLBACK (uevent_cb), self);

    /* Create socket service */
    self->priv->socket_service = g_socket_service_new ();
    g_signal_connect (self->priv->socket_service, "incoming", G_CALLBACK (incoming_cb), NULL);

    /* Setup initial scan */
    self->priv->initial_scan_id = g_idle_add ((GSourceFunc) initial_scan_cb, self);

    /* Socket service started after initial scan */
    self->priv->socket_buffer = g_byte_array_sized_new (RMF_MESSAGE_MAX_SIZE);
}

static void
dispose (GObject *object)
{
    RmfdManagerPrivate *priv = RMFD_MANAGER (object)->priv;

    if (priv->requests_idle_id != 0) {
        g_source_remove (priv->requests_idle_id);
        priv->requests_idle_id = 0;
    }

    if (priv->requests) {
        g_list_free_full (priv->requests, (GDestroyNotify) request_free);
        priv->requests = NULL;
    }

    if (priv->initial_scan_id != 0) {
        g_source_remove (priv->initial_scan_id);
        priv->initial_scan_id = 0;
    }

    if (priv->socket_service && g_socket_service_is_active (priv->socket_service)) {
        g_socket_service_stop (priv->socket_service);
        g_debug ("UNIX socket service stopped");
    }

    if (priv->socket_buffer) {
        g_byte_array_unref (priv->socket_buffer);
        priv->socket_buffer = NULL;
    }

    g_clear_object (&priv->socket_service);
    g_clear_object (&priv->processor);
    g_clear_object (&priv->data);
    g_clear_object (&priv->udev_client);

    G_OBJECT_CLASS (rmfd_manager_parent_class)->dispose (object);
}

static void
rmfd_manager_class_init (RmfdManagerClass *manager_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (manager_class);

    g_type_class_add_private (object_class, sizeof (RmfdManagerPrivate));

    /* Virtual methods */
    object_class->dispose = dispose;
}

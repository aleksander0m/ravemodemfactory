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

#include <string.h>
#include <ctype.h>

#include <gudev/gudev.h>
#include <gio/gunixsocketaddress.h>

#include <rmf-messages.h>

#include "rmfd-manager.h"
#include "rmfd-processor.h"
#include "rmfd-error.h"

G_DEFINE_TYPE (RmfdManager, rmfd_manager, G_TYPE_OBJECT)

struct _RmfdManagerPrivate {
    /* The UDev client */
    GUdevClient *udev_client;
    guint initial_scan_id;

    /* QMI and net ports */
    GUdevDevice *qmi;
    GUdevDevice *wwan;

    /* Processor */
    RmfdProcessor *processor;

    /* Unix socket service */
    GSocketService *socket_service;
    GByteArray *socket_buffer;

    /* Pending requests to process */
    GList *requests;
    guint requests_idle_id;
};

/*****************************************************************************/

static gboolean
filter_usb_device (GUdevDevice *device)
{
    const gchar *subsystem;
    const gchar *name;
    const gchar *driver;

    /* Subsystems: 'usb', 'usbmisc' or 'net' */
    subsystem = g_udev_device_get_subsystem (device);
    if (!subsystem || (!g_str_has_prefix (subsystem, "usb") && !g_str_has_prefix (subsystem, "net")))
        return TRUE;

    /* Names: if 'usb' or 'usbmisc' only 'cdc-wdm' prefixed names allowed */
    name = g_udev_device_get_name (device);
    if (!name || (g_str_has_prefix (subsystem, "usb") && !g_str_has_prefix (name, "cdc-wdm")))
        return TRUE;

    /* Drivers: 'qmi_wwan' only */
    driver = g_udev_device_get_driver (device);
    if (!driver) {
        GUdevDevice *parent;

        parent = g_udev_device_get_parent (device);
        if (parent)
            driver = g_udev_device_get_driver (parent);
    }
    if (!driver || !g_str_equal (driver, "qmi_wwan"))
        return TRUE;

    /* Not filtered! */
    return FALSE;
}

static void
processor_new_ready (GObject      *source,
                     GAsyncResult *res,
                     RmfdManager  *self)
{
    GError *error = NULL;

    /* 'self' is a full reference */

    self->priv->processor = rmfd_processor_new_finish (res, &error);
    if (!self->priv->processor) {
        g_warning ("couldn't create processor: %s", error->message);
        g_error_free (error);
    }

    g_object_unref (self);
}

static void
port_added (RmfdManager *self,
            GUdevDevice *device)
{
    const gchar *subsystem;
    const gchar *name;

    /* Filter */
    if (filter_usb_device (device))
        return;

    subsystem = g_udev_device_get_subsystem (device);
    name = g_udev_device_get_name (device);

    /* Store the QMI port */
    if (g_str_has_prefix (subsystem, "usb")) {
        GFile *file;
        gchar *path;

        if (self->priv->qmi) {
            if (!g_str_equal (name, g_udev_device_get_name (self->priv->qmi)))
                g_debug ("Replacing QMI port '%s' with %s",
                         g_udev_device_get_name (self->priv->qmi),
                         name);
            g_clear_object (&self->priv->processor);
            g_clear_object (&self->priv->qmi);
        } else
            g_debug ("QMI port added: /dev/%s", name);

        self->priv->qmi = g_object_ref (device);

        path = g_strdup_printf ("/dev/%s", name);
        file = g_file_new_for_path (path);
        g_free (path);

        /* Create Processor */
        rmfd_processor_new (file,
                            (GAsyncReadyCallback) processor_new_ready,
                            g_object_ref (self));
        return;
    }

    /* Store the net port */
    if (g_str_has_prefix (subsystem, "net")) {
        if (self->priv->wwan) {
            if (!g_str_equal (name, g_udev_device_get_name (self->priv->wwan)))
                g_debug ("Replacing NET port '%s' with %s",
                         name,
                         g_udev_device_get_name (self->priv->wwan));
            g_clear_object (&self->priv->wwan);
        } else
            g_debug ("NET port added: %s", name);

        self->priv->wwan = g_object_ref (device);
        return;
    }
}

static void
port_removed (RmfdManager *self,
              GUdevDevice *device)
{
    /* Remove the QMI port */
    if (self->priv->qmi &&
        g_str_equal (g_udev_device_get_subsystem (device),
                     g_udev_device_get_subsystem (self->priv->qmi)) &&
        g_str_equal (g_udev_device_get_name (device),
                     g_udev_device_get_name (self->priv->qmi))) {
        g_debug ("QMI port removed: /dev/%s", g_udev_device_get_name (self->priv->qmi));
        g_clear_object (&self->priv->processor);
        g_clear_object (&self->priv->qmi);
    }

    /* Remove the net port */
    if (self->priv->wwan &&
        g_str_equal (g_udev_device_get_subsystem (device),
                     g_udev_device_get_subsystem (self->priv->wwan)) &&
        g_str_equal (g_udev_device_get_name (device),
                     g_udev_device_get_name (self->priv->wwan))) {
        g_debug ("NET port removed: %s", g_udev_device_get_name (self->priv->wwan));
        g_clear_object (&self->priv->wwan);
    }
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
    guint8 *message;
} Request;

static void
request_free (Request *request)
{
    g_free (request->message);
    g_output_stream_close (g_io_stream_get_output_stream (G_IO_STREAM (request->connection)), NULL, NULL);
    g_object_unref (request->connection);
    g_free (request);
}

static void
processor_run_ready (RmfdProcessor *processor,
                     GAsyncResult  *result,
                     Request       *request)
{
    const guint8 *response;
    guint8 *response_error = NULL;
    GError *error = NULL;

    response = rmfd_processor_run_finish (processor, result, &error);
    if (!response) {
        g_warning ("error processing the request: %s", error->message);
        response_error = rmfd_error_message_new_from_gerror (request->message, error);
        g_clear_error (&error);
    }

    if (!g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (request->connection)),
                                    response_error ? response_error : response,
                                    rmf_message_get_length (response_error ? response_error : response),
                                    NULL,
                                    NULL, /* cancellable */
                                    &error)) {
        g_warning ("error writing to output stream: %s", error->message);
        g_error_free (error);
    }

    request_free (request);
    g_free (response_error);
}

static void
request_process (RmfdManager *self,
                 Request     *request)
{
    rmfd_processor_run (self->priv->processor,
                        request->message,
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
    gsize bytes_read = 0;
    Request *request;

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
    request = g_new (Request, 1);
    request->connection = g_object_ref (connection);
    request->message = g_malloc (message_size);
    memcpy (&request->message[0], &message_size, 4);

    if (!g_input_stream_read_all (g_io_stream_get_input_stream (G_IO_STREAM (connection)),
                                  &request->message[4],
                                  message_size - 4,
                                  NULL,
                                  NULL, /* cancellable */
                                  &error)) {
        g_warning ("error reading from input stream: %s", error->message);
        g_error_free (error);
        request_free (request);
        return;
    }

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
    g_clear_object (&priv->qmi);
    g_clear_object (&priv->wwan);
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

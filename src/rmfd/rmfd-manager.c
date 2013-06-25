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

#include <rmf-messages.h>

#include "rmfd-manager.h"

G_DEFINE_TYPE (RmfdManager, rmfd_manager, G_TYPE_OBJECT)

struct _RmfdManagerPrivate {
    /* The UDev client */
    GUdevClient *udev_client;
    guint initial_scan_id;

    /* QMI and net ports */
    GUdevDevice *qmi;
    GUdevDevice *wwan;
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
        if (self->priv->qmi) {
            if (!g_str_equal (name, g_udev_device_get_name (self->priv->qmi)))
                g_debug ("Replacing QMI port '%s' with %s",
                         name,
                         g_udev_device_get_name (self->priv->qmi));
            g_clear_object (&self->priv->qmi);
        } else
            g_debug ("QMI port added: /dev/%s", name);

        self->priv->qmi = g_object_ref (device);
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

    /* Setup initial scan */
    self->priv->initial_scan_id = g_idle_add ((GSourceFunc) initial_scan_cb, self);
}

static void
dispose (GObject *object)
{
    RmfdManagerPrivate *priv = RMFD_MANAGER (object)->priv;

    if (priv->initial_scan_id != 0) {
        g_source_remove (priv->initial_scan_id);
        priv->initial_scan_id = 0;
    }

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

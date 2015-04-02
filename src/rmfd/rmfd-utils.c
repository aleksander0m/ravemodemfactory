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

#include <stdlib.h>
#include <string.h>
#include "rmfd-utils.h"

guint8
rmfd_utils_get_mnc_length_for_mcc (const gchar *mcc)
{
    /*
     * Info obtained from the mobile-broadband-provider-info database
     *   https://git.gnome.org/browse/mobile-broadband-provider-info
     */

    switch (atoi (mcc)) {
    case 302: /* Canada */
    case 310: /* United states */
    case 311: /* United states */
    case 338: /* Jamaica */
    case 342: /* Barbados */
    case 358: /* St Lucia */
    case 360: /* St Vincent */
    case 364: /* Bahamas */
    case 405: /* India */
    case 732: /* Colombia */
        return 3;
    default:
        /* For the remaining ones, default to 2 */
        return 2;
    }
}

RmfdModemType
rmfd_utils_get_modem_type (GUdevDevice *device)
{
    RmfdModemType type = RMFD_MODEM_TYPE_UNKNOWN;
    const gchar *subsystem;
    const gchar *name;
    const gchar *driver;
    GUdevDevice *parent = NULL;

    subsystem = g_udev_device_get_subsystem (device);
    name = g_udev_device_get_name (device);
    driver = g_udev_device_get_driver (device);

    /* Get driver, may be coming in parent */
    if (!driver) {
        parent = g_udev_device_get_parent (device);
        if (parent)
            driver = g_udev_device_get_driver (parent);
    }

    /*
     * QMI device?
     *   Subsystems: 'usb', 'usbmisc' or 'net'
     *   Names: if 'usb' or 'usbmisc' only 'cdc-wdm' prefixed names allowed
     */
    if (driver &&
        g_str_equal (driver, "qmi_wwan") &&
        subsystem &&
        (g_str_has_prefix (subsystem, "net") ||
         (g_str_has_prefix (subsystem, "usb") &&
          name && g_str_has_prefix (name, "cdc-wdm")))) {
        type = RMFD_MODEM_TYPE_QMI;
    }

    if (parent)
        g_object_unref (parent);

    return type;
}

gchar *
rmfd_utils_build_interface_name (GUdevDevice *device)
{
    const gchar *subsystem;
    const gchar *name;

    subsystem = g_udev_device_get_subsystem (device);
    name = g_udev_device_get_name (device);

    if (g_str_has_prefix (subsystem, "usb"))
        return g_strdup_printf ("/dev/%s", name);

    if (g_str_has_prefix (subsystem, "net"))
        return g_strdup (name);

    return NULL;
}

/* From ModemManager sources */
GUdevDevice *
rmfd_utils_get_physical_device (GUdevDevice *child)
{
    GUdevDevice *iter, *old = NULL;
    GUdevDevice *physdev = NULL;
    const char *subsys, *type;
    guint32 i = 0;
    gboolean is_usb = FALSE, is_pci = FALSE, is_pcmcia = FALSE, is_platform = FALSE;
    gboolean is_pnp = FALSE;

    g_return_val_if_fail (child != NULL, NULL);

    iter = g_object_ref (child);
    while (iter && i++ < 8) {
        subsys = g_udev_device_get_subsystem (iter);
        if (subsys) {
            if (is_usb || g_str_has_prefix (subsys, "usb")) {
                is_usb = TRUE;
                type = g_udev_device_get_devtype (iter);
                if (type && !strcmp (type, "usb_device")) {
                    physdev = iter;
                    break;
                }
            } else if (is_pcmcia || !strcmp (subsys, "pcmcia")) {
                GUdevDevice *pcmcia_parent;
                const char *tmp_subsys;

                is_pcmcia = TRUE;

                /* If the parent of this PCMCIA device is no longer part of
                 * the PCMCIA subsystem, we want to stop since we're looking
                 * for the base PCMCIA device, not the PCMCIA controller which
                 * is usually PCI or some other bus type.
                 */
                pcmcia_parent = g_udev_device_get_parent (iter);
                if (pcmcia_parent) {
                    tmp_subsys = g_udev_device_get_subsystem (pcmcia_parent);
                    if (tmp_subsys && strcmp (tmp_subsys, "pcmcia"))
                        physdev = iter;
                    g_object_unref (pcmcia_parent);
                    if (physdev)
                        break;
                }
            } else if (is_platform || !strcmp (subsys, "platform")) {
                /* Take the first platform parent as the physical device */
                is_platform = TRUE;
                physdev = iter;
                break;
            } else if (is_pci || !strcmp (subsys, "pci")) {
                is_pci = TRUE;
                physdev = iter;
                break;
            } else if (is_pnp || !strcmp (subsys, "pnp")) {
                is_pnp = TRUE;
                physdev = iter;
                break;
            }
        }

        old = iter;
        iter = g_udev_device_get_parent (old);
        g_object_unref (old);
    }

    return physdev;
}

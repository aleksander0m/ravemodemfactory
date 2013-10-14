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

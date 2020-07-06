/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 * librmf-common tests
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
 * Copyright (C) 2013-2015 Safran Passenger Innovations
 *
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#include <glib.h>

#include <rmf-messages.h>

static void
test_get_manufacturer (void)
{
    uint8_t *message;
    uint32_t status;
    const char *manufacturer;

    message = rmf_message_get_manufacturer_request_new ();
    g_assert (message != NULL);
    g_free (message);

    message = rmf_message_get_manufacturer_response_new ("hello");
    g_assert (message != NULL);
    rmf_message_get_manufacturer_response_parse (message, &status, &manufacturer);
    g_assert_cmpuint (status, ==, RMF_RESPONSE_STATUS_OK);
    g_assert_cmpstr (manufacturer, ==, "hello");

    g_free (message);
}

int main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/librmf-common/message/get-manufacturer", test_get_manufacturer);

    return g_test_run ();
}

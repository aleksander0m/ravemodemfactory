/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 *  librmf-common
 *
 *  Copyright (C) 2013 Zodiac Inflight Innovations
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

    message = rmf_message_get_manufacturer_response_new (25, "hello");
    g_assert (message != NULL);
    rmf_message_get_manufacturer_response_parse (message, &status, &manufacturer);
    g_assert_cmpuint (status, ==, 25);
    g_assert_cmpstr (manufacturer, ==, "hello");

    g_free (message);
}

int main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/librmf-common/message/get-manufacturer", test_get_manufacturer);

    return g_test_run ();
}

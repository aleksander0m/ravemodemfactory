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
 * Copyright (C) 2013 Zodiac Inflight Innovations
 *
 * Author: Aleksander Morgado <aleksander@lanedo.com>
 */

#include <glib.h>

#include <rmf-messages-private.h>

#if defined ENABLE_TEST_MESSAGE_TRACES

static gchar *
str_hex (gconstpointer mem,
         gsize size,
         gchar delimiter)
{
    const guint8 *data = mem;
	gsize i;
	gsize j;
	gsize new_str_length;
	gchar *new_str;

	/* Get new string length. If input string has N bytes, we need:
	 * - 1 byte for last NUL char
	 * - 2N bytes for hexadecimal char representation of each byte...
	 * - N-1 bytes for the separator ':'
	 * So... a total of (1+2N+N-1) = 3N bytes are needed... */
	new_str_length =  3 * size;

	/* Allocate memory for new array and initialize contents to NUL */
	new_str = g_malloc0 (new_str_length);

	/* Print hexadecimal representation of each byte... */
	for (i = 0, j = 0; i < size; i++, j += 3) {
		/* Print character in output string... */
		snprintf (&new_str[j], 3, "%02X", data[i]);
		/* And if needed, add separator */
		if (i != (size - 1) )
			new_str[j + 2] = delimiter;
	}

	/* Set output string */
	return new_str;
}

static void
test_message_trace (const guint8 *computed,
                    guint32       computed_size,
                    const guint8 *expected,
                    guint32       expected_size)
{
    gchar *message_str;
    gchar *expected_str;

    message_str = str_hex (computed, computed_size, ':');
    expected_str = str_hex (expected, expected_size, ':');

    /* Dump all message contents */
    g_print ("\n"
             "Message str:\n"
             "'%s'\n"
             "Expected str:\n"
             "'%s'\n",
             message_str,
             expected_str);

    /* If they are different, tell which are the different bytes */
    if (computed_size != expected_size ||
        memcmp (computed, expected, expected_size)) {
        guint32 i;

        for (i = 0; i < MIN (computed_size, expected_size); i++) {
            if (computed[i] != expected[i])
                g_print ("Byte [%u] is different (computed: 0x%02X vs expected: 0x%02x)\n", i, computed[i], expected[i]);
        }
    }

    g_free (message_str);
    g_free (expected_str);
}
#else
#define test_message_trace(...)
#endif

static void
test_empty (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    static const uint8_t expected[] = {
        0x18, 0x00, 0x00, 0x00, /* length */
        0x01, 0x00, 0x00, 0x00, /* type */
        0x27, 0x00, 0x00, 0x00, /* command */
        0x00, 0x00, 0x00, 0x00, /* status */
        0x00, 0x00, 0x00, 0x00, /* fixed_size */
        0x00, 0x00, 0x00, 0x00, /* variable_size */
    };

    /* Check builder */
    builder = rmf_message_builder_new (1, 39, 0);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    test_message_trace (message, RMF_MESSAGE_LENGTH (message),
                        expected, sizeof (expected));

    /* Check byte stream */
    g_assert (!memcmp (message, expected, sizeof (expected)));

    /* Check getters */
    g_assert_cmpuint (RMF_MESSAGE_LENGTH      (message), ==, 24);
    g_assert_cmpuint (rmf_message_get_type    (message), ==, 1);
    g_assert_cmpuint (rmf_message_get_command (message), ==, 39);
    g_assert_cmpuint (rmf_message_get_status  (message), ==, 0);

    g_free (message);
}

static void
test_integers32_one (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;
    uint32_t walker = 0;

    static const uint8_t expected[] = {
        0x1C, 0x00, 0x00, 0x00, /* length */
        0x01, 0x00, 0x00, 0x00, /* type */
        0x27, 0x00, 0x00, 0x00, /* command */
        0x00, 0x00, 0x00, 0x00, /* status */
        0x04, 0x00, 0x00, 0x00, /* fixed_size */
        0x00, 0x00, 0x00, 0x00, /* variable_size */
        /* fixed */
        0x07, 0x00, 0x00, 0x00, /* integer 1 */
    };

    /* Check builder */
    builder = rmf_message_builder_new (1, 39, 0);
    rmf_message_builder_add_uint32 (builder, 7);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    test_message_trace (message, RMF_MESSAGE_LENGTH (message),
                        expected, sizeof (expected));

    /* Check byte stream */
    g_assert (!memcmp (message, expected, sizeof (expected)));

    /* Check getters */
    g_assert_cmpuint (RMF_MESSAGE_LENGTH      (message), ==, 28);
    g_assert_cmpuint (rmf_message_get_type    (message), ==, 1);
    g_assert_cmpuint (rmf_message_get_command (message), ==, 39);
    g_assert_cmpuint (rmf_message_get_status  (message), ==, 0);
    g_assert_cmpuint (rmf_message_read_uint32 (message, &walker), ==, 7);

    g_free (message);
}

static void
test_integers32_multiple (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;
    uint32_t walker = 0;

    static const uint8_t expected[] = {
        0x24, 0x00, 0x00, 0x00, /* length */
        0x01, 0x00, 0x00, 0x00, /* type */
        0x27, 0x00, 0x00, 0x00, /* command */
        0x00, 0x00, 0x00, 0x00, /* status */
        0x0C, 0x00, 0x00, 0x00, /* fixed_size */
        0x00, 0x00, 0x00, 0x00, /* variable_size */
        /* fixed */
        0x01, 0x00, 0x00, 0x00, /* integer 1 */
        0x02, 0x00, 0x00, 0x00, /* integer 2 */
        0x03, 0x00, 0x00, 0x00  /* integer 3 */
    };

    /* Check builder */
    builder = rmf_message_builder_new (1, 39, 0);
    rmf_message_builder_add_uint32 (builder, 1);
    rmf_message_builder_add_uint32 (builder, 2);
    rmf_message_builder_add_uint32 (builder, 3);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    test_message_trace (message, RMF_MESSAGE_LENGTH (message),
                        expected, sizeof (expected));

    /* Check byte stream */
    g_assert (!memcmp (message, expected, sizeof (expected)));

    /* Check getters */
    g_assert_cmpuint (RMF_MESSAGE_LENGTH      (message), ==, 36);
    g_assert_cmpuint (rmf_message_get_type    (message), ==, 1);
    g_assert_cmpuint (rmf_message_get_command (message), ==, 39);
    g_assert_cmpuint (rmf_message_get_status  (message), ==, 0);
    g_assert_cmpuint (rmf_message_read_uint32 (message, &walker), ==, 1);
    g_assert_cmpuint (rmf_message_read_uint32 (message, &walker), ==, 2);
    g_assert_cmpuint (rmf_message_read_uint32 (message, &walker), ==, 3);

    g_free (message);
}

static void
test_integers64_one (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;
    uint32_t walker = 0;

    static const uint8_t expected[] = {
        0x20, 0x00, 0x00, 0x00, /* length */
        0x01, 0x00, 0x00, 0x00, /* type */
        0x27, 0x00, 0x00, 0x00, /* command */
        0x00, 0x00, 0x00, 0x00, /* status */
        0x08, 0x00, 0x00, 0x00, /* fixed_size */
        0x00, 0x00, 0x00, 0x00, /* variable_size */
        /* fixed */
        0x07, 0x00, 0x00, 0x00, /* integer 1 */
        0x00, 0x00, 0x00, 0x00
    };

    /* Check builder */
    builder = rmf_message_builder_new (1, 39, 0);
    rmf_message_builder_add_uint64 (builder, 7);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    test_message_trace (message, RMF_MESSAGE_LENGTH (message),
                        expected, sizeof (expected));

    /* Check byte stream */
    g_assert (!memcmp (message, expected, sizeof (expected)));

    /* Check getters */
    g_assert_cmpuint (RMF_MESSAGE_LENGTH      (message), ==, 32);
    g_assert_cmpuint (rmf_message_get_type    (message), ==, 1);
    g_assert_cmpuint (rmf_message_get_command (message), ==, 39);
    g_assert_cmpuint (rmf_message_get_status  (message), ==, 0);
    g_assert_cmpuint (rmf_message_read_uint64 (message, &walker), ==, 7);

    g_free (message);
}

static void
test_integers64_multiple (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;
    uint32_t walker = 0;

    static const uint8_t expected[] = {
        0x30, 0x00, 0x00, 0x00, /* length */
        0x01, 0x00, 0x00, 0x00, /* type */
        0x27, 0x00, 0x00, 0x00, /* command */
        0x00, 0x00, 0x00, 0x00, /* status */
        0x18, 0x00, 0x00, 0x00, /* fixed_size */
        0x00, 0x00, 0x00, 0x00, /* variable_size */
        /* fixed */
        0x01, 0x00, 0x00, 0x00, /* integer 1 */
        0x00, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00, /* integer 2 */
        0x00, 0x00, 0x00, 0x00,
        0x03, 0x00, 0x00, 0x00, /* integer 3 */
        0x00, 0x00, 0x00, 0x00
    };

    /* Check builder */
    builder = rmf_message_builder_new (1, 39, 0);
    rmf_message_builder_add_uint64 (builder, 1);
    rmf_message_builder_add_uint64 (builder, 2);
    rmf_message_builder_add_uint64 (builder, 3);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    test_message_trace (message, RMF_MESSAGE_LENGTH (message),
                        expected, sizeof (expected));

    /* Check byte stream */
    g_assert (!memcmp (message, expected, sizeof (expected)));

    /* Check getters */
    g_assert_cmpuint (RMF_MESSAGE_LENGTH      (message), ==, 48);
    g_assert_cmpuint (rmf_message_get_type    (message), ==, 1);
    g_assert_cmpuint (rmf_message_get_command (message), ==, 39);
    g_assert_cmpuint (rmf_message_get_status  (message), ==, 0);
    g_assert_cmpuint (rmf_message_read_uint64 (message, &walker), ==, 1);
    g_assert_cmpuint (rmf_message_read_uint64 (message, &walker), ==, 2);
    g_assert_cmpuint (rmf_message_read_uint64 (message, &walker), ==, 3);

    g_free (message);
}

static void
test_strings_one (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;
    uint32_t walker = 0;

    static const uint8_t expected[] = {
        0x28, 0x00, 0x00, 0x00, /* length */
        0x01, 0x00, 0x00, 0x00, /* type */
        0x27, 0x00, 0x00, 0x00, /* command */
        0x00, 0x00, 0x00, 0x00, /* status */
        0x08, 0x00, 0x00, 0x00, /* fixed_size */
        0x08, 0x00, 0x00, 0x00, /* variable_size */
        /* fixed */
        0x00, 0x00, 0x00, 0x00, /* string 1 offset */
        0x06, 0x00, 0x00, 0x00, /* string 1 len */
        /* variable */
        'h',  'e',  'l',  'l', /* string 1 */
        'o',  '\0', 0x00, 0x00
    };

    /* Check builder */
    builder = rmf_message_builder_new (1, 39, 0);
    rmf_message_builder_add_string (builder, "hello");
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    test_message_trace (message, RMF_MESSAGE_LENGTH (message),
                        expected, sizeof (expected));

    /* Check byte stream */
    g_assert (!memcmp (message, expected, sizeof (expected)));

    /* Check getters */
    g_assert_cmpuint (RMF_MESSAGE_LENGTH      (message), ==, 40);
    g_assert_cmpuint (rmf_message_get_type    (message), ==, 1);
    g_assert_cmpuint (rmf_message_get_command (message), ==, 39);
    g_assert_cmpuint (rmf_message_get_status  (message), ==, 0);
    g_assert_cmpstr  (rmf_message_read_string (message, &walker), ==, "hello");

    g_free (message);
}

static void
test_strings_multiple (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;
    uint32_t walker = 0;

    static const uint8_t expected[] = {
        0x68, 0x00, 0x00, 0x00, /* length */
        0x01, 0x00, 0x00, 0x00, /* type */
        0x27, 0x00, 0x00, 0x00, /* command */
        0x00, 0x00, 0x00, 0x00, /* status */
        0x30, 0x00, 0x00, 0x00, /* fixed_size */
        0x20, 0x00, 0x00, 0x00, /* variable_size */
        /* fixed */
        0x00, 0x00, 0x00, 0x00, /* string 1 offset */
        0x02, 0x00, 0x00, 0x00, /* string 1 len */
        0x04, 0x00, 0x00, 0x00, /* string 2 offset */
        0x03, 0x00, 0x00, 0x00, /* string 2 len */
        0x08, 0x00, 0x00, 0x00, /* string 3 offset */
        0x04, 0x00, 0x00, 0x00, /* string 3 len */
        0x0C, 0x00, 0x00, 0x00, /* string 4 offset */
        0x05, 0x00, 0x00, 0x00, /* string 4 len */
        0x14, 0x00, 0x00, 0x00, /* string 5 offset */
        0x06, 0x00, 0x00, 0x00, /* string 5 len */
        0x1C, 0x00, 0x00, 0x00, /* string 6 offset */
        0x01, 0x00, 0x00, 0x00, /* string 6 len */
        /* variable */
        'h',  '\0', 0x00, 0x00, /* string 1 */
        'h',  'e',  '\0', 0x00, /* string 2 */
        'h',  'e',  'l',  '\0', /* string 3 */
        'h',  'e',  'l',  'l',  /* string 4 */
        '\0', 0x00, 0x00, 0x00,
        'h',  'e',  'l',  'l',  /* string 5 */
        'o',  '\0', 0x00, 0x00,
        '\0', 0x00, 0x00, 0x00  /* string 6 */
    };

    /* Check builder */
    builder = rmf_message_builder_new (1, 39, 0);
    rmf_message_builder_add_string (builder, "h");
    rmf_message_builder_add_string (builder, "he");
    rmf_message_builder_add_string (builder, "hel");
    rmf_message_builder_add_string (builder, "hell");
    rmf_message_builder_add_string (builder, "hello");
    rmf_message_builder_add_string (builder, "");
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    test_message_trace (message, RMF_MESSAGE_LENGTH (message),
                        expected, sizeof (expected));

    /* Check byte stream */
    g_assert (!memcmp (message, expected, sizeof (expected)));

    /* Check getters */
    g_assert_cmpuint (RMF_MESSAGE_LENGTH      (message), ==, 104);
    g_assert_cmpuint (rmf_message_get_type    (message), ==, 1);
    g_assert_cmpuint (rmf_message_get_command (message), ==, 39);
    g_assert_cmpuint (rmf_message_get_status  (message), ==, 0);
    g_assert_cmpstr  (rmf_message_read_string (message, &walker), ==, "h");
    g_assert_cmpstr  (rmf_message_read_string (message, &walker), ==, "he");
    g_assert_cmpstr  (rmf_message_read_string (message, &walker), ==, "hel");
    g_assert_cmpstr  (rmf_message_read_string (message, &walker), ==, "hell");
    g_assert_cmpstr  (rmf_message_read_string (message, &walker), ==, "hello");
    g_assert_cmpstr  (rmf_message_read_string (message, &walker), ==, "");

    g_free (message);
}

static void
test_mixed (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;
    uint32_t walker = 0;

    static const uint8_t expected[] = {
        0x4C, 0x00, 0x00, 0x00, /* length */
        0x01, 0x00, 0x00, 0x00, /* type */
        0x27, 0x00, 0x00, 0x00, /* command */
        0x00, 0x00, 0x00, 0x00, /* status */
        0x24, 0x00, 0x00, 0x00, /* fixed_size */
        0x10, 0x00, 0x00, 0x00, /* variable_size */
        /* fixed */
        0x00, 0x00, 0x00, 0x00, /* string 1 offset */
        0x06, 0x00, 0x00, 0x00, /* string 1 len */
        0x07, 0x00, 0x00, 0x00, /* number 1 */
        0x08, 0x00, 0x00, 0x00, /* number 2 */
        0x00, 0x00, 0x00, 0x00,
        0x09, 0x00, 0x00, 0x00, /* number 3 */
        0x08, 0x00, 0x00, 0x00, /* string 2 offset */
        0x06, 0x00, 0x00, 0x00, /* string 2 len */
        0x00, 0x00, 0x00, 0x00, /* number 4 */
        /* variable */
        'h',  'e',  'l',  'l',  /* string 1 */
        'o',  '\0', 0x00, 0x00,
        'w',  'o',  'r',  'l',  /* string 2 */
        'd',  '\0', 0x00, 0x00,
    };

    /* Check builder */
    builder = rmf_message_builder_new (1, 39, 0);
    rmf_message_builder_add_string (builder, "hello");
    rmf_message_builder_add_uint32 (builder, 7);
    rmf_message_builder_add_uint64 (builder, 8);
    rmf_message_builder_add_uint32 (builder, 9);
    rmf_message_builder_add_string (builder, "world");
    rmf_message_builder_add_uint32 (builder, 0);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    test_message_trace (message, RMF_MESSAGE_LENGTH (message),
                        expected, sizeof (expected));

    /* Check byte stream */
    g_assert (!memcmp (message, expected, sizeof (expected)));

    /* Check getters */
    g_assert_cmpuint (RMF_MESSAGE_LENGTH      (message), ==, 76);
    g_assert_cmpuint (rmf_message_get_type    (message), ==, 1);
    g_assert_cmpuint (rmf_message_get_command (message), ==, 39);
    g_assert_cmpuint (rmf_message_get_status  (message), ==, 0);
    g_assert_cmpstr  (rmf_message_read_string (message, &walker), ==, "hello");
    g_assert_cmpuint (rmf_message_read_uint32 (message, &walker), ==, 7);
    g_assert_cmpuint (rmf_message_read_uint64 (message, &walker), ==, 8);
    g_assert_cmpuint (rmf_message_read_uint32 (message, &walker), ==, 9);
    g_assert_cmpstr  (rmf_message_read_string (message, &walker), ==, "world");
    g_assert_cmpuint (rmf_message_read_uint32 (message, &walker), ==, 0);

    g_free (message);
}

int main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/librmf-common/message-private/empty", test_empty);
    g_test_add_func ("/librmf-common/message-private/integers32/one", test_integers32_one);
    g_test_add_func ("/librmf-common/message-private/integers32/multiple", test_integers32_multiple);
    g_test_add_func ("/librmf-common/message-private/integers64/one", test_integers64_one);
    g_test_add_func ("/librmf-common/message-private/integers64/multiple", test_integers64_multiple);
    g_test_add_func ("/librmf-common/message-private/strings/one", test_strings_one);
    g_test_add_func ("/librmf-common/message-private/strings/multiple", test_strings_multiple);
    g_test_add_func ("/librmf-common/message-private/mixed", test_mixed);

    return g_test_run ();
}

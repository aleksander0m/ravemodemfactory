/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details:
 *
 *
 * Charset support based on ModemManager:
 *   Copyright (C) 2010 Red Hat, Inc.
 *
 * Copyright (C) 2015 Zodiac Inflight Innovations
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#ifndef RMFD_CHARSETS_H
#define RMFD_CHARSETS_H

#include <glib.h>

typedef enum {
    RMFD_MODEM_CHARSET_UNKNOWN = 0x00000000,
    RMFD_MODEM_CHARSET_GSM     = 0x00000001,
    RMFD_MODEM_CHARSET_IRA     = 0x00000002,
    RMFD_MODEM_CHARSET_8859_1  = 0x00000004,
    RMFD_MODEM_CHARSET_UTF8    = 0x00000008,
    RMFD_MODEM_CHARSET_UCS2    = 0x00000010,
    RMFD_MODEM_CHARSET_PCCP437 = 0x00000020,
    RMFD_MODEM_CHARSET_PCDN    = 0x00000040,
    RMFD_MODEM_CHARSET_HEX     = 0x00000080
} RmfdModemCharset;

const char *rmfd_modem_charset_to_string (RmfdModemCharset charset);

RmfdModemCharset rmfd_modem_charset_from_string (const char *string);

/* Append the given string to the given byte array but re-encode it
 * into the given charset first.  The original string is assumed to be
 * UTF-8 encoded.
 */
gboolean rmfd_modem_charset_byte_array_append (GByteArray *array,
                                               const char *utf8,
                                               gboolean quoted,
                                               RmfdModemCharset charset);

/* Take a string in hex representation ("00430052" or "A4BE11" for example)
 * and convert it from the given character set to UTF-8.
 */
char *rmfd_modem_charset_hex_to_utf8 (const char *src, RmfdModemCharset charset);

/* Take a string in UTF-8 and convert it to the given charset in hex
 * representation.
 */
char *rmfd_modem_charset_utf8_to_hex (const char *src, RmfdModemCharset charset);

guint8 *rmfd_charset_utf8_to_unpacked_gsm (const char *utf8, guint32 *out_len);

guint8 *rmfd_charset_gsm_unpacked_to_utf8 (const guint8 *gsm, guint32 len);

/* Returns the size in bytes required to hold the UTF-8 string in the given charset */
guint rmfd_charset_get_encoded_len (const char *utf8,
                                    RmfdModemCharset charset,
                                    guint *out_unsupported);

guint8 *gsm_unpack (const guint8 *gsm,
                    guint32 num_septets,
                    guint8 start_offset,  /* in bits */
                    guint32 *out_unpacked_len);

guint8 *gsm_pack (const guint8 *src,
                  guint32 src_len,
                  guint8 start_offset,  /* in bits */
                  guint32 *out_packed_len);

gchar *rmfd_charset_take_and_convert_to_utf8 (gchar *str, RmfdModemCharset charset);

gchar *rmfd_utf8_take_and_convert_to_charset (gchar *str, RmfdModemCharset charset);

gchar *rmfd_utils_hexstr2bin (const gchar *hex, gsize *out_len);
gchar *rmfd_utils_bin2hexstr (const guint8 *bin, gsize len);

#endif /* RMFD_CHARSETS_H */

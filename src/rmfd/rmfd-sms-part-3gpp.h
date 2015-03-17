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
 * SMS part support based on ModemManager:
 *   Copyright (C) 2010 - 2012 Red Hat, Inc.
 *   Copyright (C) 2012 Google, Inc.
 *
 * Copyright (C) 2015 Zodiac Inflight Innovations
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#ifndef RMFD_SMS_PART_3GPP_H
#define RMFD_SMS_PART_3GPP_H

#include <glib.h>

#include "rmfd-sms-part.h"

#define RMFD_SMS_PART_3GPP_MAX_PDU_LEN 344

RmfdSmsPart *rmfd_sms_part_3gpp_new_from_pdu  (guint index,
                                               const gchar *hexpdu,
                                               GError **error);

RmfdSmsPart *rmfd_sms_part_3gpp_new_from_binary_pdu (guint index,
                                                     const guint8 *pdu,
                                                     gsize pdu_len,
                                                     GError **error);

guint8    *rmfd_sms_part_3gpp_get_submit_pdu (RmfdSmsPart *part,
                                              guint *out_pdulen,
                                              guint *out_msgstart,
                                              GError **error);

guint rmfd_sms_part_3gpp_encode_address (const gchar *address,
                                         guint8 *buf,
                                         gsize buflen,
                                         gboolean is_smsc);

gchar **rmfd_sms_part_3gpp_util_split_text (const gchar *text,
                                            RmfdSmsEncoding *encoding);

GByteArray **rmfd_sms_part_3gpp_util_split_data (const guint8 *data,
                                                 gsize data_len);

#endif /* RMFD_SMS_PART_3GPP_H */

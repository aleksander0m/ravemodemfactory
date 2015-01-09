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
 * SMS support based on ModemManager:
 *   Copyright (C) 2010 - 2012 Red Hat, Inc.
 *   Copyright (C) 2012 Google, Inc.
 *
 * Copyright (C) 2015 Zodiac Inflight Innovations
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#ifndef RMFD_SMS_H
#define RMFD_SMS_H

#include <glib.h>
#include <libqmi-glib.h>

#include "rmfd-sms-part.h"

typedef struct _RmfdSms RmfdSms;

RmfdSms  *rmfd_sms_singlepart_new         (QmiWmsStorageType   storage,
                                           RmfdSmsPart        *part,
                                           GError            **error);
RmfdSms  *rmfd_sms_multipart_new          (QmiWmsStorageType   storage,
                                           guint               reference,
                                           guint               max_parts,
                                           RmfdSmsPart        *first_part,
                                           GError            **error);
gboolean  rmfd_sms_multipart_take_part    (RmfdSms            *self,
                                           RmfdSmsPart        *part,
                                           GError            **error);
gboolean  rmfd_sms_multipart_is_complete  (RmfdSms            *self);
gboolean  rmfd_sms_multipart_is_assembled (RmfdSms            *self);

#endif /* RMFD_SMS_H */

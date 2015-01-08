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
 * SMS part support based on ModemManager:
 *   Copyright (C) 2010 - 2012 Red Hat, Inc.
 *   Copyright (C) 2012 Google, Inc.
 *
 * Copyright (C) 2015 Zodiac Inflight Innovations
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#ifndef RMFD_SMS_PART_H
#define RMFD_SMS_PART_H

#include <glib.h>

typedef enum {
    RMFD_SMS_PDU_TYPE_UNKNOWN,
    RMFD_SMS_PDU_TYPE_DELIVER,
    RMFD_SMS_PDU_TYPE_SUBMIT,
    RMFD_SMS_PDU_TYPE_STATUS_REPORT,
} RmfdSmsPduType;

typedef enum {
    RMFD_SMS_ENCODING_UNKNOWN,
    RMFD_SMS_ENCODING_GSM7,
    RMFD_SMS_ENCODING_8BIT,
    RMFD_SMS_ENCODING_UCS2
} RmfdSmsEncoding;

typedef enum {
    /* --------------- 3GPP specific errors ---------------------- */
    /* Completed deliveries */
    RMFD_SMS_DELIVERY_STATE_COMPLETED_RECEIVED              = 0x00,
    RMFD_SMS_DELIVERY_STATE_COMPLETED_FORWARDED_UNCONFIRMED = 0x01,
    RMFD_SMS_DELIVERY_STATE_COMPLETED_REPLACED_BY_SC        = 0x02,
    /* Temporary failures */
    RMFD_SMS_DELIVERY_STATE_TEMPORARY_ERROR_CONGESTION           = 0x20,
    RMFD_SMS_DELIVERY_STATE_TEMPORARY_ERROR_SME_BUSY             = 0x21,
    RMFD_SMS_DELIVERY_STATE_TEMPORARY_ERROR_NO_RESPONSE_FROM_SME = 0x22,
    RMFD_SMS_DELIVERY_STATE_TEMPORARY_ERROR_SERVICE_REJECTED     = 0x23,
    RMFD_SMS_DELIVERY_STATE_TEMPORARY_ERROR_QOS_NOT_AVAILABLE    = 0x24,
    RMFD_SMS_DELIVERY_STATE_TEMPORARY_ERROR_IN_SME               = 0x25,
    /* Permanent failures */
    RMFD_SMS_DELIVERY_STATE_ERROR_REMOTE_PROCEDURE             = 0x40,
    RMFD_SMS_DELIVERY_STATE_ERROR_INCOMPATIBLE_DESTINATION     = 0x41,
    RMFD_SMS_DELIVERY_STATE_ERROR_CONNECTION_REJECTED          = 0x42,
    RMFD_SMS_DELIVERY_STATE_ERROR_NOT_OBTAINABLE               = 0x43,
    RMFD_SMS_DELIVERY_STATE_ERROR_QOS_NOT_AVAILABLE            = 0x44,
    RMFD_SMS_DELIVERY_STATE_ERROR_NO_INTERWORKING_AVAILABLE    = 0x45,
    RMFD_SMS_DELIVERY_STATE_ERROR_VALIDITY_PERIOD_EXPIRED      = 0x46,
    RMFD_SMS_DELIVERY_STATE_ERROR_DELETED_BY_ORIGINATING_SME   = 0x47,
    RMFD_SMS_DELIVERY_STATE_ERROR_DELETED_BY_SC_ADMINISTRATION = 0x48,
    RMFD_SMS_DELIVERY_STATE_ERROR_MESSAGE_DOES_NOT_EXIST       = 0x49,
    /* Temporary failures that became permanent */
    RMFD_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_CONGESTION           = 0x60,
    RMFD_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_SME_BUSY             = 0x61,
    RMFD_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_NO_RESPONSE_FROM_SME = 0x62,
    RMFD_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_SERVICE_REJECTED     = 0x63,
    RMFD_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_QOS_NOT_AVAILABLE    = 0x64,
    RMFD_SMS_DELIVERY_STATE_TEMPORARY_FATAL_ERROR_IN_SME               = 0x65,
    /* Unknown, out of any possible valid value [0x00-0xFF] */
    RMFD_SMS_DELIVERY_STATE_UNKNOWN = 0x100,
} RmfdSmsDeliveryState;

typedef struct _RmfdSmsPart RmfdSmsPart;

#define SMS_PART_INVALID_INDEX G_MAXUINT

RmfdSmsPart *rmfd_sms_part_new  (guint index,
                                 RmfdSmsPduType type);
void         rmfd_sms_part_free (RmfdSmsPart *part);

guint             rmfd_sms_part_get_index              (RmfdSmsPart *part);
void              rmfd_sms_part_set_index              (RmfdSmsPart *part,
                                                        guint index);

RmfdSmsPduType    rmfd_sms_part_get_pdu_type           (RmfdSmsPart *part);
void              rmfd_sms_part_set_pdu_type           (RmfdSmsPart *part,
                                                        RmfdSmsPduType type);

const gchar      *rmfd_sms_part_get_smsc               (RmfdSmsPart *part);
void              rmfd_sms_part_set_smsc               (RmfdSmsPart *part,
                                                        const gchar *smsc);
void              rmfd_sms_part_take_smsc              (RmfdSmsPart *part,
                                                        gchar *smsc);

const gchar      *rmfd_sms_part_get_number             (RmfdSmsPart *part);
void              rmfd_sms_part_set_number             (RmfdSmsPart *part,
                                                        const gchar *number);
void              rmfd_sms_part_take_number            (RmfdSmsPart *part,
                                                        gchar *number);

const gchar      *rmfd_sms_part_get_timestamp          (RmfdSmsPart *part);
void              rmfd_sms_part_set_timestamp          (RmfdSmsPart *part,
                                                        const gchar *timestamp);
void              rmfd_sms_part_take_timestamp         (RmfdSmsPart *part,
                                                        gchar *timestamp);

const gchar      *rmfd_sms_part_get_discharge_timestamp  (RmfdSmsPart *part);
void              rmfd_sms_part_set_discharge_timestamp  (RmfdSmsPart *part,
                                                          const gchar *timestamp);
void              rmfd_sms_part_take_discharge_timestamp (RmfdSmsPart *part,
                                                          gchar *timestamp);

const gchar      *rmfd_sms_part_get_text               (RmfdSmsPart *part);
void              rmfd_sms_part_set_text               (RmfdSmsPart *part,
                                                        const gchar *text);
void              rmfd_sms_part_take_text              (RmfdSmsPart *part,
                                                        gchar *text);

const GByteArray *rmfd_sms_part_get_data               (RmfdSmsPart *part);
void              rmfd_sms_part_set_data               (RmfdSmsPart *part,
                                                        GByteArray *data);
void              rmfd_sms_part_take_data              (RmfdSmsPart *part,
                                                        GByteArray *data);

RmfdSmsEncoding   rmfd_sms_part_get_encoding           (RmfdSmsPart *part);
void              rmfd_sms_part_set_encoding           (RmfdSmsPart *part,
                                                        RmfdSmsEncoding encoding);

gint              rmfd_sms_part_get_class              (RmfdSmsPart *part);
void              rmfd_sms_part_set_class              (RmfdSmsPart *part,
                                                        gint class);

guint             rmfd_sms_part_get_validity_relative  (RmfdSmsPart *part);
void              rmfd_sms_part_set_validity_relative  (RmfdSmsPart *part,
                                                        guint validity);

guint             rmfd_sms_part_get_delivery_state     (RmfdSmsPart *part);
void              rmfd_sms_part_set_delivery_state     (RmfdSmsPart *part,
                                                        guint delivery_state);

guint             rmfd_sms_part_get_message_reference  (RmfdSmsPart *part);
void              rmfd_sms_part_set_message_reference  (RmfdSmsPart *part,
                                                        guint message_reference);

gboolean          rmfd_sms_part_get_delivery_report_request (RmfdSmsPart *part);
void              rmfd_sms_part_set_delivery_report_request (RmfdSmsPart *part,
                                                             gboolean delivery_report_request);

guint             rmfd_sms_part_get_concat_reference   (RmfdSmsPart *part);
void              rmfd_sms_part_set_concat_reference   (RmfdSmsPart *part,
                                                        guint concat_reference);

guint             rmfd_sms_part_get_concat_max         (RmfdSmsPart *part);
void              rmfd_sms_part_set_concat_max         (RmfdSmsPart *part,
                                                        guint concat_max);
guint             rmfd_sms_part_get_concat_sequence    (RmfdSmsPart *part);
void              rmfd_sms_part_set_concat_sequence    (RmfdSmsPart *part,
                                                        guint concat_sequence);

gboolean          rmfd_sms_part_should_concat          (RmfdSmsPart *part);

#endif /* RMFD_SMS_PART_H */

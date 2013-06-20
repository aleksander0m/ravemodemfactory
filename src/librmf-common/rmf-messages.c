/* -*- Mode: c; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 *  librmf-common
 *
 *  Copyright (C) 2013 Zodiac Inflight Innovations
 */

#include <assert.h>

#include <rmf-messages.h>
#include <rmf-messages-private.h>

/******************************************************************************/
/* Common */

enum RmfMessageType {
    RMF_MESSAGE_TYPE_UNKNOWN  = 0,
    RMF_MESSAGE_TYPE_REQUEST  = 1,
    RMF_MESSAGE_TYPE_RESPONSE = 2
};

enum RmfMessageCommand {
    RMF_MESSAGE_COMMAND_UNKNOWN                 = 0,
    RMF_MESSAGE_COMMAND_GET_MANUFACTURER        = 1,
    RMF_MESSAGE_COMMAND_GET_MODEL               = 2,
    RMF_MESSAGE_COMMAND_GET_SOFTWARE_REVISION   = 3,
    RMF_MESSAGE_COMMAND_GET_HARDWARE_REVISION   = 4,
    RMF_MESSAGE_COMMAND_GET_IMEI                = 5,
    RMF_MESSAGE_COMMAND_GET_IMSI                = 6,
    RMF_MESSAGE_COMMAND_GET_ICCID               = 7,
    RMF_MESSAGE_COMMAND_UNLOCK                  = 8,
    RMF_MESSAGE_COMMAND_GET_POWER_STATUS        = 9,
    RMF_MESSAGE_COMMAND_SET_POWER_STATUS        = 10,
    RMF_MESSAGE_COMMAND_GET_POWER_INFO          = 11,
    RMF_MESSAGE_COMMAND_GET_SIGNAL_INFO         = 12,
    RMF_MESSAGE_COMMAND_GET_REGISTRATION_STATUS = 13,
    RMF_MESSAGE_COMMAND_GET_CONNECTION_STATUS   = 14,
    RMF_MESSAGE_COMMAND_GET_CONNECTION_STATS    = 15,
    RMF_MESSAGE_COMMAND_CONNECT                 = 16,
    RMF_MESSAGE_COMMAND_DISCONNECT              = 17,
};

uint32_t
rmf_message_get_length (const uint8_t *message)
{
    return RMF_MESSAGE_LENGTH (message);
}

/******************************************************************************/
/* Get Manufacturer
 *
 *  Request:
 *    - no arguments
 *  Response:
 *    - string
 */

uint8_t *
rmf_message_get_manufacturer_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_MANUFACTURER, 0);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_manufacturer_response_new (uint32_t    status,
                                           const char *manufacturer)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_MANUFACTURER, status);
    rmf_message_builder_add_string (builder, manufacturer);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_get_manufacturer_response_parse (const uint8_t  *message,
                                             uint32_t       *status,
                                             const char    **manufacturer)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_GET_MANUFACTURER);

    if (status)
        *status = rmf_message_get_status (message);
    if (manufacturer)
        *manufacturer = rmf_message_read_string (message, &offset);
}

/******************************************************************************/
/* Get Model */

uint8_t *
rmf_message_get_model_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_MODEL, 0);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_model_response_new (uint32_t    status,
                                    const char *model)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_MODEL, status);
    rmf_message_builder_add_string (builder, model);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_get_model_response_parse (const uint8_t  *message,
                                      uint32_t       *status,
                                      const char    **model)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_GET_MODEL);

    if (status)
        *status = rmf_message_get_status (message);
    if (model)
        *model = rmf_message_read_string (message, &offset);
}

/******************************************************************************/
/* Get Software Revision */

uint8_t *
rmf_message_get_software_revision_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_SOFTWARE_REVISION, 0);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_software_revision_response_new (uint32_t    status,
                                                const char *software_revision)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_SOFTWARE_REVISION, status);
    rmf_message_builder_add_string (builder, software_revision);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_get_software_revision_response_parse (const uint8_t  *message,
                                                  uint32_t       *status,
                                                  const char    **software_revision)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_GET_SOFTWARE_REVISION);

    if (status)
        *status = rmf_message_get_status (message);
    if (software_revision)
        *software_revision = rmf_message_read_string (message, &offset);
}

/******************************************************************************/
/* Get Hardware Revision */

uint8_t *
rmf_message_get_hardware_revision_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_HARDWARE_REVISION, 0);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_hardware_revision_response_new (uint32_t        status,
                                                const char     *hardware_revision)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_HARDWARE_REVISION, status);
    rmf_message_builder_add_string (builder, hardware_revision);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_get_hardware_revision_response_parse (const uint8_t  *message,
                                                  uint32_t       *status,
                                                  const char    **hardware_revision)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_GET_HARDWARE_REVISION);

    if (status)
        *status = rmf_message_get_status (message);
    if (hardware_revision)
        *hardware_revision = rmf_message_read_string (message, &offset);
}

/******************************************************************************/
/* Get IMEI */

uint8_t *
rmf_message_get_imei_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_IMEI, 0);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_imei_response_new (uint32_t     status,
                                   const char *imei)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_IMEI, status);
    rmf_message_builder_add_string (builder, imei);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_get_imei_response_parse (const uint8_t  *message,
                                     uint32_t       *status,
                                     const char    **imei)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_GET_IMEI);

    if (status)
        *status = rmf_message_get_status (message);
    if (imei)
        *imei = rmf_message_read_string (message, &offset);
}

/******************************************************************************/
/* Get IMSI */

uint8_t *
rmf_message_get_imsi_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_IMSI, 0);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_imsi_response_new (uint32_t    status,
                                   const char *imsi)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_IMSI, status);
    rmf_message_builder_add_string (builder, imsi);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_get_imsi_response_parse (const uint8_t  *message,
                                     uint32_t       *status,
                                     const char    **imsi)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_GET_IMSI);

    if (status)
        *status = rmf_message_get_status (message);
    if (imsi)
        *imsi = rmf_message_read_string (message, &offset);
}

/******************************************************************************/
/* Get ICCID */

uint8_t *
rmf_message_get_iccid_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_ICCID, 0);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_iccid_response_new (uint32_t    status,
                                    const char *iccid)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_ICCID, status);
    rmf_message_builder_add_string (builder, iccid);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_get_iccid_response_parse (const uint8_t  *message,
                                      uint32_t       *status,
                                      const char    **iccid)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_GET_ICCID);

    if (status)
        *status = rmf_message_get_status (message);
    if (iccid)
        *iccid = rmf_message_read_string (message, &offset);
}

/******************************************************************************/
/* Unlock */

uint8_t *
rmf_message_unlock_request_new (const char *pin)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_UNLOCK, 0);
    rmf_message_builder_add_string (builder, pin);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_unlock_response_new (uint32_t status)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_UNLOCK, status);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_unlock_response_parse (const uint8_t *message,
                                   uint32_t      *status)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_UNLOCK);

    if (status)
        *status = rmf_message_get_status (message);
}

/******************************************************************************/
/* Get Power Status */

uint8_t *
rmf_message_get_power_status_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_POWER_STATUS, 0);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_power_status_response_new   (uint32_t status,
                                             uint32_t power_status)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_POWER_STATUS, status);
    rmf_message_builder_add_uint32 (builder, power_status);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_get_power_status_response_parse (const uint8_t *message,
                                             uint32_t      *status,
                                             uint32_t      *power_status)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_GET_POWER_STATUS);

    if (status)
        *status = rmf_message_get_status (message);
    if (power_status)
        *power_status = rmf_message_read_uint32 (message, &offset);
}

/******************************************************************************/
/* Set Power Status */

uint8_t *
rmf_message_set_power_status_request_new (uint32_t power_status)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_SET_POWER_STATUS, 0);
    rmf_message_builder_add_uint32 (builder, power_status);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_set_power_status_response_new (uint32_t status)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_SET_POWER_STATUS, status);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_set_power_status_response_parse (const uint8_t *message,
                                             uint32_t      *status)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_SET_POWER_STATUS);

    if (status)
        *status = rmf_message_get_status (message);
}

/******************************************************************************/
/* Get Power Info */

uint8_t *
rmf_message_get_power_info_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_POWER_INFO, 0);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_power_info_response_new (uint32_t status,
                                         uint32_t gsm_in_traffic,
                                         uint32_t gsm_tx_power,
                                         uint32_t gsm_rx0_radio_tuned,
                                         uint32_t gsm_rx0_power,
                                         uint32_t gsm_rx1_radio_tuned,
                                         uint32_t gsm_rx1_power,
                                         uint32_t umts_in_traffic,
                                         uint32_t umts_tx_power,
                                         uint32_t umts_rx0_radio_tuned,
                                         uint32_t umts_rx0_power,
                                         uint32_t umts_rx1_radio_tuned,
                                         uint32_t umts_rx1_power,
                                         uint32_t lte_in_traffic,
                                         uint32_t lte_tx_power,
                                         uint32_t lte_rx0_radio_tuned,
                                         uint32_t lte_rx0_power,
                                         uint32_t lte_rx1_radio_tuned,
                                         uint32_t lte_rx1_power)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_POWER_INFO, status);
    rmf_message_builder_add_uint32 (builder, gsm_in_traffic);
    rmf_message_builder_add_uint32 (builder, gsm_tx_power);
    rmf_message_builder_add_uint32 (builder, gsm_rx0_radio_tuned);
    rmf_message_builder_add_uint32 (builder, gsm_rx0_power);
    rmf_message_builder_add_uint32 (builder, gsm_rx1_radio_tuned);
    rmf_message_builder_add_uint32 (builder, gsm_rx1_power);
    rmf_message_builder_add_uint32 (builder, umts_in_traffic);
    rmf_message_builder_add_uint32 (builder, umts_tx_power);
    rmf_message_builder_add_uint32 (builder, umts_rx0_radio_tuned);
    rmf_message_builder_add_uint32 (builder, umts_rx0_power);
    rmf_message_builder_add_uint32 (builder, umts_rx1_radio_tuned);
    rmf_message_builder_add_uint32 (builder, umts_rx1_power);
    rmf_message_builder_add_uint32 (builder, lte_in_traffic);
    rmf_message_builder_add_uint32 (builder, lte_tx_power);
    rmf_message_builder_add_uint32 (builder, lte_rx0_radio_tuned);
    rmf_message_builder_add_uint32 (builder, lte_rx0_power);
    rmf_message_builder_add_uint32 (builder, lte_rx1_radio_tuned);
    rmf_message_builder_add_uint32 (builder, lte_rx1_power);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_get_power_info_response_parse (const uint8_t *message,
                                           uint32_t      *status,
                                           uint32_t      *gsm_in_traffic,
                                           uint32_t      *gsm_tx_power,
                                           uint32_t      *gsm_rx0_radio_tuned,
                                           uint32_t      *gsm_rx0_power,
                                           uint32_t      *gsm_rx1_radio_tuned,
                                           uint32_t      *gsm_rx1_power,
                                           uint32_t      *umts_in_traffic,
                                           uint32_t      *umts_tx_power,
                                           uint32_t      *umts_rx0_radio_tuned,
                                           uint32_t      *umts_rx0_power,
                                           uint32_t      *umts_rx1_radio_tuned,
                                           uint32_t      *umts_rx1_power,
                                           uint32_t      *lte_in_traffic,
                                           uint32_t      *lte_tx_power,
                                           uint32_t      *lte_rx0_radio_tuned,
                                           uint32_t      *lte_rx0_power,
                                           uint32_t      *lte_rx1_radio_tuned,
                                           uint32_t      *lte_rx1_power)
{
    uint32_t offset = 0;
    uint32_t value;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_GET_POWER_INFO);

    if (status)
        *status = rmf_message_get_status (message);

    value = rmf_message_read_uint32 (message, &offset);
    if (gsm_in_traffic)
        *gsm_in_traffic = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (gsm_tx_power)
        *gsm_tx_power = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (gsm_rx0_radio_tuned)
        *gsm_rx0_radio_tuned = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (gsm_rx0_power)
        *gsm_rx0_power = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (gsm_rx1_power)
        *gsm_rx1_power = value;

    value = rmf_message_read_uint32 (message, &offset);
    if (umts_in_traffic)
        *umts_in_traffic = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (umts_tx_power)
        *umts_tx_power = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (umts_rx0_radio_tuned)
        *umts_rx0_radio_tuned = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (umts_rx0_power)
        *umts_rx0_power = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (umts_rx1_power)
        *umts_rx1_power = value;

    value = rmf_message_read_uint32 (message, &offset);
    if (lte_in_traffic)
        *lte_in_traffic = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (lte_tx_power)
        *lte_tx_power = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (lte_rx0_radio_tuned)
        *lte_rx0_radio_tuned = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (lte_rx0_power)
        *lte_rx0_power = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (lte_rx1_power)
        *lte_rx1_power = value;
}

/******************************************************************************/
/* Get Signal Info */

uint8_t *
rmf_message_get_signal_info_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_SIGNAL_INFO, 0);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_signal_info_response_new (uint32_t status,
                                          uint32_t gsm_available,
                                          uint32_t gsm_rssi,
                                          uint32_t gsm_quality,
                                          uint32_t umts_available,
                                          uint32_t umts_rssi,
                                          uint32_t umts_quality,
                                          uint32_t lte_available,
                                          uint32_t lte_rssi,
                                          uint32_t lte_quality)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_SIGNAL_INFO, status);
    rmf_message_builder_add_uint32 (builder, gsm_available);
    rmf_message_builder_add_uint32 (builder, gsm_rssi);
    rmf_message_builder_add_uint32 (builder, gsm_quality);
    rmf_message_builder_add_uint32 (builder, umts_available);
    rmf_message_builder_add_uint32 (builder, umts_rssi);
    rmf_message_builder_add_uint32 (builder, umts_quality);
    rmf_message_builder_add_uint32 (builder, lte_available);
    rmf_message_builder_add_uint32 (builder, lte_rssi);
    rmf_message_builder_add_uint32 (builder, lte_quality);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_get_signal_info_response_parse (const uint8_t *message,
                                            uint32_t      *status,
                                            uint32_t      *gsm_available,
                                            uint32_t      *gsm_rssi,
                                            uint32_t      *gsm_quality,
                                            uint32_t      *umts_available,
                                            uint32_t      *umts_rssi,
                                            uint32_t      *umts_quality,
                                            uint32_t      *lte_available,
                                            uint32_t      *lte_rssi,
                                            uint32_t      *lte_quality)
{
    uint32_t offset = 0;
    uint32_t value;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_GET_SIGNAL_INFO);

    if (status)
        *status = rmf_message_get_status (message);

    value = rmf_message_read_uint32 (message, &offset);
    if (gsm_available)
        *gsm_available = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (gsm_rssi)
        *gsm_rssi = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (gsm_quality)
        *gsm_quality = value;

    value = rmf_message_read_uint32 (message, &offset);
    if (umts_available)
        *umts_available = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (umts_rssi)
        *umts_rssi = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (umts_quality)
        *umts_quality = value;

    value = rmf_message_read_uint32 (message, &offset);
    if (lte_available)
        *lte_available = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (lte_rssi)
        *lte_rssi = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (lte_quality)
        *lte_quality = value;
}

/******************************************************************************/
/* Get Registration Status */

uint8_t *
rmf_message_get_registration_status_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_REGISTRATION_STATUS, 0);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_registration_status_response_new (uint32_t    status,
                                                  uint32_t    registration_status,
                                                  const char *operator_description,
                                                  uint32_t    operator_mcc,
                                                  uint32_t    operator_mnc,
                                                  uint32_t    lac,
                                                  uint32_t    cid)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_REGISTRATION_STATUS, status);
    rmf_message_builder_add_uint32 (builder, registration_status);
    rmf_message_builder_add_string (builder, operator_description);
    rmf_message_builder_add_uint32 (builder, operator_mcc);
    rmf_message_builder_add_uint32 (builder, operator_mnc);
    rmf_message_builder_add_uint32 (builder, lac);
    rmf_message_builder_add_uint32 (builder, cid);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_get_registration_status_response_parse (const uint8_t  *message,
                                                    uint32_t       *status,
                                                    uint32_t       *registration_status,
                                                    const char    **operator_description,
                                                    uint32_t       *operator_mcc,
                                                    uint32_t       *operator_mnc,
                                                    uint32_t       *lac,
                                                    uint32_t       *cid)
{
    uint32_t offset = 0;
    uint32_t value;
    const char *str;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_GET_REGISTRATION_STATUS);

    if (status)
        *status = rmf_message_get_status (message);

    value = rmf_message_read_uint32 (message, &offset);
    if (registration_status)
        *registration_status = value;
    str = rmf_message_read_string (message, &offset);
    if (operator_description)
        *operator_description = str;
    value = rmf_message_read_uint32 (message, &offset);
    if (operator_mcc)
        *operator_mcc = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (operator_mnc)
        *operator_mnc = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (lac)
        *lac = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (cid)
        *cid = value;
}

/******************************************************************************/
/* Get Connection Status */

uint8_t *
rmf_message_get_connection_status_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_CONNECTION_STATUS, 0);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_connection_status_response_new (uint32_t status,
                                                uint32_t connection_status)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_CONNECTION_STATUS, status);
    rmf_message_builder_add_uint32 (builder, connection_status);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_get_connection_status_response_parse (const uint8_t *message,
                                                  uint32_t      *status,
                                                  uint32_t      *connection_status)
{
    uint32_t offset = 0;
    uint32_t value;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_GET_CONNECTION_STATUS);

    if (status)
        *status = rmf_message_get_status (message);

    value = rmf_message_read_uint32 (message, &offset);
    if (connection_status)
        *connection_status = value;
}

/******************************************************************************/
/* Get Connection Stats */

uint8_t *
rmf_message_get_connection_stats_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_CONNECTION_STATS, 0);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_connection_stats_response_new (uint32_t status,
                                               uint32_t tx_packets_ok,
                                               uint32_t rx_packets_ok,
                                               uint32_t tx_packets_error,
                                               uint32_t rx_packets_error,
                                               uint32_t tx_packets_overflow,
                                               uint32_t rx_packets_overflow,
                                               uint64_t tx_bytes_ok,
                                               uint64_t rx_bytes_ok)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_CONNECTION_STATS, status);
    rmf_message_builder_add_uint32 (builder, tx_packets_ok);
    rmf_message_builder_add_uint32 (builder, rx_packets_ok);
    rmf_message_builder_add_uint32 (builder, tx_packets_error);
    rmf_message_builder_add_uint32 (builder, rx_packets_error);
    rmf_message_builder_add_uint32 (builder, tx_packets_overflow);
    rmf_message_builder_add_uint32 (builder, rx_packets_overflow);
    rmf_message_builder_add_uint64 (builder, tx_bytes_ok);
    rmf_message_builder_add_uint64 (builder, rx_bytes_ok);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_get_connection_stats_response_parse (const uint8_t *message,
                                                 uint32_t      *status,
                                                 uint32_t      *tx_packets_ok,
                                                 uint32_t      *rx_packets_ok,
                                                 uint32_t      *tx_packets_error,
                                                 uint32_t      *rx_packets_error,
                                                 uint32_t      *tx_packets_overflow,
                                                 uint32_t      *rx_packets_overflow,
                                                 uint64_t      *tx_bytes_ok,
                                                 uint64_t      *rx_bytes_ok)
{
    uint32_t offset = 0;
    uint32_t value;
    uint64_t value64;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_GET_CONNECTION_STATS);

    if (status)
        *status = rmf_message_get_status (message);

    value = rmf_message_read_uint32 (message, &offset);
    if (tx_packets_ok)
        *tx_packets_ok = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (rx_packets_ok)
        *rx_packets_ok = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (tx_packets_error)
        *tx_packets_error = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (rx_packets_error)
        *rx_packets_error = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (tx_packets_overflow)
        *tx_packets_overflow = value;
    value = rmf_message_read_uint32 (message, &offset);
    if (rx_packets_overflow)
        *rx_packets_overflow = value;
    value64 = rmf_message_read_uint64 (message, &offset);
    if (tx_bytes_ok)
        *tx_bytes_ok = value64;
    value64 = rmf_message_read_uint64 (message, &offset);
    if (rx_bytes_ok)
        *rx_bytes_ok = value64;
}

/******************************************************************************/
/* Connect */

uint8_t *
rmf_message_connect_request_new (const char *apn,
                                 const char *user,
                                 const char *password)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_CONNECT, 0);
    rmf_message_builder_add_string (builder, apn);
    rmf_message_builder_add_string (builder, user);
    rmf_message_builder_add_string (builder, password);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_connect_response_new (uint32_t status)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_CONNECT, status);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_connect_response_parse (const uint8_t *message,
                                    uint32_t      *status)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_CONNECT);

    if (status)
        *status = rmf_message_get_status (message);
}

/******************************************************************************/
/* Disconnect */

uint8_t *
rmf_message_disconnect_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_DISCONNECT, 0);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_disconnect_response_new (uint32_t status)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_DISCONNECT, status);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_disconnect_response_parse (const uint8_t *message,
                                       uint32_t      *status)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_DISCONNECT);

    if (status)
        *status = rmf_message_get_status (message);
}

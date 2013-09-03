/* -*- Mode: c; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 * librmf-common
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2013 Zodiac Inflight Innovations
 *
 * Author: Aleksander Morgado <aleksander@lanedo.com>
 */

#include <assert.h>

#include <rmf-messages.h>
#include <rmf-messages-private.h>

/******************************************************************************/
/* Common */

uint32_t
rmf_message_get_length (const uint8_t *message)
{
    return RMF_MESSAGE_LENGTH (message);
}

uint32_t
rmf_message_get_type (const uint8_t *message)
{
    return RMF_MESSAGE_TYPE (message);
}

uint32_t
rmf_message_get_command (const uint8_t *message)
{
    return RMF_MESSAGE_COMMAND (message);
}

uint32_t
rmf_message_request_and_response_match (const uint8_t *request,
                                        const uint8_t *response)
{
    if (rmf_message_get_type (request) != RMF_MESSAGE_TYPE_REQUEST)
        return 0;
    if (rmf_message_get_type (response) != RMF_MESSAGE_TYPE_RESPONSE)
        return 0;
    if (rmf_message_get_command (request) != rmf_message_get_command (response))
        return 0;
    return 1;
}

/******************************************************************************/
/* Generic error response */

uint8_t *
rmf_message_error_response_new (uint32_t command,
                                uint32_t status)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    assert (status != RMF_RESPONSE_STATUS_OK);

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, command, status);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
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

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_MANUFACTURER, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_manufacturer_response_new (const char *manufacturer)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_MANUFACTURER, RMF_RESPONSE_STATUS_OK);
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

    if (rmf_message_get_status (message) != RMF_RESPONSE_STATUS_OK)
        return;

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

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_MODEL, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_model_response_new (const char *model)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_MODEL, RMF_RESPONSE_STATUS_OK);
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

    if (rmf_message_get_status (message) != RMF_RESPONSE_STATUS_OK)
        return;

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

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_SOFTWARE_REVISION, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_software_revision_response_new (const char *software_revision)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_SOFTWARE_REVISION, RMF_RESPONSE_STATUS_OK);
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

    if (rmf_message_get_status (message) != RMF_RESPONSE_STATUS_OK)
        return;

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

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_HARDWARE_REVISION, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_hardware_revision_response_new (const char *hardware_revision)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_HARDWARE_REVISION, RMF_RESPONSE_STATUS_OK);
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

    if (rmf_message_get_status (message) != RMF_RESPONSE_STATUS_OK)
        return;

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

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_IMEI, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_imei_response_new (const char *imei)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_IMEI, RMF_RESPONSE_STATUS_OK);
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

    if (rmf_message_get_status (message) != RMF_RESPONSE_STATUS_OK)
        return;

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

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_IMSI, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_imsi_response_new (const char *imsi)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_IMSI, RMF_RESPONSE_STATUS_OK);
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

    if (rmf_message_get_status (message) != RMF_RESPONSE_STATUS_OK)
        return;

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

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_ICCID, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_iccid_response_new (const char *iccid)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_ICCID, RMF_RESPONSE_STATUS_OK);
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

    if (rmf_message_get_status (message) != RMF_RESPONSE_STATUS_OK)
        return;

    if (iccid)
        *iccid = rmf_message_read_string (message, &offset);
}

/******************************************************************************/
/* Is Locked */

uint8_t *
rmf_message_is_sim_locked_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_IS_SIM_LOCKED, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_is_sim_locked_response_new (uint8_t locked)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_IS_SIM_LOCKED, RMF_RESPONSE_STATUS_OK);
    rmf_message_builder_add_uint32 (builder, (uint32_t) locked);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_is_sim_locked_response_parse (const uint8_t *message,
                                          uint32_t      *status,
                                          uint8_t       *locked)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_IS_SIM_LOCKED);

    if (status)
        *status = rmf_message_get_status (message);

    if (locked)
        *locked = (uint8_t) rmf_message_read_uint32 (message, &offset);
}

/******************************************************************************/
/* Unlock */

uint8_t *
rmf_message_unlock_request_new (const char *pin)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_UNLOCK, RMF_RESPONSE_STATUS_OK);
    rmf_message_builder_add_string (builder, pin);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_unlock_request_parse (const uint8_t  *message,
                                  const char    **pin)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_REQUEST);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_UNLOCK);

    if (pin)
        *pin = rmf_message_read_string (message, &offset);
}

uint8_t *
rmf_message_unlock_response_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_UNLOCK, RMF_RESPONSE_STATUS_OK);
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
/* Enable/Disable PIN */

uint8_t *
rmf_message_enable_pin_request_new (uint32_t   enable,
                                    const char *pin)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_ENABLE_PIN, RMF_RESPONSE_STATUS_OK);
    rmf_message_builder_add_uint32 (builder, enable);
    rmf_message_builder_add_string (builder, pin);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_enable_pin_request_parse (const uint8_t  *message,
                                      uint32_t       *enable,
                                      const char    **pin)
{
    uint32_t offset = 0;
    uint32_t val;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_REQUEST);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_ENABLE_PIN);
    val = rmf_message_read_uint32 (message, &offset);
    if (enable)
        *enable = val;
    if (pin)
        *pin = rmf_message_read_string (message, &offset);
}

uint8_t *
rmf_message_enable_pin_response_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_ENABLE_PIN, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_enable_pin_response_parse (const uint8_t *message,
                                       uint32_t      *status)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_ENABLE_PIN);

    if (status)
        *status = rmf_message_get_status (message);
}

/******************************************************************************/
/* Change PIN */

uint8_t *
rmf_message_change_pin_request_new (const char *pin,
                                    const char *new_pin)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_CHANGE_PIN, RMF_RESPONSE_STATUS_OK);
    rmf_message_builder_add_string (builder, pin);
    rmf_message_builder_add_string (builder, new_pin);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_change_pin_request_parse (const uint8_t  *message,
                                      const char    **pin,
                                      const char    **new_pin)
{
    uint32_t offset = 0;
    const char *val;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_REQUEST);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_CHANGE_PIN);
    val = rmf_message_read_string (message, &offset);
    if (pin)
        *pin = val;
    if (new_pin)
        *new_pin = rmf_message_read_string (message, &offset);
}

uint8_t *
rmf_message_change_pin_response_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_CHANGE_PIN, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_change_pin_response_parse (const uint8_t *message,
                                       uint32_t      *status)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_CHANGE_PIN);

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

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_POWER_STATUS, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_power_status_response_new (uint32_t power_status)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_POWER_STATUS, RMF_RESPONSE_STATUS_OK);
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

    if (rmf_message_get_status (message) != RMF_RESPONSE_STATUS_OK)
        return;

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

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_SET_POWER_STATUS, RMF_RESPONSE_STATUS_OK);
    rmf_message_builder_add_uint32 (builder, power_status);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_set_power_status_request_parse (const uint8_t *message,
                                            uint32_t      *power_status)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_REQUEST);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_SET_POWER_STATUS);
    if (power_status)
        *power_status = rmf_message_read_uint32 (message, &offset);
}

uint8_t *
rmf_message_set_power_status_response_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_SET_POWER_STATUS, RMF_RESPONSE_STATUS_OK);
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

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_POWER_INFO, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_power_info_response_new (uint32_t gsm_in_traffic,
                                         int32_t  gsm_tx_power,
                                         uint32_t gsm_rx0_radio_tuned,
                                         int32_t  gsm_rx0_power,
                                         uint32_t gsm_rx1_radio_tuned,
                                         int32_t  gsm_rx1_power,
                                         uint32_t umts_in_traffic,
                                         int32_t  umts_tx_power,
                                         uint32_t umts_rx0_radio_tuned,
                                         int32_t  umts_rx0_power,
                                         uint32_t umts_rx1_radio_tuned,
                                         int32_t  umts_rx1_power,
                                         uint32_t lte_in_traffic,
                                         int32_t  lte_tx_power,
                                         uint32_t lte_rx0_radio_tuned,
                                         int32_t  lte_rx0_power,
                                         uint32_t lte_rx1_radio_tuned,
                                         int32_t  lte_rx1_power)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_POWER_INFO, RMF_RESPONSE_STATUS_OK);
    rmf_message_builder_add_uint32 (builder, gsm_in_traffic);
    rmf_message_builder_add_int32 (builder, gsm_tx_power);
    rmf_message_builder_add_uint32 (builder, gsm_rx0_radio_tuned);
    rmf_message_builder_add_int32 (builder, gsm_rx0_power);
    rmf_message_builder_add_uint32 (builder, gsm_rx1_radio_tuned);
    rmf_message_builder_add_int32 (builder, gsm_rx1_power);
    rmf_message_builder_add_uint32 (builder, umts_in_traffic);
    rmf_message_builder_add_int32 (builder, umts_tx_power);
    rmf_message_builder_add_uint32 (builder, umts_rx0_radio_tuned);
    rmf_message_builder_add_int32 (builder, umts_rx0_power);
    rmf_message_builder_add_uint32 (builder, umts_rx1_radio_tuned);
    rmf_message_builder_add_int32 (builder, umts_rx1_power);
    rmf_message_builder_add_uint32 (builder, lte_in_traffic);
    rmf_message_builder_add_int32 (builder, lte_tx_power);
    rmf_message_builder_add_uint32 (builder, lte_rx0_radio_tuned);
    rmf_message_builder_add_int32 (builder, lte_rx0_power);
    rmf_message_builder_add_uint32 (builder, lte_rx1_radio_tuned);
    rmf_message_builder_add_int32 (builder, lte_rx1_power);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_get_power_info_response_parse (const uint8_t *message,
                                           uint32_t      *status,
                                           uint32_t      *gsm_in_traffic,
                                           int32_t       *gsm_tx_power,
                                           uint32_t      *gsm_rx0_radio_tuned,
                                           int32_t       *gsm_rx0_power,
                                           uint32_t      *gsm_rx1_radio_tuned,
                                           int32_t       *gsm_rx1_power,
                                           uint32_t      *umts_in_traffic,
                                           int32_t       *umts_tx_power,
                                           uint32_t      *umts_rx0_radio_tuned,
                                           int32_t       *umts_rx0_power,
                                           uint32_t      *umts_rx1_radio_tuned,
                                           int32_t       *umts_rx1_power,
                                           uint32_t      *lte_in_traffic,
                                           int32_t       *lte_tx_power,
                                           uint32_t      *lte_rx0_radio_tuned,
                                           int32_t       *lte_rx0_power,
                                           uint32_t      *lte_rx1_radio_tuned,
                                           int32_t       *lte_rx1_power)
{
    uint32_t offset = 0;
    uint32_t uvalue;
    int32_t ivalue;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_GET_POWER_INFO);

    if (status)
        *status = rmf_message_get_status (message);

    if (rmf_message_get_status (message) != RMF_RESPONSE_STATUS_OK)
        return;

    uvalue = rmf_message_read_uint32 (message, &offset);
    if (gsm_in_traffic)
        *gsm_in_traffic = uvalue;
    ivalue = rmf_message_read_int32 (message, &offset);
    if (gsm_tx_power)
        *gsm_tx_power = ivalue;
    uvalue = rmf_message_read_uint32 (message, &offset);
    if (gsm_rx0_radio_tuned)
        *gsm_rx0_radio_tuned = uvalue;
    ivalue = rmf_message_read_int32 (message, &offset);
    if (gsm_rx0_power)
        *gsm_rx0_power = ivalue;
    uvalue = rmf_message_read_uint32 (message, &offset);
    if (gsm_rx1_radio_tuned)
        *gsm_rx1_radio_tuned = uvalue;
    ivalue = rmf_message_read_int32 (message, &offset);
    if (gsm_rx1_power)
        *gsm_rx1_power = ivalue;

    uvalue = rmf_message_read_uint32 (message, &offset);
    if (umts_in_traffic)
        *umts_in_traffic = uvalue;
    ivalue = rmf_message_read_int32 (message, &offset);
    if (umts_tx_power)
        *umts_tx_power = ivalue;
    uvalue = rmf_message_read_uint32 (message, &offset);
    if (umts_rx0_radio_tuned)
        *umts_rx0_radio_tuned = uvalue;
    ivalue = rmf_message_read_int32 (message, &offset);
    if (umts_rx0_power)
        *umts_rx0_power = ivalue;
    uvalue = rmf_message_read_uint32 (message, &offset);
    if (umts_rx1_radio_tuned)
        *umts_rx1_radio_tuned = uvalue;
    ivalue = rmf_message_read_int32 (message, &offset);
    if (umts_rx1_power)
        *umts_rx1_power = ivalue;

    uvalue = rmf_message_read_uint32 (message, &offset);
    if (lte_in_traffic)
        *lte_in_traffic = uvalue;
    ivalue = rmf_message_read_int32 (message, &offset);
    if (lte_tx_power)
        *lte_tx_power = ivalue;
    uvalue = rmf_message_read_uint32 (message, &offset);
    if (lte_rx0_radio_tuned)
        *lte_rx0_radio_tuned = uvalue;
    ivalue = rmf_message_read_int32 (message, &offset);
    if (lte_rx0_power)
        *lte_rx0_power = ivalue;
    uvalue = rmf_message_read_uint32 (message, &offset);
    if (lte_rx1_radio_tuned)
        *lte_rx1_radio_tuned = uvalue;
    ivalue = rmf_message_read_int32 (message, &offset);
    if (lte_rx1_power)
        *lte_rx1_power = ivalue;
}

/******************************************************************************/
/* Get Signal Info */

uint8_t *
rmf_message_get_signal_info_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_SIGNAL_INFO, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_signal_info_response_new (uint32_t gsm_available,
                                          int32_t  gsm_rssi,
                                          uint32_t gsm_quality,
                                          uint32_t umts_available,
                                          int32_t  umts_rssi,
                                          uint32_t umts_quality,
                                          uint32_t lte_available,
                                          int32_t  lte_rssi,
                                          uint32_t lte_quality)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_SIGNAL_INFO, RMF_RESPONSE_STATUS_OK);
    rmf_message_builder_add_uint32 (builder, gsm_available);
    rmf_message_builder_add_int32 (builder, gsm_rssi);
    rmf_message_builder_add_uint32 (builder, gsm_quality);
    rmf_message_builder_add_uint32 (builder, umts_available);
    rmf_message_builder_add_int32 (builder, umts_rssi);
    rmf_message_builder_add_uint32 (builder, umts_quality);
    rmf_message_builder_add_uint32 (builder, lte_available);
    rmf_message_builder_add_int32 (builder, lte_rssi);
    rmf_message_builder_add_uint32 (builder, lte_quality);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_get_signal_info_response_parse (const uint8_t *message,
                                            uint32_t      *status,
                                            uint32_t      *gsm_available,
                                            int32_t       *gsm_rssi,
                                            uint32_t      *gsm_quality,
                                            uint32_t      *umts_available,
                                            int32_t       *umts_rssi,
                                            uint32_t      *umts_quality,
                                            uint32_t      *lte_available,
                                            int32_t       *lte_rssi,
                                            uint32_t      *lte_quality)
{
    uint32_t offset = 0;
    uint32_t uvalue;
    int32_t ivalue;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_GET_SIGNAL_INFO);

    if (status)
        *status = rmf_message_get_status (message);

    if (rmf_message_get_status (message) != RMF_RESPONSE_STATUS_OK)
        return;

    uvalue = rmf_message_read_uint32 (message, &offset);
    if (gsm_available)
        *gsm_available = uvalue;
    ivalue = rmf_message_read_int32 (message, &offset);
    if (gsm_rssi)
        *gsm_rssi = ivalue;
    uvalue = rmf_message_read_uint32 (message, &offset);
    if (gsm_quality)
        *gsm_quality = uvalue;

    uvalue = rmf_message_read_uint32 (message, &offset);
    if (umts_available)
        *umts_available = uvalue;
    ivalue = rmf_message_read_int32 (message, &offset);
    if (umts_rssi)
        *umts_rssi = ivalue;
    uvalue = rmf_message_read_uint32 (message, &offset);
    if (umts_quality)
        *umts_quality = uvalue;

    uvalue = rmf_message_read_uint32 (message, &offset);
    if (lte_available)
        *lte_available = uvalue;
    ivalue = rmf_message_read_int32 (message, &offset);
    if (lte_rssi)
        *lte_rssi = ivalue;
    uvalue = rmf_message_read_uint32 (message, &offset);
    if (lte_quality)
        *lte_quality = uvalue;
}

/******************************************************************************/
/* Get Registration Status */

uint8_t *
rmf_message_get_registration_status_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_REGISTRATION_STATUS, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_registration_status_response_new (uint32_t    registration_status,
                                                  const char *operator_description,
                                                  uint32_t    operator_mcc,
                                                  uint32_t    operator_mnc,
                                                  uint32_t    lac,
                                                  uint32_t    cid)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_REGISTRATION_STATUS, RMF_RESPONSE_STATUS_OK);
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

    if (rmf_message_get_status (message) != RMF_RESPONSE_STATUS_OK)
        return;

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

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_CONNECTION_STATUS, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_connection_status_response_new (uint32_t connection_status)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_CONNECTION_STATUS, RMF_RESPONSE_STATUS_OK);
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

    if (rmf_message_get_status (message) != RMF_RESPONSE_STATUS_OK)
        return;

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

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_GET_CONNECTION_STATS, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_get_connection_stats_response_new (uint32_t tx_packets_ok,
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

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_GET_CONNECTION_STATS, RMF_RESPONSE_STATUS_OK);
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

    if (rmf_message_get_status (message) != RMF_RESPONSE_STATUS_OK)
        return;

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

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_CONNECT, RMF_RESPONSE_STATUS_OK);
    rmf_message_builder_add_string (builder, apn);
    rmf_message_builder_add_string (builder, user);
    rmf_message_builder_add_string (builder, password);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_connect_request_parse (const uint8_t  *message,
                                   const char    **apn,
                                   const char    **user,
                                   const char    **password)
{
    uint32_t offset = 0;
    const char *val;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_REQUEST);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_CONNECT);
    val = rmf_message_read_string (message, &offset);
    if (apn)
        *apn = val;
    val = rmf_message_read_string (message, &offset);
    if (user)
        *user = val;
    if (password)
        *password = rmf_message_read_string (message, &offset);
}

uint8_t *
rmf_message_connect_response_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_CONNECT, RMF_RESPONSE_STATUS_OK);
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

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_DISCONNECT, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_disconnect_response_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_DISCONNECT, RMF_RESPONSE_STATUS_OK);
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

/******************************************************************************/
/* Modem Is Available */

uint8_t *
rmf_message_is_modem_available_request_new (void)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_REQUEST, RMF_MESSAGE_COMMAND_IS_MODEM_AVAILABLE, RMF_RESPONSE_STATUS_OK);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

uint8_t *
rmf_message_is_modem_available_response_new (uint8_t available)
{
    RmfMessageBuilder *builder;
    uint8_t *message;

    builder = rmf_message_builder_new (RMF_MESSAGE_TYPE_RESPONSE, RMF_MESSAGE_COMMAND_IS_MODEM_AVAILABLE, RMF_RESPONSE_STATUS_OK);
    rmf_message_builder_add_uint32 (builder, (uint32_t) available);
    message = rmf_message_builder_serialize (builder);
    rmf_message_builder_free (builder);

    return message;
}

void
rmf_message_is_modem_available_response_parse (const uint8_t *message,
                                               uint32_t      *status,
                                               uint8_t       *available)
{
    uint32_t offset = 0;

    assert (rmf_message_get_type (message) == RMF_MESSAGE_TYPE_RESPONSE);
    assert (rmf_message_get_command (message) == RMF_MESSAGE_COMMAND_IS_MODEM_AVAILABLE);

    if (status)
        *status = rmf_message_get_status (message);

    if (available)
        *available = (uint8_t) rmf_message_read_uint32 (message, &offset);
}

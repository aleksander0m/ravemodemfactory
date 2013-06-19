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
    RMF_MESSAGE_COMMAND_UNKNOWN = 0,
    RMF_MESSAGE_COMMAND_GET_MANUFACTURER = 1,
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

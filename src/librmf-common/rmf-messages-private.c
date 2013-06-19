/* -*- Mode: c; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 *  librmf-common
 *
 *  Copyright (C) 2013 Zodiac Inflight Innovations
 */

#include <assert.h>
#include <malloc.h>
#include <string.h>

#include "rmf-messages-private.h"

/******************************************************************************/
/* Message builder */

struct _RmfMessageBuilder {
    struct RmfMessageHeader header;
    uint8_t *fixed;
    uint8_t *variable;
};

void
rmf_message_builder_free (RmfMessageBuilder *builder)
{
    free (builder->fixed);
    free (builder->variable);
    free (builder);
}

RmfMessageBuilder *
rmf_message_builder_new (uint32_t type,
                         uint32_t command,
                         uint32_t status)
{
    RmfMessageBuilder *builder;

    builder = malloc (sizeof (RmfMessageBuilder));
    builder->header.length = sizeof (struct RmfMessageHeader);
    builder->header.type = type;
    builder->header.command = command;
    builder->header.status = status;
    builder->header.fixed_size = 0;
    builder->header.variable_size = 0;
    builder->fixed = NULL;
    builder->variable = NULL;

    return builder;
}

void
rmf_message_builder_add_uint32 (RmfMessageBuilder *builder,
                                uint32_t           value)
{
    uint32_t original_fixed_size;

    /* Integers are added directly to the fixed size chunk, in host-byte
     * order, whatever it is */

    original_fixed_size = builder->header.fixed_size;
    builder->header.fixed_size += 4;
    builder->fixed = realloc (builder->fixed, builder->header.fixed_size);
    memcpy (&builder->fixed[original_fixed_size], &value, 4);
    builder->header.length += 4;
}

void
rmf_message_builder_add_string (RmfMessageBuilder *builder,
                                const char        *value)
{
    uint32_t original_variable_size;
    uint32_t value_len;
    uint32_t extra_variable_size;

    /* Strings are added as:
     *  - size + offset in the fixed buffer,
     *  - actual string in the variable buffer
     *
     * Note that the end-of-string NUL byte is always added
     */

    original_variable_size = builder->header.variable_size;
    value_len = strlen (value) + 1;

    /* Members of the variable length chunk are always memory aligned to 32bit
     * (making sure the string field size is multiple of 4) */
    extra_variable_size = 0;
    while ((value_len + extra_variable_size) % 4 != 0)
        extra_variable_size++;

    rmf_message_builder_add_uint32 (builder, original_variable_size);
    rmf_message_builder_add_uint32 (builder, value_len);

    builder->header.variable_size += (value_len + extra_variable_size);
    builder->variable = realloc (builder->variable, builder->header.variable_size);
    if (extra_variable_size)
        memset (&builder->variable[builder->header.variable_size - extra_variable_size], 0, extra_variable_size);
    memcpy (&builder->variable[original_variable_size], value, value_len);
    builder->header.length += (value_len + extra_variable_size);
}

uint8_t *
rmf_message_builder_serialize (RmfMessageBuilder *builder)
{
    uint8_t *buffer;
    uint32_t aux = 0;

    assert (builder->header.length % 4 == 0);
    assert (builder->header.fixed_size % 4 == 0);
    assert (builder->header.variable_size % 4 == 0);

    buffer = malloc (builder->header.length);
    memcpy (&buffer[aux], &builder->header, sizeof (struct RmfMessageHeader));
    aux += sizeof (struct RmfMessageHeader);
    if (builder->header.fixed_size) {
        memcpy (&buffer[aux], builder->fixed, builder->header.fixed_size);
        aux += builder->header.fixed_size;
    }
    if (builder->header.variable_size) {
        memcpy (&buffer[aux], builder->variable, builder->header.variable_size);
        aux += builder->header.variable_size;
    }

    assert (aux == builder->header.length);

    return buffer;
}

/******************************************************************************/
/* Message reader */

uint32_t
rmf_message_get_type (const uint8_t *buffer)
{
    return ((struct RmfMessageHeader *)buffer)->type;
}

uint32_t
rmf_message_get_command (const uint8_t *buffer)
{
    return ((struct RmfMessageHeader *)buffer)->command;
}

uint32_t
rmf_message_get_status (const uint8_t *buffer)
{
    return ((struct RmfMessageHeader *)buffer)->status;
}

uint32_t
rmf_message_read_uint32 (const uint8_t *buffer,
                         uint32_t      *relative_fixed_offset)
{
    uint32_t absolute_fixed_offset;

    absolute_fixed_offset = sizeof (struct RmfMessageHeader) + *relative_fixed_offset;

    *relative_fixed_offset += 4;

    return *((uint32_t *)(&buffer[absolute_fixed_offset]));
}

const char *
rmf_message_read_string (const uint8_t *buffer,
                         uint32_t      *relative_fixed_offset)
{
    uint32_t absolute_fixed_offset;
    uint32_t string_relative_variable_offset;
    uint32_t string_len;

    absolute_fixed_offset = sizeof (struct RmfMessageHeader) + *relative_fixed_offset;
    string_relative_variable_offset = *((uint32_t *)(&buffer[absolute_fixed_offset]));
    string_len = *((uint32_t *)(&buffer[absolute_fixed_offset + 4]));
    *relative_fixed_offset += 8;

    return (const char *) &buffer[sizeof (struct RmfMessageHeader) +
                                  ((struct RmfMessageHeader *)buffer)->fixed_size +
                                  string_relative_variable_offset];
}

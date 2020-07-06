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
 * Copyright (C) 2013-2015 Safran Passenger Innovations
 *
 * Author: Aleksander Morgado <aleksander@aleksander.es>
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
    builder->header.length  = htole32 (sizeof (struct RmfMessageHeader));
    builder->header.type    = htole32 (type);
    builder->header.command = htole32 (command);
    builder->header.status  = htole32 (status);
    builder->header.fixed_size    = 0;
    builder->header.variable_size = 0;
    builder->fixed    = NULL;
    builder->variable = NULL;

    return builder;
}

void
rmf_message_builder_add_uint32 (RmfMessageBuilder *builder,
                                uint32_t           value)
{
    uint32_t original_fixed_size;
    uint32_t new_fixed_size;
    uint32_t value_le;

    /* Integers are added directly to the fixed size chunk, in BE always */
    original_fixed_size = le32toh (builder->header.fixed_size);
    new_fixed_size = original_fixed_size + 4;
    builder->header.fixed_size = htole32 (new_fixed_size);
    builder->fixed = realloc (builder->fixed, new_fixed_size);
    value_le = htole32 (value);
    memcpy (&builder->fixed[original_fixed_size], &value_le, 4);
    builder->header.length = htole32 ((le32toh (builder->header.length)) + 4);
}

void
rmf_message_builder_add_int32 (RmfMessageBuilder *builder,
                               int32_t            value)
{
    uint32_t original_fixed_size;
    uint32_t new_fixed_size;
    int32_t  value_le;

    /* Integers are added directly to the fixed size chunk, in BE always */
    original_fixed_size = le32toh (builder->header.fixed_size);
    new_fixed_size = original_fixed_size + 4;
    builder->header.fixed_size = htole32 (new_fixed_size);
    builder->fixed = realloc (builder->fixed, new_fixed_size);
    value_le = (int32_t) (htole32 ((uint32_t) value));
    memcpy (&builder->fixed[original_fixed_size], &value, 4);
    memcpy (&builder->fixed[original_fixed_size], &value_le, 4);
    builder->header.length = htole32 ((le32toh (builder->header.length)) + 4);
}

void
rmf_message_builder_add_uint64 (RmfMessageBuilder *builder,
                                uint64_t           value)
{
    uint32_t original_fixed_size;
    uint32_t new_fixed_size;
    uint64_t value_le;

    /* Integers are added directly to the fixed size chunk, in BE always */
    original_fixed_size = le32toh (builder->header.fixed_size);
    new_fixed_size = original_fixed_size + 8;
    builder->header.fixed_size = htole32 (new_fixed_size);
    builder->fixed = realloc (builder->fixed, new_fixed_size);
    value_le = htole64 (value);
    memcpy (&builder->fixed[original_fixed_size], &value_le, 8);
    builder->header.length = htole32 ((le32toh (builder->header.length)) + 8);
}

void
rmf_message_builder_add_string (RmfMessageBuilder *builder,
                                const char        *value)
{
    uint32_t original_variable_size;
    uint32_t new_variable_size;
    uint32_t value_len;
    uint32_t extra_variable_size;

    if (!value)
        value = "";

    /* Strings are added as:
     *  - size + offset in the fixed buffer,
     *  - actual string in the variable buffer
     *
     * Note that the end-of-string NUL byte is always added
     */

    original_variable_size = le32toh (builder->header.variable_size);
    value_len = strlen (value) + 1;

    /* Members of the variable length chunk are always memory aligned to 32bit
     * (making sure the string field size is multiple of 4) */
    extra_variable_size = 0;
    while ((value_len + extra_variable_size) % 4 != 0)
        extra_variable_size++;

    rmf_message_builder_add_uint32 (builder, original_variable_size);
    rmf_message_builder_add_uint32 (builder, value_len);

    new_variable_size = original_variable_size + value_len + extra_variable_size;
    builder->header.variable_size = htole32 (new_variable_size);
    builder->variable = realloc (builder->variable, new_variable_size);
    if (extra_variable_size)
        memset (&builder->variable[new_variable_size - extra_variable_size], 0, extra_variable_size);
    memcpy (&builder->variable[original_variable_size], value, value_len);
    builder->header.length = htole32 ((le32toh (builder->header.length)) + (value_len + extra_variable_size));
}

uint8_t *
rmf_message_builder_serialize (RmfMessageBuilder *builder)
{
    uint8_t *buffer;
    uint32_t aux = 0;

    assert (le32toh (builder->header.length) % 4 == 0);
    assert (le32toh (builder->header.fixed_size) % 4 == 0);
    assert (le32toh (builder->header.variable_size) % 4 == 0);

    buffer = malloc (le32toh (builder->header.length));
    memcpy (&buffer[aux], &builder->header, sizeof (struct RmfMessageHeader));
    aux += sizeof (struct RmfMessageHeader);
    if (builder->header.fixed_size) {
        memcpy (&buffer[aux], builder->fixed, le32toh (builder->header.fixed_size));
        aux += le32toh (builder->header.fixed_size);
    }
    if (builder->header.variable_size) {
        memcpy (&buffer[aux], builder->variable, le32toh (builder->header.variable_size));
        aux += le32toh (builder->header.variable_size);
    }

    assert (aux == le32toh (builder->header.length));

    return buffer;
}

/******************************************************************************/
/* Message reader */

uint32_t
rmf_message_get_status (const uint8_t *buffer)
{
    return le32toh (((struct RmfMessageHeader *)buffer)->status);
}

uint32_t
rmf_message_read_uint32 (const uint8_t *buffer,
                         uint32_t      *relative_fixed_offset)
{
    uint32_t absolute_fixed_offset;

    absolute_fixed_offset = sizeof (struct RmfMessageHeader) + *relative_fixed_offset;

    *relative_fixed_offset += 4;

    return le32toh (*((uint32_t *)(&buffer[absolute_fixed_offset])));
}

int32_t
rmf_message_read_int32 (const uint8_t *buffer,
                        uint32_t      *relative_fixed_offset)
{
    uint32_t absolute_fixed_offset;

    absolute_fixed_offset = sizeof (struct RmfMessageHeader) + *relative_fixed_offset;

    *relative_fixed_offset += 4;

    return (int32_t) le32toh (*((uint32_t *)(&buffer[absolute_fixed_offset])));
}

uint64_t
rmf_message_read_uint64 (const uint8_t *buffer,
                         uint32_t      *relative_fixed_offset)
{
    uint32_t absolute_fixed_offset;

    absolute_fixed_offset = sizeof (struct RmfMessageHeader) + *relative_fixed_offset;

    *relative_fixed_offset += 8;

    return le64toh (*((uint64_t *)(&buffer[absolute_fixed_offset])));
}

const char *
rmf_message_read_string (const uint8_t *buffer,
                         uint32_t      *relative_fixed_offset)
{
    uint32_t absolute_fixed_offset;
    uint32_t string_relative_variable_offset;

    absolute_fixed_offset = sizeof (struct RmfMessageHeader) + *relative_fixed_offset;
    string_relative_variable_offset = le32toh (*((uint32_t *)(&buffer[absolute_fixed_offset])));
    *relative_fixed_offset += 8;

    return (const char *) &buffer[sizeof (struct RmfMessageHeader) +
                                  ((struct RmfMessageHeader *)buffer)->fixed_size +
                                  string_relative_variable_offset];
}

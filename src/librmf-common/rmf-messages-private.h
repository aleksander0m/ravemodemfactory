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

#ifndef _RMF_MESSAGES_PRIVATE_H_
#define _RMF_MESSAGES_PRIVATE_H_

#include <stdio.h>
#include <stdint.h>
#include <endian.h>

/******************************************************************************/

struct RmfMessageHeader {
    uint32_t length;
    uint32_t type;
    uint32_t command;
    uint32_t status;
    uint32_t fixed_size;
    uint32_t variable_size;
}  __attribute__((packed));

#define RMF_MESSAGE_LENGTH(buffer)  (le32toh (((struct RmfMessageHeader *)buffer)->length))
#define RMF_MESSAGE_TYPE(buffer)    (le32toh (((struct RmfMessageHeader *)buffer)->type))
#define RMF_MESSAGE_COMMAND(buffer) (le32toh (((struct RmfMessageHeader *)buffer)->command))

/******************************************************************************/
/* Message builder */

typedef struct _RmfMessageBuilder RmfMessageBuilder;

RmfMessageBuilder *rmf_message_builder_new (uint32_t type,
                                            uint32_t command,
                                            uint32_t status);
void rmf_message_builder_add_uint32 (RmfMessageBuilder *builder,
                                     uint32_t           value);
void rmf_message_builder_add_int32  (RmfMessageBuilder *builder,
                                     int32_t            value);
void rmf_message_builder_add_uint64 (RmfMessageBuilder *builder,
                                     uint64_t           value);
void rmf_message_builder_add_string (RmfMessageBuilder *builder,
                                     const char        *value);
uint8_t *rmf_message_builder_serialize (RmfMessageBuilder *builder);
void rmf_message_builder_free (RmfMessageBuilder *builder);

/******************************************************************************/
/* Message reader */

uint32_t rmf_message_get_status (const uint8_t *buffer);
uint32_t rmf_message_read_uint32 (const uint8_t *buffer,
                                  uint32_t      *relative_fixed_offset);
int32_t  rmf_message_read_int32  (const uint8_t *buffer,
                                  uint32_t      *relative_fixed_offset);
uint64_t rmf_message_read_uint64 (const uint8_t *buffer,
                                  uint32_t      *relative_fixed_offset);
const char *rmf_message_read_string (const uint8_t *buffer,
                                     uint32_t      *relative_fixed_offset);


#endif /* _RMF_MESSAGES_PRIVATE_H_ */

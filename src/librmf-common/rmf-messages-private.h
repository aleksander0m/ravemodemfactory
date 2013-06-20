/* -*- Mode: c; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 *  librmf-common
 *
 *  Copyright (C) 2013 Zodiac Inflight Innovations
 */

#ifndef _RMF_MESSAGES_PRIVATE_H_
#define _RMF_MESSAGES_PRIVATE_H_

#include <stdio.h>
#include <stdint.h>

/******************************************************************************/

struct RmfMessageHeader {
    uint32_t length;
    uint32_t type;
    uint32_t command;
    uint32_t status;
    uint32_t fixed_size;
    uint32_t variable_size;
}  __attribute__((packed));

#define RMF_MESSAGE_LENGTH(buffer) ((struct RmfMessageHeader *)buffer)->length

/******************************************************************************/
/* Message builder */

typedef struct _RmfMessageBuilder RmfMessageBuilder;

RmfMessageBuilder *rmf_message_builder_new (uint32_t type,
                                            uint32_t command,
                                            uint32_t status);
void rmf_message_builder_add_uint32 (RmfMessageBuilder *builder,
                                     uint32_t           value);
void rmf_message_builder_add_uint64 (RmfMessageBuilder *builder,
                                     uint64_t           value);
void rmf_message_builder_add_string (RmfMessageBuilder *builder,
                                     const char        *value);
uint8_t *rmf_message_builder_serialize (RmfMessageBuilder *builder);

/******************************************************************************/
/* Message reader */

uint32_t rmf_message_get_type (const uint8_t *buffer);
uint32_t rmf_message_get_command (const uint8_t *buffer);
uint32_t rmf_message_get_status (const uint8_t *buffer);
uint32_t rmf_message_read_uint32 (const uint8_t *buffer,
                                  uint32_t      *relative_fixed_offset);
uint64_t rmf_message_read_uint64 (const uint8_t *buffer,
                                  uint32_t      *relative_fixed_offset);
const char *rmf_message_read_string (const uint8_t *buffer,
                                     uint32_t      *relative_fixed_offset);


#endif /* _RMF_MESSAGES_PRIVATE_H_ */

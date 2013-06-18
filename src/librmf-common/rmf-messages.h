/* -*- Mode: c; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 *  librmf-common
 *
 *  Copyright (C) 2013 Zodiac Inflight Innovations
 */

#ifndef _RMF_MESSAGES_H_
#define _RMF_MESSAGES_H_

#include <stdio.h>
#include <stdint.h>

/******************************************************************************/
/* Get Manufacturer */

uint8_t *rmf_message_get_manufacturer_request_new    (void);
uint8_t *rmf_message_get_manufacturer_response_new   (uint32_t        status,
                                                      const char     *manufacturer);
void     rmf_message_get_manufacturer_response_parse (const uint8_t  *message,
                                                      uint32_t       *status,
                                                      const char    **manufacturer);

#endif /* _RMF_MESSAGES_H_ */

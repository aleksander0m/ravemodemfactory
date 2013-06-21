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

#define RMF_MESSAGE_MAX_SIZE 4096
uint32_t rmf_message_get_length (const uint8_t *message);
uint32_t rmf_message_request_and_response_match (const uint8_t *request,
                                                 const uint8_t *response);

enum {
    RMF_RESPONSE_STATUS_OK = 0,
    RMF_RESPONSE_STATUS_ERROR_UNKNOWN,
};

/******************************************************************************/
/* Get Manufacturer */

uint8_t *rmf_message_get_manufacturer_request_new    (void);
uint8_t *rmf_message_get_manufacturer_response_new   (uint32_t        status,
                                                      const char     *manufacturer);
void     rmf_message_get_manufacturer_response_parse (const uint8_t  *message,
                                                      uint32_t       *status,
                                                      const char    **manufacturer);

/******************************************************************************/
/* Get Model */

uint8_t *rmf_message_get_model_request_new    (void);
uint8_t *rmf_message_get_model_response_new   (uint32_t        status,
                                               const char     *model);
void     rmf_message_get_model_response_parse (const uint8_t  *message,
                                               uint32_t       *status,
                                               const char    **model);

/******************************************************************************/
/* Get Software Revision */

uint8_t *rmf_message_get_software_revision_request_new    (void);
uint8_t *rmf_message_get_software_revision_response_new   (uint32_t        status,
                                                           const char     *software_revision);
void     rmf_message_get_software_revision_response_parse (const uint8_t  *message,
                                                           uint32_t       *status,
                                                           const char    **software_revision);

/******************************************************************************/
/* Get Hardware Revision */

uint8_t *rmf_message_get_hardware_revision_request_new    (void);
uint8_t *rmf_message_get_hardware_revision_response_new   (uint32_t        status,
                                                           const char     *hardware_revision);
void     rmf_message_get_hardware_revision_response_parse (const uint8_t  *message,
                                                           uint32_t       *status,
                                                           const char    **hardware_revision);

/******************************************************************************/
/* Get IMEI */

uint8_t *rmf_message_get_imei_request_new    (void);
uint8_t *rmf_message_get_imei_response_new   (uint32_t        status,
                                              const char     *imei);
void     rmf_message_get_imei_response_parse (const uint8_t  *message,
                                              uint32_t       *status,
                                              const char    **imei);

/******************************************************************************/
/* Get IMSI */

uint8_t *rmf_message_get_imsi_request_new    (void);
uint8_t *rmf_message_get_imsi_response_new   (uint32_t        status,
                                              const char     *imsi);
void     rmf_message_get_imsi_response_parse (const uint8_t  *message,
                                              uint32_t       *status,
                                              const char    **imsi);

/******************************************************************************/
/* Get ICCID */

uint8_t *rmf_message_get_iccid_request_new    (void);
uint8_t *rmf_message_get_iccid_response_new   (uint32_t        status,
                                               const char     *iccid);
void     rmf_message_get_iccid_response_parse (const uint8_t  *message,
                                               uint32_t       *status,
                                               const char    **iccid);

/******************************************************************************/
/* Unlock */

uint8_t *rmf_message_unlock_request_new    (const char    *pin);
uint8_t *rmf_message_unlock_response_new   (uint32_t       status);
void     rmf_message_unlock_response_parse (const uint8_t *message,
                                            uint32_t      *status);

/******************************************************************************/
/* Enable/Disable PIN */

uint8_t *rmf_message_enable_pin_request_new    (uint32_t       enable,
                                                const char    *pin);
uint8_t *rmf_message_enable_pin_response_new   (uint32_t       status);
void     rmf_message_enable_pin_response_parse (const uint8_t *message,
                                                uint32_t      *status);

/******************************************************************************/
/* Change PIN */

uint8_t *rmf_message_change_pin_request_new    (const char    *pin,
                                                const char    *new_pin);
uint8_t *rmf_message_change_pin_response_new   (uint32_t       status);
void     rmf_message_change_pin_response_parse (const uint8_t *message,
                                                uint32_t      *status);

/******************************************************************************/
/* Get Power Status */

uint8_t *rmf_message_get_power_status_request_new    (void);
uint8_t *rmf_message_get_power_status_response_new   (uint32_t       status,
                                                      uint32_t       power_status);
void     rmf_message_get_power_status_response_parse (const uint8_t *message,
                                                      uint32_t      *status,
                                                      uint32_t      *power_status);

/******************************************************************************/
/* Set Power Status */

uint8_t *rmf_message_set_power_status_request_new    (uint32_t       power_status);
uint8_t *rmf_message_set_power_status_response_new   (uint32_t       status);
void     rmf_message_set_power_status_response_parse (const uint8_t *message,
                                                      uint32_t      *status);

/******************************************************************************/
/* Get Power Info */

uint8_t *rmf_message_get_power_info_request_new    (void);
uint8_t *rmf_message_get_power_info_response_new   (uint32_t       status,
                                                    uint32_t       gsm_in_traffic,
                                                    uint32_t       gsm_tx_power,
                                                    uint32_t       gsm_rx0_radio_tuned,
                                                    uint32_t       gsm_rx0_power,
                                                    uint32_t       gsm_rx1_radio_tuned,
                                                    uint32_t       gsm_rx1_power,
                                                    uint32_t       umts_in_traffic,
                                                    uint32_t       umts_tx_power,
                                                    uint32_t       umts_rx0_radio_tuned,
                                                    uint32_t       umts_rx0_power,
                                                    uint32_t       umts_rx1_radio_tuned,
                                                    uint32_t       umts_rx1_power,
                                                    uint32_t       lte_in_traffic,
                                                    uint32_t       lte_tx_power,
                                                    uint32_t       lte_rx0_radio_tuned,
                                                    uint32_t       lte_rx0_power,
                                                    uint32_t       lte_rx1_radio_tuned,
                                                    uint32_t       lte_rx1_power);
void     rmf_message_get_power_info_response_parse (const uint8_t *message,
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
                                                    uint32_t      *lte_rx1_power);

/******************************************************************************/
/* Get Signal Info */

uint8_t *rmf_message_get_signal_info_request_new    (void);
uint8_t *rmf_message_get_signal_info_response_new   (uint32_t       status,
                                                     uint32_t       gsm_available,
                                                     uint32_t       gsm_rssi,
                                                     uint32_t       gsm_quality,
                                                     uint32_t       umts_available,
                                                     uint32_t       umts_rssi,
                                                     uint32_t       umts_quality,
                                                     uint32_t       lte_available,
                                                     uint32_t       lte_rssi,
                                                     uint32_t       lte_quality);
void     rmf_message_get_signal_info_response_parse (const uint8_t *message,
                                                     uint32_t      *status,
                                                     uint32_t      *gsm_available,
                                                     uint32_t      *gsm_rssi,
                                                     uint32_t      *gsm_quality,
                                                     uint32_t      *umts_available,
                                                     uint32_t      *umts_rssi,
                                                     uint32_t      *umts_quality,
                                                     uint32_t      *lte_available,
                                                     uint32_t      *lte_rssi,
                                                     uint32_t      *lte_quality);

/******************************************************************************/
/* Get Registration Status */

uint8_t *rmf_message_get_registration_status_request_new    (void);
uint8_t *rmf_message_get_registration_status_response_new   (uint32_t        status,
                                                             uint32_t        registration_status,
                                                             const char     *operator_description,
                                                             uint32_t        operator_mcc,
                                                             uint32_t        operator_mnc,
                                                             uint32_t        lac,
                                                             uint32_t        cid);
void     rmf_message_get_registration_status_response_parse (const uint8_t  *message,
                                                             uint32_t       *status,
                                                             uint32_t       *registration_status,
                                                             const char    **operator_description,
                                                             uint32_t       *operator_mcc,
                                                             uint32_t       *operator_mnc,
                                                             uint32_t       *lac,
                                                             uint32_t       *cid);

/******************************************************************************/
/* Get Connection Status */

uint8_t *rmf_message_get_connection_status_request_new    (void);
uint8_t *rmf_message_get_connection_status_response_new   (uint32_t       status,
                                                           uint32_t       connection_status);
void     rmf_message_get_connection_status_response_parse (const uint8_t *message,
                                                           uint32_t      *status,
                                                           uint32_t      *connection_status);

/******************************************************************************/
/* Get Connection Stats */

uint8_t *rmf_message_get_connection_stats_request_new    (void);
uint8_t *rmf_message_get_connection_stats_response_new   (uint32_t       status,
                                                          uint32_t       tx_packets_ok,
                                                          uint32_t       rx_packets_ok,
                                                          uint32_t       tx_packets_error,
                                                          uint32_t       rx_packets_error,
                                                          uint32_t       tx_packets_overflow,
                                                          uint32_t       rx_packets_overflow,
                                                          uint64_t       tx_bytes_ok,
                                                          uint64_t       rx_bytes_ok);
void     rmf_message_get_connection_stats_response_parse (const uint8_t *message,
                                                          uint32_t      *status,
                                                          uint32_t      *tx_packets_ok,
                                                          uint32_t      *rx_packets_ok,
                                                          uint32_t      *tx_packets_error,
                                                          uint32_t      *rx_packets_error,
                                                          uint32_t      *tx_packets_overflow,
                                                          uint32_t      *rx_packets_overflow,
                                                          uint64_t      *tx_bytes_ok,
                                                          uint64_t      *rx_bytes_ok);

/******************************************************************************/
/* Connect */

uint8_t *rmf_message_connect_request_new    (const char    *apn,
                                             const char    *user,
                                             const char    *password);
uint8_t *rmf_message_connect_response_new   (uint32_t       status);
void     rmf_message_connect_response_parse (const uint8_t *message,
                                             uint32_t      *status);

/******************************************************************************/
/* Disconnect */

uint8_t *rmf_message_disconnect_request_new    (void);
uint8_t *rmf_message_disconnect_response_new   (uint32_t       status);
void     rmf_message_disconnect_response_parse (const uint8_t *message,
                                                uint32_t      *status);

#endif /* _RMF_MESSAGES_H_ */

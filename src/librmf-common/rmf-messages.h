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

#ifndef _RMF_MESSAGES_H_
#define _RMF_MESSAGES_H_

#include <stdio.h>
#include <stdint.h>

/******************************************************************************/

#define RMFD_SOCKET_PATH "/tmp/rmfd-server"

/******************************************************************************/

#define RMF_MESSAGE_MAX_SIZE 4096

uint32_t rmf_message_get_length                 (const uint8_t *message);
uint32_t rmf_message_get_type                   (const uint8_t *buffer);
uint32_t rmf_message_get_command                (const uint8_t *buffer);
uint32_t rmf_message_request_and_response_match (const uint8_t *request,
                                                 const uint8_t *response);

enum {
    RMF_RESPONSE_STATUS_OK                     = 0,
    RMF_RESPONSE_STATUS_ERROR_UNKNOWN          = 1,
    RMF_RESPONSE_STATUS_ERROR_INVALID_REQUEST  = 2,
    RMF_RESPONSE_STATUS_ERROR_UNKNOWN_COMMAND  = 3,
    RMF_RESPONSE_STATUS_ERROR_NO_MODEM         = 4,
    RMF_RESPONSE_STATUS_ERROR_INVALID_STATE    = 5,
    /* Mapping of QMI errors (libqmi error + 100) */
    RMF_RESPONSE_STATUS_ERROR_MALFORMED_MESSAGE                = 101,
    RMF_RESPONSE_STATUS_ERROR_NO_MEMORY                        = 102,
    RMF_RESPONSE_STATUS_ERROR_INTERNAL                         = 103,
    RMF_RESPONSE_STATUS_ERROR_ABORTED                          = 104,
    RMF_RESPONSE_STATUS_ERROR_CLIENT_IDS_EXHAUSTED             = 105,
    RMF_RESPONSE_STATUS_ERROR_UNABORTABLE_TRANSACTION          = 106,
    RMF_RESPONSE_STATUS_ERROR_INVALID_CLIENT_ID                = 107,
    RMF_RESPONSE_STATUS_ERROR_NO_THRESHOLDS_PROVIDED           = 108,
    RMF_RESPONSE_STATUS_ERROR_INVALID_HANDLE                   = 109,
    RMF_RESPONSE_STATUS_ERROR_INVALID_PROFILE                  = 110,
    RMF_RESPONSE_STATUS_ERROR_INVALID_PIN_ID                   = 111,
    RMF_RESPONSE_STATUS_ERROR_INCORRECT_PIN                    = 112,
    RMF_RESPONSE_STATUS_ERROR_NO_NETWORK_FOUND                 = 113,
    RMF_RESPONSE_STATUS_ERROR_CALL_FAILED                      = 114,
    RMF_RESPONSE_STATUS_ERROR_OUT_OF_CALL                      = 115,
    RMF_RESPONSE_STATUS_ERROR_NOT_PROVISIONED                  = 116,
    RMF_RESPONSE_STATUS_ERROR_MISSING_ARGUMENT                 = 117,
    /* 118 unused */
    RMF_RESPONSE_STATUS_ERROR_ARGUMENT_TOO_LONG                = 119,
    /* 120, 121 unused */
    RMF_RESPONSE_STATUS_ERROR_INVALID_TRANSACTION_ID           = 122,
    RMF_RESPONSE_STATUS_ERROR_DEVICE_IN_USE                    = 123,
    RMF_RESPONSE_STATUS_ERROR_NETWORK_UNSUPPORTED              = 124,
    RMF_RESPONSE_STATUS_ERROR_DEVICE_UNSUPPORTED               = 125,
    RMF_RESPONSE_STATUS_ERROR_NO_EFFECT                        = 126,
    RMF_RESPONSE_STATUS_ERROR_NO_FREE_PROFILE                  = 127,
    RMF_RESPONSE_STATUS_ERROR_INVALID_PDP_TYPE                 = 128,
    RMF_RESPONSE_STATUS_ERROR_INVALID_TECHNOLOGY_PREFERENCE    = 129,
    RMF_RESPONSE_STATUS_ERROR_INVALID_PROFILE_TYPE             = 130,
    RMF_RESPONSE_STATUS_ERROR_INVALID_SERVICE_TYPE             = 131,
    RMF_RESPONSE_STATUS_ERROR_INVALID_REGISTER_ACTION          = 132,
    RMF_RESPONSE_STATUS_ERROR_INVALID_PS_ATTACH_ACTION         = 133,
    RMF_RESPONSE_STATUS_ERROR_AUTHENTICATION_FAILED            = 134,
    RMF_RESPONSE_STATUS_ERROR_PIN_BLOCKED                      = 135,
    RMF_RESPONSE_STATUS_ERROR_PIN_ALWAYS_BLOCKED               = 136,
    RMF_RESPONSE_STATUS_ERROR_UIM_UNINITIALIZED                = 137,
    RMF_RESPONSE_STATUS_ERROR_MAXIMUM_QOS_REQUESTS_IN_USE      = 138,
    RMF_RESPONSE_STATUS_ERROR_INCORRECT_FLOW_FILTER            = 139,
    RMF_RESPONSE_STATUS_ERROR_NETWORK_QOS_UNAWARE              = 140,
    RMF_RESPONSE_STATUS_ERROR_INVALID_QOS_ID                   = 141,
    RMF_RESPONSE_STATUS_ERROR_QOS_UNAVAILABLE                  = 142,
    RMF_RESPONSE_STATUS_ERROR_FLOW_SUSPENDED                   = 143,
    /* 144, 145 unused */
    RMF_RESPONSE_STATUS_ERROR_GENERAL_ERROR                    = 146,
    RMF_RESPONSE_STATUS_ERROR_UNKNOWN_ERROR                    = 147,
    RMF_RESPONSE_STATUS_ERROR_INVALID_ARGUMENT                 = 148,
    RMF_RESPONSE_STATUS_ERROR_INVALID_INDEX                    = 149,
    RMF_RESPONSE_STATUS_ERROR_NO_ENTRY                         = 150,
    RMF_RESPONSE_STATUS_ERROR_DEVICE_STORAGE_FULL              = 151,
    RMF_RESPONSE_STATUS_ERROR_DEVICE_NOT_READY                 = 152,
    RMF_RESPONSE_STATUS_ERROR_NETWORK_NOT_READY                = 153,
    RMF_RESPONSE_STATUS_ERROR_WMS_CAUSE_CODE                   = 154,
    RMF_RESPONSE_STATUS_ERROR_WMS_MESSAGE_NOT_SENT             = 155,
    RMF_RESPONSE_STATUS_ERROR_WMS_MESSAGE_DELIVERY_FAILURE     = 156,
    RMF_RESPONSE_STATUS_ERROR_WMS_INVALID_MESSAGE_ID           = 157,
    RMF_RESPONSE_STATUS_ERROR_WMS_ENCODING                     = 158,
    RMF_RESPONSE_STATUS_ERROR_AUTHENTICATION_LOCK              = 159,
    RMF_RESPONSE_STATUS_ERROR_INVALID_TRANSITION               = 160,
    /* 161-164 unused */
    RMF_RESPONSE_STATUS_ERROR_SESSION_INACTIVE                 = 165,
    RMF_RESPONSE_STATUS_ERROR_SESSION_INVALID                  = 166,
    RMF_RESPONSE_STATUS_ERROR_SESSION_OWNERSHIP                = 167,
    RMF_RESPONSE_STATUS_ERROR_INSUFFICIENT_RESOURCES           = 168,
    RMF_RESPONSE_STATUS_ERROR_DISABLED                         = 169,
    RMF_RESPONSE_STATUS_ERROR_INVALID_OPERATION                = 170,
    RMF_RESPONSE_STATUS_ERROR_INVALID_QMI_COMMAND              = 171,
    RMF_RESPONSE_STATUS_ERROR_WMS_T_PDU_TYPE                   = 172,
    RMF_RESPONSE_STATUS_ERROR_WMS_SMSC_ADDRESS                 = 173,
    RMF_RESPONSE_STATUS_ERROR_INFORMATION_UNAVAILABLE          = 174,
    RMF_RESPONSE_STATUS_ERROR_SEGMENT_TOO_LONG                 = 175,
    RMF_RESPONSE_STATUS_ERROR_SEGMENT_ORDER                    = 176,
    RMF_RESPONSE_STATUS_ERROR_BUNDLING_NOT_SUPPORTED           = 177,
    /* 178, 179 unused */
    RMF_RESPONSE_STATUS_ERROR_SIM_FILE_NOT_FOUND               = 180,
    RMF_RESPONSE_STATUS_ERROR_EXTENDED_INTERNAL                = 181,
    RMF_RESPONSE_STATUS_ERROR_ACCESS_DENIED                    = 182,
    RMF_RESPONSE_STATUS_ERROR_HARDWARE_RESTRICTED              = 183,
    RMF_RESPONSE_STATUS_ERROR_ACK_NOT_SENT                     = 184,
    RMF_RESPONSE_STATUS_ERROR_INJECT_TIMEOUT                   = 185,
    /* 186-189 unused */
    RMF_RESPONSE_STATUS_ERROR_INCOMPATIBLE_STATE               = 190,
    RMF_RESPONSE_STATUS_ERROR_FDN_RESTRICT                     = 191,
    RMF_RESPONSE_STATUS_ERROR_SUPS_FAILURE_CASE                = 192,
    RMF_RESPONSE_STATUS_ERROR_NO_RADIO                         = 193,
    RMF_RESPONSE_STATUS_ERROR_NOT_SUPPORTED                    = 194,
    RMF_RESPONSE_STATUS_ERROR_NO_SUBSCRIPTION                  = 195,
    RMF_RESPONSE_STATUS_ERROR_CARD_CALL_CONTROL_FAILED         = 196,
    RMF_RESPONSE_STATUS_ERROR_NETWORK_ABORTED                  = 197,
    RMF_RESPONSE_STATUS_ERROR_MSG_BLOCKED                      = 198,
    /* 199 unused */
    RMF_RESPONSE_STATUS_ERROR_INVALID_SESSION_TYPE             = 200,
    RMF_RESPONSE_STATUS_ERROR_INVALID_PB_TYPE                  = 201,
    RMF_RESPONSE_STATUS_ERROR_NO_SIM                           = 202,
    RMF_RESPONSE_STATUS_ERROR_PB_NOT_READY                     = 203,
    RMF_RESPONSE_STATUS_ERROR_PIN_RESTRICTION                  = 204,
    RMF_RESPONSE_STATUS_ERROR_PIN2_RESTRICTION                 = 205,
    RMF_RESPONSE_STATUS_ERROR_PUK_RESTRICTION                  = 206,
    RMF_RESPONSE_STATUS_ERROR_PUK2_RESTRICTION                 = 207,
    RMF_RESPONSE_STATUS_ERROR_PB_ACCESS_RESTRICTED             = 208,
    RMF_RESPONSE_STATUS_ERROR_PB_TEXT_TOO_LONG                 = 209,
    RMF_RESPONSE_STATUS_ERROR_PB_NUMBER_TOO_LONG               = 210,
    RMF_RESPONSE_STATUS_ERROR_PB_HIDDEN_KEY_RESTRICTION        = 211,
};

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
    RMF_MESSAGE_COMMAND_ENABLE_PIN              = 9,
    RMF_MESSAGE_COMMAND_CHANGE_PIN              = 10,
    RMF_MESSAGE_COMMAND_GET_POWER_STATUS        = 11,
    RMF_MESSAGE_COMMAND_SET_POWER_STATUS        = 12,
    RMF_MESSAGE_COMMAND_GET_POWER_INFO          = 13,
    RMF_MESSAGE_COMMAND_GET_SIGNAL_INFO         = 14,
    RMF_MESSAGE_COMMAND_GET_REGISTRATION_STATUS = 15,
    RMF_MESSAGE_COMMAND_GET_CONNECTION_STATUS   = 16,
    RMF_MESSAGE_COMMAND_GET_CONNECTION_STATS    = 17,
    RMF_MESSAGE_COMMAND_CONNECT                 = 18,
    RMF_MESSAGE_COMMAND_DISCONNECT              = 19,
    RMF_MESSAGE_COMMAND_IS_SIM_LOCKED           = 20,
    RMF_MESSAGE_COMMAND_IS_MODEM_AVAILABLE      = 21
};

/******************************************************************************/
/* Additional enums, same as the ones in the librmf interface */

typedef enum  {
    RMF_REGISTRATION_STATUS_IDLE,
    RMF_REGISTRATION_STATUS_SEARCHING,
    RMF_REGISTRATION_STATUS_HOME,
    RMF_REGISTRATION_STATUS_ROAMING,
} RmfRegistrationStatus;

typedef enum {
    RMF_CONNECTION_STATUS_DISCONNECTED,
    RMF_CONNECTION_STATUS_DISCONNECTING,
    RMF_CONNECTION_STATUS_CONNECTING,
    RMF_CONNECTION_STATUS_CONNECTED,
} RmfConnectionStatus;

typedef enum {
    RMF_POWER_STATUS_FULL,
    RMF_POWER_STATUS_LOW,
} RmfPowerStatus;

typedef enum {
    RMF_RADIO_INTERFACE_GSM,
    RMF_RADIO_INTERFACE_UMTS,
    RMF_RADIO_INTERFACE_LTE
} RmfRadioInterface;

/******************************************************************************/
/* Generic error response */

uint8_t *rmf_message_error_response_new (uint32_t command,
                                         uint32_t status);

/******************************************************************************/
/* Get Manufacturer */

uint8_t *rmf_message_get_manufacturer_request_new    (void);
uint8_t *rmf_message_get_manufacturer_response_new   (const char     *manufacturer);
void     rmf_message_get_manufacturer_response_parse (const uint8_t  *message,
                                                      uint32_t       *status,
                                                      const char    **manufacturer);

/******************************************************************************/
/* Get Model */

uint8_t *rmf_message_get_model_request_new    (void);
uint8_t *rmf_message_get_model_response_new   (const char     *model);
void     rmf_message_get_model_response_parse (const uint8_t  *message,
                                               uint32_t       *status,
                                               const char    **model);

/******************************************************************************/
/* Get Software Revision */

uint8_t *rmf_message_get_software_revision_request_new    (void);
uint8_t *rmf_message_get_software_revision_response_new   (const char     *software_revision);
void     rmf_message_get_software_revision_response_parse (const uint8_t  *message,
                                                           uint32_t       *status,
                                                           const char    **software_revision);

/******************************************************************************/
/* Get Hardware Revision */

uint8_t *rmf_message_get_hardware_revision_request_new    (void);
uint8_t *rmf_message_get_hardware_revision_response_new   (const char     *hardware_revision);
void     rmf_message_get_hardware_revision_response_parse (const uint8_t  *message,
                                                           uint32_t       *status,
                                                           const char    **hardware_revision);

/******************************************************************************/
/* Get IMEI */

uint8_t *rmf_message_get_imei_request_new    (void);
uint8_t *rmf_message_get_imei_response_new   (const char     *imei);
void     rmf_message_get_imei_response_parse (const uint8_t  *message,
                                              uint32_t       *status,
                                              const char    **imei);

/******************************************************************************/
/* Get IMSI */

uint8_t *rmf_message_get_imsi_request_new    (void);
uint8_t *rmf_message_get_imsi_response_new   (const char     *imsi);
void     rmf_message_get_imsi_response_parse (const uint8_t  *message,
                                              uint32_t       *status,
                                              const char    **imsi);

/******************************************************************************/
/* Get ICCID */

uint8_t *rmf_message_get_iccid_request_new    (void);
uint8_t *rmf_message_get_iccid_response_new   (const char     *iccid);
void     rmf_message_get_iccid_response_parse (const uint8_t  *message,
                                               uint32_t       *status,
                                               const char    **iccid);

/******************************************************************************/
/* Is SIM Locked */

uint8_t *rmf_message_is_sim_locked_request_new    (void);
uint8_t *rmf_message_is_sim_locked_response_new   (uint8_t         locked);
void     rmf_message_is_sim_locked_response_parse (const uint8_t  *message,
                                                   uint32_t       *status,
                                                   uint8_t        *locked);

/******************************************************************************/
/* Unlock */

uint8_t *rmf_message_unlock_request_new    (const char     *pin);
void     rmf_message_unlock_request_parse  (const uint8_t  *message,
                                            const char    **pin);
uint8_t *rmf_message_unlock_response_new   (void);
void     rmf_message_unlock_response_parse (const uint8_t  *message,
                                            uint32_t       *status);

/******************************************************************************/
/* Enable/Disable PIN */

uint8_t *rmf_message_enable_pin_request_new    (uint32_t        enable,
                                                const char     *pin);
void     rmf_message_enable_pin_request_parse  (const uint8_t  *message,
                                                uint32_t       *enable,
                                                const char    **pin);
uint8_t *rmf_message_enable_pin_response_new   (void);
void     rmf_message_enable_pin_response_parse (const uint8_t  *message,
                                                uint32_t       *status);

/******************************************************************************/
/* Change PIN */

uint8_t *rmf_message_change_pin_request_new    (const char     *pin,
                                                const char     *new_pin);
void     rmf_message_change_pin_request_parse  (const uint8_t  *message,
                                                const char    **pin,
                                                const char    **new_pin);
uint8_t *rmf_message_change_pin_response_new   (void);
void     rmf_message_change_pin_response_parse (const uint8_t  *message,
                                                uint32_t       *status);

/******************************************************************************/
/* Get Power Status */

uint8_t *rmf_message_get_power_status_request_new    (void);
uint8_t *rmf_message_get_power_status_response_new   (uint32_t       power_status);
void     rmf_message_get_power_status_response_parse (const uint8_t *message,
                                                      uint32_t      *status,
                                                      uint32_t      *power_status);

/******************************************************************************/
/* Set Power Status */

uint8_t *rmf_message_set_power_status_request_new    (uint32_t       power_status);
void     rmf_message_set_power_status_request_parse  (const uint8_t *message,
                                                      uint32_t      *power_status);
uint8_t *rmf_message_set_power_status_response_new   (void);
void     rmf_message_set_power_status_response_parse (const uint8_t *message,
                                                      uint32_t      *status);

/******************************************************************************/
/* Get Power Info */

uint8_t *rmf_message_get_power_info_request_new    (void);
uint8_t *rmf_message_get_power_info_response_new   (uint32_t       gsm_in_traffic,
                                                    int32_t        gsm_tx_power,
                                                    uint32_t       gsm_rx0_radio_tuned,
                                                    int32_t        gsm_rx0_power,
                                                    uint32_t       gsm_rx1_radio_tuned,
                                                    int32_t        gsm_rx1_power,
                                                    uint32_t       umts_in_traffic,
                                                    int32_t        umts_tx_power,
                                                    uint32_t       umts_rx0_radio_tuned,
                                                    int32_t        umts_rx0_power,
                                                    uint32_t       umts_rx1_radio_tuned,
                                                    int32_t        umts_rx1_power,
                                                    uint32_t       lte_in_traffic,
                                                    int32_t        lte_tx_power,
                                                    uint32_t       lte_rx0_radio_tuned,
                                                    int32_t        lte_rx0_power,
                                                    uint32_t       lte_rx1_radio_tuned,
                                                    int32_t        lte_rx1_power);
void     rmf_message_get_power_info_response_parse (const uint8_t *message,
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
                                                    int32_t       *lte_rx1_power);

/******************************************************************************/
/* Get Signal Info */

uint8_t *rmf_message_get_signal_info_request_new    (void);
uint8_t *rmf_message_get_signal_info_response_new   (uint32_t       gsm_available,
                                                     int32_t        gsm_rssi,
                                                     uint32_t       gsm_quality,
                                                     uint32_t       umts_available,
                                                     int32_t        umts_rssi,
                                                     uint32_t       umts_quality,
                                                     uint32_t       lte_available,
                                                     int32_t        lte_rssi,
                                                     uint32_t       lte_quality);
void     rmf_message_get_signal_info_response_parse (const uint8_t *message,
                                                     uint32_t      *status,
                                                     uint32_t      *gsm_available,
                                                     int32_t       *gsm_rssi,
                                                     uint32_t      *gsm_quality,
                                                     uint32_t      *umts_available,
                                                     int32_t       *umts_rssi,
                                                     uint32_t      *umts_quality,
                                                     uint32_t      *lte_available,
                                                     int32_t      *lte_rssi,
                                                     uint32_t      *lte_quality);

/******************************************************************************/
/* Get Registration Status */

uint8_t *rmf_message_get_registration_status_request_new    (void);
uint8_t *rmf_message_get_registration_status_response_new   (uint32_t        registration_status,
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
uint8_t *rmf_message_get_connection_status_response_new   (uint32_t       connection_status);
void     rmf_message_get_connection_status_response_parse (const uint8_t *message,
                                                           uint32_t      *status,
                                                           uint32_t      *connection_status);

/******************************************************************************/
/* Get Connection Stats */

uint8_t *rmf_message_get_connection_stats_request_new    (void);
uint8_t *rmf_message_get_connection_stats_response_new   (uint32_t       tx_packets_ok,
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

uint8_t *rmf_message_connect_request_new    (const char     *apn,
                                             const char     *user,
                                             const char     *password);
void     rmf_message_connect_request_parse  (const uint8_t  *message,
                                             const char    **apn,
                                             const char    **user,
                                             const char    **password);
uint8_t *rmf_message_connect_response_new   (void);
void     rmf_message_connect_response_parse (const uint8_t  *message,
                                             uint32_t       *status);

/******************************************************************************/
/* Disconnect */

uint8_t *rmf_message_disconnect_request_new    (void);
uint8_t *rmf_message_disconnect_response_new   (void);
void     rmf_message_disconnect_response_parse (const uint8_t *message,
                                                uint32_t      *status);

/******************************************************************************/
/* Modem Is Available */

uint8_t *rmf_message_is_modem_available_request_new    (void);
uint8_t *rmf_message_is_modem_available_response_new   (uint8_t        available);
void     rmf_message_is_modem_available_response_parse (const uint8_t *message,
                                                        uint32_t      *status,
                                                        uint8_t       *available);


#endif /* _RMF_MESSAGES_H_ */

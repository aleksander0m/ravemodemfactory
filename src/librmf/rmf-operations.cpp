// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * librmf
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
 */

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <assert.h>
#include <malloc.h>

#include <stdexcept>

#include "rmf-operations.h"

extern "C" {
#include "rmf-messages.h"
}

using namespace std;
using namespace Modem;

/*****************************************************************************/

static const char *response_status_str[] = {
    "Ok", /* RMF_RESPONSE_STATUS_OK */
    "Unknown error", /* RMF_RESPONSE_STATUS_ERROR_UNKNOWN */
};

/*****************************************************************************/

#define SOCKET_PATH "/tmp/rmf-server"

enum {
    ERROR_NONE,
    ERROR_SOCKET_FAILED,
    ERROR_CONNECT_FAILED,
    ERROR_SEND_FAILED,
    ERROR_POLL_FAILED,
    ERROR_TIMEOUT,
    ERROR_RECV_FAILED,
    ERROR_NO_MATCH,
};

static const char *error_strings[] = {
    "None",
    "Socket failed",
    "Connect failed",
    "Send failed",
    "Poll failed",
    "Timeout",
    "Recv failed",
    "Request and response didn't match"
};

static int
send_and_receive (const uint8_t  *request,
                  uint32_t        timeout_ms,
                  uint8_t       **response)
{
    int ret = ERROR_NONE;
    uint8_t *buffer = NULL;
    uint8_t *ptr;
    ssize_t current;
    size_t left;
    size_t total;
    struct sockaddr_un address;
    struct pollfd fds[1];
    int fd = -1;

    assert (request != NULL);
    assert (response != NULL);
    assert (strlen (SOCKET_PATH) < sizeof (address.sun_path));

    /* Setup address */
    address.sun_family = AF_UNIX;
    strcpy (address.sun_path, SOCKET_PATH);

    /* 1st step: socket(). Create communication endpoint. */
    if ((fd = socket (AF_UNIX, SOCK_STREAM, 0)) < 0) {
        ret = ERROR_SOCKET_FAILED;
        goto failed;
    }

    /* 2nd step: connect(). Give address to the endpoint. */
    if (connect (fd, (const sockaddr*)&address, sizeof (address)) < 0) {
        ret = ERROR_CONNECT_FAILED;
        goto failed;
    }

    /* 3rd step: write(). Send data. */
    left = rmf_message_get_length (request);
    total = 0;
    do {
        if ((current = send (fd, &request[total], left, 0)) < 0) {
            /* We'll just retry on EINTR, not a real error */
            if (errno != EINTR) {
                ret =  ERROR_SEND_FAILED;
                goto failed;
            }
            current = 0;
        }

        assert (left >= current);
        left -= current;
        total += current;
    } while (left > 0);

    /* 4th step: wait for reply, but don't wait forever */
    fds[0].fd = fd;
    fds[0].events= POLLIN | POLLPRI;

    switch (poll (fds, 1, timeout_ms)) {
    case -1:
        ret =  ERROR_POLL_FAILED;
        goto failed;
    case 0:
        ret =  ERROR_TIMEOUT;
        goto failed;
    default:
        /* all good */
        break;
    }

    /* Setup buffer to receive the response; we'll assume it has a max
     * size for now */
    buffer = (uint8_t *) malloc (RMF_MESSAGE_MAX_SIZE);

    /* 5th step: recv() */
    total = 0;
    left = RMF_MESSAGE_MAX_SIZE;
    do {
        if ((current = recv (fd, &buffer[total], left, 0)) < 0) {
            ret = ERROR_RECV_FAILED;
            goto failed;
        }

        assert (left >= current);
        left -= current;
        total += current;
    } while (total < 4 || total < rmf_message_get_length (buffer));

    if (!rmf_message_request_and_response_match (request, buffer)) {
        ret = ERROR_NO_MATCH;
        goto failed;
    }

failed:

    /* 6th step: shutdown() */
    if (fd >= 0)
        shutdown (fd, 2);

    if (buffer) {
        if (ret != ERROR_NONE)
            free (buffer);
        else
            *response = buffer;
    }

    return ret;
}

/*****************************************************************************/

string
Modem::GetManufacturer (void)
{
    uint8_t *request;
    uint8_t *response;
    const char *str;
    uint32_t status;
    int ret;

    request = rmf_message_get_manufacturer_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_manufacturer_response_parse (response, &status, &str);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);

    return str;
}

/*****************************************************************************/

string
Modem::GetModel (void)
{
    uint8_t *request;
    uint8_t *response;
    const char *str;
    uint32_t status;
    int ret;

    request = rmf_message_get_model_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_model_response_parse (response, &status, &str);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);

    return str;
}

/*****************************************************************************/

string
Modem::GetSoftwareRevision (void)
{
    uint8_t *request;
    uint8_t *response;
    const char *str;
    uint32_t status;
    int ret;

    request = rmf_message_get_software_revision_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_software_revision_response_parse (response, &status, &str);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);

    return str;
}

/*****************************************************************************/

string
Modem::GetHardwareRevision (void)

{
    uint8_t *request;
    uint8_t *response;
    const char *str;
    uint32_t status;
    int ret;

    request = rmf_message_get_hardware_revision_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_hardware_revision_response_parse (response, &status, &str);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);

    return str;
}

/*****************************************************************************/

string
Modem::GetImei (void)
{
    uint8_t *request;
    uint8_t *response;
    const char *str;
    uint32_t status;
    int ret;

    request = rmf_message_get_imei_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_imei_response_parse (response, &status, &str);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);

    return str;
}

/*****************************************************************************/

string
Modem::GetImsi (void)
{
    uint8_t *request;
    uint8_t *response;
    const char *str;
    uint32_t status;
    int ret;

    request = rmf_message_get_imsi_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_imsi_response_parse (response, &status, &str);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);

    return str;
}

/*****************************************************************************/

string
Modem::GetIccid (void)
{
    uint8_t *request;
    uint8_t *response;
    const char *str;
    uint32_t status;
    int ret;

    request = rmf_message_get_iccid_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_iccid_response_parse (response, &status, &str);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);

    return str;
}

/*****************************************************************************/

void
Modem::Unlock (const string pin)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    int ret;

    request = rmf_message_unlock_request_new (pin.c_str());
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_unlock_response_parse (response, &status);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);
}

/*****************************************************************************/

void
Modem::EnablePin (bool         enable,
                  const string pin)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    int ret;

    request = rmf_message_enable_pin_request_new ((uint32_t)enable, pin.c_str());
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_enable_pin_response_parse (response, &status);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);
}

/*****************************************************************************/

void
Modem::ChangePin (const string pin,
                  const string new_pin)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    int ret;

    request = rmf_message_change_pin_request_new (pin.c_str(), new_pin.c_str());
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_change_pin_response_parse (response, &status);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);
}

/*****************************************************************************/

PowerStatus
Modem::GetPowerStatus (void)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    uint32_t power_status;
    int ret;

    request = rmf_message_get_power_status_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_power_status_response_parse (response, &status, &power_status);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);

    return (PowerStatus) power_status;
}

/*****************************************************************************/

void
Modem::SetPowerStatus (PowerStatus power_status)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    int ret;

    request = rmf_message_set_power_status_request_new ((uint32_t)power_status);
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_set_power_status_response_parse (response, &status);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);
}

/*****************************************************************************/

vector<RadioPowerInfo>
Modem::GetPowerInfo (void)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    std::vector<RadioPowerInfo> info_vector;
    RadioPowerInfo info;
    uint32_t gsm_in_traffic;
    uint32_t gsm_tx_power;
    uint32_t gsm_rx0_radio_tuned;
    uint32_t gsm_rx0_power;
    uint32_t gsm_rx1_radio_tuned;
    uint32_t gsm_rx1_power;
    uint32_t umts_in_traffic;
    uint32_t umts_tx_power;
    uint32_t umts_rx0_radio_tuned;
    uint32_t umts_rx0_power;
    uint32_t umts_rx1_radio_tuned;
    uint32_t umts_rx1_power;
    uint32_t lte_in_traffic;
    uint32_t lte_tx_power;
    uint32_t lte_rx0_radio_tuned;
    uint32_t lte_rx0_power;
    uint32_t lte_rx1_radio_tuned;
    uint32_t lte_rx1_power;
    int ret;

    request = rmf_message_get_power_info_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_power_info_response_parse (
        response,
        &status,
        &gsm_in_traffic,
        &gsm_tx_power,
        &gsm_rx0_radio_tuned,
        &gsm_rx0_power,
        &gsm_rx1_radio_tuned,
        &gsm_rx1_power,
        &umts_in_traffic,
        &umts_tx_power,
        &umts_rx0_radio_tuned,
        &umts_rx0_power,
        &umts_rx1_radio_tuned,
        &umts_rx1_power,
        &lte_in_traffic,
        &lte_tx_power,
        &lte_rx0_radio_tuned,
        &lte_rx0_power,
        &lte_rx1_radio_tuned,
        &lte_rx1_power);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);

    /* Note: power values come in scale of 0.1 dBm */

    /* GSM */
    if (gsm_in_traffic || gsm_rx0_radio_tuned || gsm_rx1_radio_tuned) {
        info.radioInterface = Gsm;
        info.inTraffic = gsm_in_traffic;
        info.txPower = (0.1) * ((double)gsm_tx_power);
        info.rx0RadioTuned = gsm_rx0_radio_tuned;
        info.rx0Power = (0.1) * ((double)gsm_rx0_power);
        info.rx1RadioTuned = gsm_rx1_radio_tuned;
        info.rx1Power = (0.1) * ((double)gsm_rx1_power);
        info_vector.push_back (info);
    }

    /* UMTS */
    if (umts_in_traffic || umts_rx0_radio_tuned || umts_rx1_radio_tuned) {
        info.radioInterface = Umts;
        info.inTraffic = umts_in_traffic;
        info.txPower = (0.1) * ((double)umts_tx_power);
        info.rx0RadioTuned = umts_rx0_radio_tuned;
        info.rx0Power = (0.1) * ((double)umts_rx0_power);
        info.rx1RadioTuned = umts_rx1_radio_tuned;
        info.rx1Power = (0.1) * ((double)umts_rx1_power);
        info_vector.push_back (info);
    }

    /* LTE */
    if (lte_in_traffic || lte_rx0_radio_tuned || lte_rx1_radio_tuned) {
        info.radioInterface = Lte;
        info.inTraffic = lte_in_traffic;
        info.txPower = (0.1) * ((double)lte_tx_power);
        info.rx0RadioTuned = lte_rx0_radio_tuned;
        info.rx0Power = (0.1) * ((double)lte_rx0_power);
        info.rx1RadioTuned = lte_rx1_radio_tuned;
        info.rx1Power = (0.1) * ((double)lte_rx1_power);
        info_vector.push_back (info);
    }

    return info_vector;
}

/*****************************************************************************/

vector<RadioSignalInfo>
Modem::GetSignalInfo (void)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    std::vector<RadioSignalInfo> info_vector;
    RadioSignalInfo info;
    uint32_t gsm_available;
    uint32_t gsm_rssi;
    uint32_t gsm_quality;
    uint32_t umts_available;
    uint32_t umts_rssi;
    uint32_t umts_quality;
    uint32_t lte_available;
    uint32_t lte_rssi;
    uint32_t lte_quality;
    int ret;

    request = rmf_message_get_signal_info_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_signal_info_response_parse (
        response,
        &status,
        &gsm_available,
        &gsm_rssi,
        &gsm_quality,
        &umts_available,
        &umts_rssi,
        &umts_quality,
        &lte_available,
        &lte_rssi,
        &lte_quality);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);

    /* GSM */
    if (gsm_available) {
        info.radioInterface = Gsm;
        info.rssi = (int32_t)gsm_rssi;;
        info.quality = gsm_quality;
        info_vector.push_back (info);
    }

    /* UMTS */
    if (umts_available) {
        info.radioInterface = Umts;
        info.rssi = (int32_t)umts_rssi;;
        info.quality = umts_quality;
        info_vector.push_back (info);
    }

    /* LTE */
    if (lte_available) {
        info.radioInterface = Lte;
        info.rssi = (int32_t)lte_rssi;;
        info.quality = lte_quality;
        info_vector.push_back (info);
    }

    return info_vector;
}

/*****************************************************************************/

RegistrationStatus
Modem::GetRegistrationStatus (string   &operatorDescription,
                              uint16_t &operatorMcc,
                              uint16_t &operatorMnc,
                              uint16_t &lac,
                              uint32_t &cid)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    uint32_t registration_status;
    const char *operator_description;
    uint32_t operator_mcc;
    uint32_t operator_mnc;
    uint32_t _lac;
    uint32_t _cid;
    int ret;

    request = rmf_message_get_registration_status_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_registration_status_response_parse (
        response,
        &status,
        &registration_status,
        &operator_description,
        &operator_mcc,
        &operator_mnc,
        &_lac,
        &_cid);

    if (status != RMF_RESPONSE_STATUS_OK) {
        free (response);
        throw std::runtime_error (response_status_str[status]);
    }

    operatorDescription = operator_description;
    operatorMcc = (uint16_t)operator_mcc;
    operatorMnc = (uint16_t)operator_mnc;
    lac = (uint16_t)_lac;
    cid = cid;

    free (response);

    return (RegistrationStatus)registration_status;
}

/*****************************************************************************/

ConnectionStatus
Modem::GetConnectionStatus (void)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    uint32_t connection_status;
    int ret;

    request = rmf_message_get_connection_status_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_connection_status_response_parse (
        response,
        &status,
        &connection_status);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);

    return (ConnectionStatus) connection_status;
}

/*****************************************************************************/

bool
Modem::GetConnectionStats (uint32_t &txPacketsOk,
                           uint32_t &rxPacketsOk,
                           uint32_t &txPacketsError,
                           uint32_t &rxPacketsError,
                           uint32_t &txPacketsOverflow,
                           uint32_t &rxPacketsOverflow,
                           uint64_t &txBytesOk,
                           uint64_t &rxBytesOk)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    uint32_t tx_packets_ok;
    uint32_t rx_packets_ok;
    uint32_t tx_packets_error;
    uint32_t rx_packets_error;
    uint32_t tx_packets_overflow;
    uint32_t rx_packets_overflow;
    uint64_t tx_bytes_ok;
    uint64_t rx_bytes_ok;
    int ret;

    request = rmf_message_get_connection_stats_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_connection_stats_response_parse (
        response,
        &status,
        &tx_packets_ok,
        &rx_packets_ok,
        &tx_packets_error,
        &rx_packets_error,
        &tx_packets_overflow,
        &rx_packets_overflow,
        &tx_bytes_ok,
        &rx_bytes_ok);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);

    txPacketsOk = tx_packets_ok;
    rxPacketsOk = rx_packets_ok;
    txPacketsError = tx_packets_error;
    rxPacketsError = rx_packets_error;
    txPacketsOverflow = tx_packets_overflow;
    rxPacketsOverflow = rx_packets_overflow;
    txBytesOk = tx_bytes_ok;
    rxBytesOk = rx_bytes_ok;

    return true;
}


/*****************************************************************************/

void
Modem::Connect (const string apn,
                const string user,
                const string password)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    int ret;

    request = rmf_message_connect_request_new (apn.c_str(),
                                               user.c_str(),
                                               password.c_str());
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_connect_response_parse (response, &status);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);
}

/*****************************************************************************/

void
Modem::Disconnect (void)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    int ret;

    request = rmf_message_disconnect_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_disconnect_response_parse (response, &status);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw std::runtime_error (response_status_str[status]);
}

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
 * Copyright (C) 2013-2020 Safran Passenger Innovations
 *
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#include <config.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <assert.h>
#include <malloc.h>
#include <fcntl.h>

#include <stdexcept>

#include "rmf-operations.h"

extern "C" {
#include "rmf-messages.h"
}

using namespace std;
using namespace Modem;

/*****************************************************************************/

static const char *response_status_str[] = {
    "Ok",              /* RMF_RESPONSE_STATUS_OK */
    "Unknown error",   /* RMF_RESPONSE_STATUS_ERROR_UNKNOWN */
    "Invalid request", /* RMF_RESPONSE_STATUS_ERROR_INVALID_REQUEST */
    "Unknown command", /* RMF_RESPONSE_STATUS_ERROR_UNKNOWN_COMMAND */
    "No modem",        /* RMF_RESPONSE_STATUS_ERROR_NO_MODEM */
    "Invalid state",   /* RMF_RESPONSE_STATUS_ERROR_INVALID_STATE */
    "Invalid input",   /* RMF_RESPONSE_STATUS_ERROR_INVALID_INPUT */
};

static const char *qmi_response_status_str[] = {
    "", /* 100 unused, no qmi error */
    "Malformed message", /* 101, RMF_RESPONSE_STATUS_ERROR_MALFORMED_MESSAGE */
    "No memory", /* 102, RMF_RESPONSE_STATUS_ERROR_NO_MEMORY */
    "Internal", /* 103, RMF_RESPONSE_STATUS_ERROR_INTERNAL */
    "Aborted", /* 104, RMF_RESPONSE_STATUS_ERROR_ABORTED */
    "Client IDs exhausted", /* 105, RMF_RESPONSE_STATUS_ERROR_CLIENT_IDS_EXHAUSTED */
    "Unabortable transaction", /* 106, RMF_RESPONSE_STATUS_ERROR_UNABORTABLE_TRANSACTION */
    "Invalid client ID", /* 107, RMF_RESPONSE_STATUS_ERROR_INVALID_CLIENT_ID */
    "No thresholds provided", /* 108, RMF_RESPONSE_STATUS_ERROR_NO_THRESHOLDS_PROVIDED */
    "Invalid handle", /* 109, RMF_RESPONSE_STATUS_ERROR_INVALID_HANDLE */
    "Invalid profile", /* 110, RMF_RESPONSE_STATUS_ERROR_INVALID_PROFILE */
    "Invalid PIN ID", /* 111, RMF_RESPONSE_STATUS_ERROR_INVALID_PIN_ID */
    "Incorrect PIN", /* 112, RMF_RESPONSE_STATUS_ERROR_INCORRECT_PIN */
    "No network found", /* 113, RMF_RESPONSE_STATUS_ERROR_NO_NETWORK_FOUND */
    "Call failed", /* 114, RMF_RESPONSE_STATUS_ERROR_CALL_FAILED */
    "Out of call", /* 115, RMF_RESPONSE_STATUS_ERROR_OUT_OF_CALL */
    "Not provisioned", /* 116, RMF_RESPONSE_STATUS_ERROR_NOT_PROVISIONED */
    "Missing argument", /* 117, RMF_RESPONSE_STATUS_ERROR_MISSING_ARGUMENT */
    "", /* 118 */
    "Argument too long", /* 119, RMF_RESPONSE_STATUS_ERROR_ARGUMENT_TOO_LONG */
    "", /* 120 */
    "", /* 121 */
    "Invalid transaction ID", /* 122, RMF_RESPONSE_STATUS_ERROR_INVALID_TRANSACTION_ID */
    "Device in use", /* 123, RMF_RESPONSE_STATUS_ERROR_DEVICE_IN_USE */
    "Network unsupported", /* 124, RMF_RESPONSE_STATUS_ERROR_NETWORK_UNSUPPORTED */
    "Device unsupported", /* 125, RMF_RESPONSE_STATUS_ERROR_DEVICE_UNSUPPORTED */
    "No effect", /* 126, RMF_RESPONSE_STATUS_ERROR_NO_EFFECT */
    "No free profile", /* 127, RMF_RESPONSE_STATUS_ERROR_NO_FREE_PROFILE */
    "Invalid PDP type", /* 128, RMF_RESPONSE_STATUS_ERROR_INVALID_PDP_TYPE */
    "Invalid technology preference", /* 129, RMF_RESPONSE_STATUS_ERROR_INVALID_TECHNOLOGY_PREFERENCE */
    "Invalid profile type", /* 130, RMF_RESPONSE_STATUS_ERROR_INVALID_PROFILE_TYPE */
    "Invalid service type", /* 131, RMF_RESPONSE_STATUS_ERROR_INVALID_SERVICE_TYPE */
    "Invalid register action", /* 132, RMF_RESPONSE_STATUS_ERROR_INVALID_REGISTER_ACTION */
    "Invalid PS attach action", /* 133, RMF_RESPONSE_STATUS_ERROR_INVALID_PS_ATTACH_ACTION */
    "Authentication failed", /* 134, RMF_RESPONSE_STATUS_ERROR_AUTHENTICATION_FAILED */
    "PIN blocked", /* 135, RMF_RESPONSE_STATUS_ERROR_PIN_BLOCKED */
    "PIN always blocked", /* 136, RMF_RESPONSE_STATUS_ERROR_PIN_ALWAYS_BLOCKED */
    "UIM uninitialized", /* 137, RMF_RESPONSE_STATUS_ERROR_UIM_UNINITIALIZED */
    "QoS requests in use", /* 138, RMF_RESPONSE_STATUS_ERROR_MAXIMUM_QOS_REQUESTS_IN_USE */
    "Incorrect flow filter", /* 139, RMF_RESPONSE_STATUS_ERROR_INCORRECT_FLOW_FILTER */
    "Network QoS unaware", /* 140, RMF_RESPONSE_STATUS_ERROR_NETWORK_QOS_UNAWARE */
    "Invalid QoS ID", /* 141, RMF_RESPONSE_STATUS_ERROR_INVALID_QOS_ID */
    "QoS unavailable", /* 142, RMF_RESPONSE_STATUS_ERROR_QOS_UNAVAILABLE */
    "Interface not found", /* 143, RMF_RESPONSE_STATUS_ERROR_INTERFACE_NOT_FOUND */
    "Flow suspended", /* 144, RMF_RESPONSE_STATUS_ERROR_FLOW_SUSPENDED */
    "Invalid data format", /* 145, RMF_RESPONSE_STATUS_ERROR_INVALID_DATA_FORMAT */
    "General error", /* 146, RMF_RESPONSE_STATUS_ERROR_GENERAL_ERROR */
    "Unknown error", /* 147, RMF_RESPONSE_STATUS_ERROR_UNKNOWN_ERROR */
    "Invalid argument", /* 148, RMF_RESPONSE_STATUS_ERROR_INVALID_ARGUMENT */
    "Invalid index", /* 149, RMF_RESPONSE_STATUS_ERROR_INVALID_INDEX */
    "No entry", /* 150, RMF_RESPONSE_STATUS_ERROR_NO_ENTRY */
    "Device storage full", /* 151, RMF_RESPONSE_STATUS_ERROR_DEVICE_STORAGE_FULL */
    "Device not ready", /* 152, RMF_RESPONSE_STATUS_ERROR_DEVICE_NOT_READY */
    "Network not ready", /* 153, RMF_RESPONSE_STATUS_ERROR_NETWORK_NOT_READY */
    "WMS cause code", /* 154, RMF_RESPONSE_STATUS_ERROR_WMS_CAUSE_CODE */
    "WMS message not sent", /* 155, RMF_RESPONSE_STATUS_ERROR_WMS_MESSAGE_NOT_SENT */
    "WMS message delivery failure", /* 156, RMF_RESPONSE_STATUS_ERROR_WMS_MESSAGE_DELIVERY_FAILURE */
    "WMS invalid message ID", /* 157, RMF_RESPONSE_STATUS_ERROR_WMS_INVALID_MESSAGE_ID */
    "WMS encoding", /* 158, RMF_RESPONSE_STATUS_ERROR_WMS_ENCODING */
    "Authentication lock", /* 159, RMF_RESPONSE_STATUS_ERROR_AUTHENTICATION_LOCK */
    "Invalid transition", /* 160, RMF_RESPONSE_STATUS_ERROR_INVALID_TRANSITION */
    "Not multicast interface", /* 161, RMF_RESPONSE_STATUS_ERROR_NOT_MCAST_INTERFACE */
    "Maximum multicast requests in use", /* 162, RMF_RESPONSE_STATUS_ERROR_MAXIMUM_MCAST_REQUESTS_IN_USE */
    "Invalid multicast handle", /* 163, RMF_RESPONSE_STATUS_ERROR_INVALID_MCAST_HANDLE */
    "Invalid IP family preference", /* 164, RMF_RESPONSE_STATUS_ERROR_INVALID_IP_FAMILY_PREFERENCE */
    "Session inactive", /* 165, RMF_RESPONSE_STATUS_ERROR_SESSION_INACTIVE */
    "Session invalid", /* 166, RMF_RESPONSE_STATUS_ERROR_SESSION_INVALID */
    "Session ownership", /* 167, RMF_RESPONSE_STATUS_ERROR_SESSION_OWNERSHIP */
    "Insufficient resources", /* 168, RMF_RESPONSE_STATUS_ERROR_INSUFFICIENT_RESOURCES */
    "Disabled", /* 169, RMF_RESPONSE_STATUS_ERROR_DISABLED */
    "Invalid operation", /* 170, RMF_RESPONSE_STATUS_ERROR_INVALID_OPERATION */
    "Invalid QMI command", /* 171, RMF_RESPONSE_STATUS_ERROR_INVALID_QMI_COMMAND */
    "WMS T-PDU type", /* 172, RMF_RESPONSE_STATUS_ERROR_WMS_T_PDU_TYPE */
    "WMS SMSC address", /* 173, RMF_RESPONSE_STATUS_ERROR_WMS_SMSC_ADDRESS */
    "Information unavailable", /* 174, RMF_RESPONSE_STATUS_ERROR_INFORMATION_UNAVAILABLE */
    "Segment too long", /* 175, RMF_RESPONSE_STATUS_ERROR_SEGMENT_TOO_LONG */
    "Segment order", /* 176, RMF_RESPONSE_STATUS_ERROR_SEGMENT_ORDER */
    "Bundling not supported", /* 177, RMF_RESPONSE_STATUS_ERROR_BUNDLING_NOT_SUPPORTED */
    "Partial failure", /* 178, RMF_RESPONSE_STATUS_ERROR_PARTIAL_FAILURE */
    "Policy mismatch", /* 179, RMF_RESPONSE_STATUS_ERROR_POLICY_MISMATCH */
    "SIM file not found", /* 180, RMF_RESPONSE_STATUS_ERROR_SIM_FILE_NOT_FOUND */
    "Extended internal", /* 181, RMF_RESPONSE_STATUS_ERROR_EXTENDED_INTERNAL */
    "Access denied", /* 182, RMF_RESPONSE_STATUS_ERROR_ACCESS_DENIED */
    "Hardware restricted", /* 183, RMF_RESPONSE_STATUS_ERROR_HARDWARE_RESTRICTED */
    "ACK not sent", /* 184, RMF_RESPONSE_STATUS_ERROR_ACK_NOT_SENT */
    "Inject timeout", /* 185, RMF_RESPONSE_STATUS_ERROR_INJECT_TIMEOUT */
    "", /* 186 */
    "", /* 187 */
    "", /* 188 */
    "", /* 189 */
    "Incompatible state", /* 190, RMF_RESPONSE_STATUS_ERROR_INCOMPATIBLE_STATE */
    "FDN restrict", /* 191, RMF_RESPONSE_STATUS_ERROR_FDN_RESTRICT */
    "SUPS failure case", /* 192, RMF_RESPONSE_STATUS_ERROR_SUPS_FAILURE_CASE */
    "No radio", /* 193, RMF_RESPONSE_STATUS_ERROR_NO_RADIO */
    "Not supported", /* 194, RMF_RESPONSE_STATUS_ERROR_NOT_SUPPORTED */
    "No subscription", /* 195, RMF_RESPONSE_STATUS_ERROR_NO_SUBSCRIPTION */
    "Card call control failed", /* 196, RMF_RESPONSE_STATUS_ERROR_CARD_CALL_CONTROL_FAILED */
    "Network aborted", /* 197, RMF_RESPONSE_STATUS_ERROR_NETWORK_ABORTED */
    "Msg blocked", /* 198, RMF_RESPONSE_STATUS_ERROR_MSG_BLOCKED */
    "", /* 199 */
    "Invalid session type", /* 200, RMF_RESPONSE_STATUS_ERROR_INVALID_SESSION_TYPE */
    "Invalid phonebook type", /* 201, RMF_RESPONSE_STATUS_ERROR_INVALID_PB_TYPE */
    "No SIM", /* 202, RMF_RESPONSE_STATUS_ERROR_NO_SIM */
    "Phonebook not ready", /* 203, RMF_RESPONSE_STATUS_ERROR_PB_NOT_READY */
    "PIN restriction", /* 204, RMF_RESPONSE_STATUS_ERROR_PIN_RESTRICTION */
    "PIN2 restriction", /* 205, RMF_RESPONSE_STATUS_ERROR_PIN2_RESTRICTION */
    "PUK restriction", /* 206, RMF_RESPONSE_STATUS_ERROR_PUK_RESTRICTION */
    "PUK2 restriction", /* 207, RMF_RESPONSE_STATUS_ERROR_PUK2_RESTRICTION */
    "Phonebook access restricted", /* 208, RMF_RESPONSE_STATUS_ERROR_PB_ACCESS_RESTRICTED */
    "Phonebook delete in progress", /* 209, RMF_RESPONSE_STATUS_ERROR_PB_DELETE_IN_PROGRESS */
    "Phonebook text too long", /* 210, RMF_RESPONSE_STATUS_ERROR_PB_TEXT_TOO_LONG */
    "Phonebook number too long", /* 211, RMF_RESPONSE_STATUS_ERROR_PB_NUMBER_TOO_LONG */
    "Phonebook hidden key restriction", /* 212, RMF_RESPONSE_STATUS_ERROR_PB_HIDDEN_KEY_RESTRICTION */
    "Phonebook not available", /* 213, RMF_RESPONSE_STATUS_ERROR_PB_NOT_AVAILABLE */
    "Device memory error", /* 214, RMF_RESPONSE_STATUS_ERROR_DEVICE_MEMORY_ERROR */
    "No permission", /* 215, RMF_RESPONSE_STATUS_ERROR_NO_PERMISSION */
    "Too soon", /* 216, RMF_RESPONSE_STATUS_ERROR_TOO_SOON */
    "Time not acquired", /* 217, RMF_RESPONSE_STATUS_ERROR_TIME_NOT_ACQUIRED */
    "Operation in progress", /* 218, RMF_RESPONSE_STATUS_ERROR_OPERATION_IN_PROGRESS */
};

/*****************************************************************************/

static bool     target_remote;
static string   target_address;
static uint16_t target_port;

bool
Modem::SetTargetRemote (const string address,
                        uint16_t     port)
{
    target_remote  = true;
    target_address = address;
    target_port    = port;
    return true;
}

bool
Modem::SetTargetLocal (void)
{
    target_remote  = false;
    target_address = "";
    target_port    = 0;
    return true;
}

/*****************************************************************************/

enum {
    ERROR_NONE,
    ERROR_SOCKET_FAILED,
    ERROR_CONNECT_FAILED,
    ERROR_SEND_FAILED,
    ERROR_POLL_FAILED,
    ERROR_TIMEOUT,
    ERROR_CHANNEL_ERROR,
    ERROR_CHANNEL_HUP,
    ERROR_RECV_FAILED,
    ERROR_NO_MATCH,
    ERROR_NO_MEMORY,
    ERROR_RECV_NOT_FULL,
    ERROR_INVALID_MSG_LENGTH,
    ERROR_N
};

static const char *error_strings[] = {
    "None",
    "Socket failed",
    "Connect failed",
    "Send failed",
    "Poll failed",
    "Timeout",
    "Error detected in channel",
    "Remote closed channel",
    "Recv failed",
    "Request and response didn't match",
    "No memory",
    "Full message not received",
    "Invalid message length",
};

/* We'll wait up to 1s for the connection to be established */
#define DEFAULT_CONNECT_TIMEOUT_SEC 1

/* We'll wait up to 1s for the full response once the start has been received */
#define DEFAULT_RECV_TIMEOUT_SEC 1

/* Up to 1000 retries if EINTR is received in send() */
#define MAX_EINTR_RETRIES 1000

static int
send_and_receive (const uint8_t  *request,
                  uint32_t        timeout_s,
                  uint8_t       **response)
{
    int ret = ERROR_NONE;
    uint8_t *buffer = NULL;
    ssize_t current;
    size_t left;
    size_t total;
    struct pollfd fds[1];
    int fd = -1;
    uint32_t max_eintr_retries = MAX_EINTR_RETRIES;

    assert (request != NULL);
    assert (response != NULL);

#if defined HAVE_CXX11
    static_assert ((sizeof (error_strings) / sizeof (error_strings[0])) == ERROR_N, "missing error strings");
#endif

    /* Operation on local unix socket */
    if (!target_remote) {
        struct sockaddr_un address;

        assert (strlen (RMFD_SOCKET_PATH) < sizeof (address.sun_path));

        /* Setup address */
        memset (&address, 0, sizeof (address));
        address.sun_family = AF_UNIX;
        strcpy (address.sun_path, RMFD_SOCKET_PATH);

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
    }
    /* Operation on remote TCP socket */
    else {
        struct sockaddr_in address;
        int so_error = -1;

        /* Setup address */
        memset (&address, 0, sizeof (address));
        address.sin_family = AF_INET;
        if (inet_aton (target_address.c_str(), &address.sin_addr) == 0) {
            ret = ERROR_SOCKET_FAILED;
            goto failed;
        }
        address.sin_port = htons (target_port);

        /* 1st step: socket(). Create communication endpoint. */
        if ((fd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
            ret = ERROR_SOCKET_FAILED;
            goto failed;
        }

        /* 2nd step: connect(). Give address to the endpoint.
         * We set the socket in non-blocking mode during the connect operation
         * so that we can poll for readiness ourselves, providing a maximum
         * timeout. */
        fcntl (fd, F_SETFL, O_NONBLOCK);
        connect (fd, (const struct sockaddr *)&address, sizeof (address));

        /* Connection is completed when socket is ready for writing */
        fds[0].fd = fd;
        fds[0].events = POLLOUT;
        if (poll (fds, 1, 1000 * DEFAULT_CONNECT_TIMEOUT_SEC) > 0) {
            socklen_t len;

            /* Read socket status */
            len = sizeof (so_error);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
        }
        /* If timed out, or socket actually not connected, fail miserably */
        if (so_error != 0) {
            ret = ERROR_CONNECT_FAILED;
            goto failed;
        }
        /* back to blocking mode */
        fcntl (fd, F_SETFL, 0);
    }

    /* 3rd step: write(). Send data. */
    left = rmf_message_get_length (request);
    total = 0;
    do {
        if ((current = send (fd, &request[total], left, 0)) < 0) {
            /* We'll just retry on EINTR, not a real error */
            if (errno != EINTR || max_eintr_retries == 0) {
                ret =  ERROR_SEND_FAILED;
                goto failed;
            }
            max_eintr_retries--;
            current = 0;
        }

        assert (((ssize_t) left) >= current);
        left -= current;
        total += current;
    } while (left > 0);

    /* 4th step: wait for reply, but don't wait forever */
    fds[0].fd = fd;
    fds[0].events = POLLIN | POLLPRI | POLLERR | POLLHUP;

    switch (poll (fds, 1, 1000 * timeout_s)) {
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

    if (fds[0].revents & POLLIN || fds[0].revents & POLLPRI) {
        struct timeval recv_timeout_tv;
        uint32_t message_size;

        /* Setup buffer to receive the response; we'll assume it has a max
         * size for now */
        buffer = (uint8_t *) malloc (RMF_MESSAGE_MAX_SIZE);
        if (!buffer) {
            ret = ERROR_NO_MEMORY;
            goto failed;
        }

        /* 5th step: recv(). We first try to read 4 bytes, as that includes the
         * full expected message length; then we read the remaining bytes.
         *
         * This step will finish in any of these actions:
         *  - The full message has been received.
         *  - The server closes socket.
         *  - The recv timeout happens.
         */

        /* Set default recv() timeout, so that we don't block forever waiting
         * for the server response */
        recv_timeout_tv.tv_sec = DEFAULT_RECV_TIMEOUT_SEC;
        recv_timeout_tv.tv_usec = 0;
        setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &recv_timeout_tv, sizeof (struct timeval));

        /* Peek 4 bytes for the message length */
        if ((current = recv (fd, buffer, sizeof (uint32_t), MSG_PEEK)) != sizeof (uint32_t)) {
            ret = ERROR_RECV_FAILED;
            goto failed;
        }

        /* Build expected message length and error out if unexpected */
        message_size = rmf_message_get_length (buffer);
        if (message_size > RMF_MESSAGE_MAX_SIZE) {
            ret = ERROR_INVALID_MSG_LENGTH;
            goto failed;
        }

        /* Try to read the full message length. Operation is blocking but with
         * a timeout. */
        if ((current = recv (fd, buffer, message_size, 0)) != message_size) {
            ret = ERROR_RECV_NOT_FULL;
            goto failed;
        }

        if (!rmf_message_request_and_response_match (request, buffer)) {
            ret = ERROR_NO_MATCH;
            goto failed;
        }
    } else if (fds[0].revents & POLLHUP) {
        ret = ERROR_CHANNEL_HUP;
        goto failed;
    } else if (fds[0].revents & POLLERR) {
        ret = ERROR_CHANNEL_ERROR;
        goto failed;
    }

failed:

    /* 6th step: shutdown() */
    if (fd >= 0)
        close (fd);

    if (buffer) {
        if (ret != ERROR_NONE)
            free (buffer);
        else
            *response = buffer;
    }

    return ret;
}

#define response_error_string(status)                                   \
    ((status < 100) ?                                                   \
     ((status < (sizeof (response_status_str) / sizeof (response_status_str[0]))) ? response_status_str[status] : "<invalid>") : \
     qmi_response_status_str[status - 100])

#define throw_response_error(status) do {                       \
    throw std::runtime_error (response_error_string (status));  \
    } while (0)

#define throw_verbose_response_error(status,str) do { \
    string s(response_error_string (status));         \
    s.append(": ");                                   \
    s.append(str);                                    \
    throw std::runtime_error (s);                     \
    } while (0)

/*****************************************************************************/

string
Modem::GetManufacturer (void)
{
    uint8_t *request;
    uint8_t *response;
    const char *str;
    uint32_t status;
    int ret;
    string result;

    request = rmf_message_get_manufacturer_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_manufacturer_response_parse (response, &status, &str);
    if (status != RMF_RESPONSE_STATUS_OK) {
        free (response);
        throw_response_error (status);
    }

    result = str;
    free (response);

    return result;
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
    string result;

    request = rmf_message_get_model_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_model_response_parse (response, &status, &str);
    if (status != RMF_RESPONSE_STATUS_OK) {
        free (response);
        throw_response_error (status);
    }

    result = str;
    free (response);

    return result;
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
    string result;

    request = rmf_message_get_software_revision_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_software_revision_response_parse (response, &status, &str);
    if (status != RMF_RESPONSE_STATUS_OK) {
        free (response);
        throw_response_error (status);
    }

    result = str;
    free (response);

    return result;
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
    string result;

    request = rmf_message_get_hardware_revision_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_hardware_revision_response_parse (response, &status, &str);
    if (status != RMF_RESPONSE_STATUS_OK) {
        free (response);
        throw_response_error (status);
    }

    result = str;
    free (response);

    return result;
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
    string result;

    request = rmf_message_get_imei_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_imei_response_parse (response, &status, &str);
    if (status != RMF_RESPONSE_STATUS_OK) {
        free (response);
        throw_response_error (status);
    }

    result = str;
    free (response);

    return result;
}

/*****************************************************************************/

uint8_t
Modem::GetSimSlot (void)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    int ret;
    uint8_t result;

    request = rmf_message_get_sim_slot_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_sim_slot_response_parse (response, &status, &result);
    if (status != RMF_RESPONSE_STATUS_OK) {
        free (response);
        throw_response_error (status);
    }

    free (response);
    return result;
}

/*****************************************************************************/

void
Modem::SetSimSlot (uint8_t slot)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    int ret;
    uint8_t result;

    request = rmf_message_set_sim_slot_request_new (slot);
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_set_sim_slot_response_parse (response, &status);
    if (status != RMF_RESPONSE_STATUS_OK) {
        free (response);
        throw_response_error (status);
    }
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
    string result;

    request = rmf_message_get_imsi_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_imsi_response_parse (response, &status, &str);
    if (status != RMF_RESPONSE_STATUS_OK) {
        free (response);
        throw_response_error (status);
    }

    result = str;
    free (response);

    return result;
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
    string result;

    request = rmf_message_get_iccid_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_iccid_response_parse (response, &status, &str);
    if (status != RMF_RESPONSE_STATUS_OK) {
        free (response);
        throw_response_error (status);
    }

    result = str;
    free (response);

    return result;
}

/*****************************************************************************/

void
Modem::GetSimInfo (uint16_t &operatorMcc,
                   uint16_t &operatorMnc,
                   std::vector<struct PlmnInfo>&plmns)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    uint32_t operator_mcc;
    uint32_t operator_mnc;
    int ret;
    RmfPlmnInfo *infos;
    uint32_t n_infos;
    uint32_t i;

    request = rmf_message_get_sim_info_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_sim_info_response_parse (
        response,
        &status,
        &operator_mcc,
        &operator_mnc,
        &n_infos,
        &infos);

    if (status != RMF_RESPONSE_STATUS_OK) {
        free (response);
        throw_response_error (status);
    }

    operatorMcc = (uint16_t)operator_mcc;
    operatorMnc = (uint16_t)operator_mnc;
    for (i = 0; i < n_infos; i++) {
        struct PlmnInfo plmn;

        plmn.mcc  = (uint16_t)infos[i].mcc;
        plmn.mnc  = (uint16_t)infos[i].mnc;
        plmn.gsm  = (bool)infos[i].gsm;
        plmn.umts = (bool)infos[i].umts;
        plmn.lte  = (bool)infos[i].lte;
        plmns.push_back (plmn);
    }

    if (infos)
        free (infos);

    free (response);
}

/*****************************************************************************/

bool
Modem::IsSimLocked (void)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    uint8_t locked;
    int ret;

    request = rmf_message_is_sim_locked_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_is_sim_locked_response_parse (response, &status, &locked);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw_response_error (status);

    return (bool)locked;
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
        throw_response_error (status);
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
        throw_response_error (status);
}

/*****************************************************************************/

void
Modem::ChangePin (const string pin,
                  const string newPin)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    int ret;

    request = rmf_message_change_pin_request_new (pin.c_str(), newPin.c_str());
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_change_pin_response_parse (response, &status);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw_response_error (status);
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
        throw_response_error (status);

    return (PowerStatus) power_status;
}

/*****************************************************************************/

void
Modem::SetPowerStatus (PowerStatus powerStatus)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    int ret;

    request = rmf_message_set_power_status_request_new ((uint32_t)powerStatus);
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_set_power_status_response_parse (response, &status);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw_response_error (status);
}

/*****************************************************************************/

void
Modem::PowerCycle (void)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    int ret;

    request = rmf_message_power_cycle_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_power_cycle_response_parse (response, &status);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw_response_error (status);
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
    int32_t gsm_tx_power;
    uint32_t gsm_rx0_radio_tuned;
    int32_t gsm_rx0_power;
    uint32_t gsm_rx1_radio_tuned;
    int32_t gsm_rx1_power;
    uint32_t umts_in_traffic;
    int32_t umts_tx_power;
    uint32_t umts_rx0_radio_tuned;
    int32_t umts_rx0_power;
    uint32_t umts_rx1_radio_tuned;
    int32_t umts_rx1_power;
    uint32_t lte_in_traffic;
    int32_t lte_tx_power;
    uint32_t lte_rx0_radio_tuned;
    int32_t lte_rx0_power;
    uint32_t lte_rx1_radio_tuned;
    int32_t lte_rx1_power;
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
        throw_response_error (status);

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
    int32_t gsm_rssi;
    uint32_t gsm_quality;
    uint32_t umts_available;
    int32_t umts_rssi;
    uint32_t umts_quality;
    uint32_t lte_available;
    int32_t lte_rssi;
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
        throw_response_error (status);

    /* GSM */
    if (gsm_available) {
        info.radioInterface = Gsm;
        info.rssi = gsm_rssi;
        info.quality = gsm_quality;
        info_vector.push_back (info);
    }

    /* UMTS */
    if (umts_available) {
        info.radioInterface = Umts;
        info.rssi = umts_rssi;
        info.quality = umts_quality;
        info_vector.push_back (info);
    }

    /* LTE */
    if (lte_available) {
        info.radioInterface = Lte;
        info.rssi = lte_rssi;
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
        throw_response_error (status);
    }

    operatorDescription = operator_description;
    operatorMcc = (uint16_t)operator_mcc;
    operatorMnc = (uint16_t)operator_mnc;
    lac = (uint16_t)_lac;
    cid = _cid;

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
        throw_response_error (status);

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
        throw_response_error (status);

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
    ret = send_and_receive (request, 200, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_connect_response_parse (response, &status);

    if (status != RMF_RESPONSE_STATUS_OK) {
        const char *error_str;

        rmf_message_error_response_parse (response, NULL, &error_str);
        free (response);
        throw_verbose_response_error (status, error_str);
    }

    free (response);
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
    ret = send_and_receive (request, 120, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_disconnect_response_parse (response, &status);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw_response_error (status);
}

/*****************************************************************************/

std::string
Modem::GetDataPort (void)
{
    uint8_t *request;
    uint8_t *response;
    const char *str;
    uint32_t status;
    int ret;
    string result;

    request = rmf_message_get_data_port_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_data_port_response_parse (response, &status, &str);
    if (status != RMF_RESPONSE_STATUS_OK) {
        free (response);
        throw_response_error (status);
    }

    result = str;
    free (response);

    return result;
}

/*****************************************************************************/

bool
Modem::IsModemAvailable (void)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    uint8_t available;
    int ret;

    request = rmf_message_is_modem_available_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_is_modem_available_response_parse (response, &status, &available);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw_response_error (status);

    return (bool)available;
}

/*****************************************************************************/

uint32_t
Modem::GetRegistrationTimeout (void)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    uint32_t timeout;
    int ret;

    request = rmf_message_get_registration_timeout_request_new ();
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_get_registration_timeout_response_parse (response, &status, &timeout);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw_response_error (status);

    return timeout;
}

/*****************************************************************************/

void
Modem::SetRegistrationTimeout (uint32_t timeout)
{
    uint8_t *request;
    uint8_t *response;
    uint32_t status;
    int ret;

    request = rmf_message_set_registration_timeout_request_new (timeout);
    ret = send_and_receive (request, 10, &response);
    free (request);

    if (ret != ERROR_NONE)
        throw std::runtime_error (error_strings[ret]);

    rmf_message_set_registration_timeout_response_parse (response, &status);
    free (response);

    if (status != RMF_RESPONSE_STATUS_OK)
        throw_response_error (status);
}

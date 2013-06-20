// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 *  librmf
 *
 *  Copyright (C) 2013 Zodiac Inflight Innovations
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

    return str;
}

/*****************************************************************************/

string
Modem::GetModel (void)
{

}

/*****************************************************************************/

string
Modem::GetSoftwareRevision (void)
{

}

/*****************************************************************************/

string
Modem::GetHardwareRevision (void)
{

}

/*****************************************************************************/

string
Modem::GetImei (void)
{

}

/*****************************************************************************/

string
Modem::GetImsi (void)
{

}

/*****************************************************************************/

string
Modem::GetIccid (void)
{

}

/*****************************************************************************/

void
Modem::Unlock (const string pin)
{
}

/*****************************************************************************/

PowerStatus
Modem::GetPowerStatus (void)
{

}

/*****************************************************************************/

void
Modem::SetPowerStatus (PowerStatus status)
{
}

/*****************************************************************************/

vector<RadioPowerInfo>
Modem::GetPowerInfo (void)
{

}

/*****************************************************************************/

vector<RadioSignalInfo>
Modem::GetSignalInfo (void)
{
}

/*****************************************************************************/

RegistrationStatus
Modem::GetRegistrationStatus (string   &operatorDescription,
                              uint16_t &operatorMcc,
                              uint16_t &operatorMnc,
                              uint16_t &lac,
                              uint32_t &cid)
{
}

/*****************************************************************************/

ConnectionStatus
Modem::GetConnectionStatus (void)
{
    return Disconnected;
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
    return false;
}


/*****************************************************************************/

void
Modem::Connect (const string apn,
                const string user,
                const string password)
{
}

/*****************************************************************************/

void
Modem::Disconnect (void)
{
}

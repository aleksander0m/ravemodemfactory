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
 *
 * Author: Aleksander Morgado <aleksander@lanedo.com>
 */

#ifndef _RMF_OPERATIONS_H_
#define _RMF_OPERATIONS_H_

#include <stdint.h>
#include <vector>
#include <string>

#include "rmf-types.h"

/**
 * Modem:
 *
 * Rave Modem Factory namespace
 */
namespace Modem {

    /**
     * GetManufacturer:
     *
     * Get the modem manufacturer string.
     *
     * Returns: a string.
     */
    std::string GetManufacturer (void);

    /**
     * GetModel:
     *
     * Get the modem model string.
     *
     * Returns: a string.
     */
    std::string GetModel (void);

    /**
     * GetSoftwareRevision:
     *
     * Get the modem software revision string.
     *
     * Returns: a string.
     */
    std::string GetSoftwareRevision (void);

    /**
     * GetHardwareRevision:
     *
     * Get the modem hardware revision string.
     *
     * Returns: a string.
     */
    std::string GetHardwareRevision (void);

    /**
     * GetImei:
     *
     * Get the modem IMEI string.
     *
     * Returns: a string.
     */
    std::string GetImei (void);

    /**
     * GetImsi:
     *
     * Get the SIM IMSI string.
     *
     * Returns: a string.
     */
    std::string GetImsi (void);

    /**
     * GetIccid:
     *
     * Get the SIM ICCID string.
     *
     * Returns: a string.
     */
    std::string GetIccid (void);

    /**
     * IsLocked:
     *
     * Gets whether the modem is locked.
     */
    bool IsLocked (void);

    /**
     * Unlock:
     * @pin: (in) PIN to send.
     *
     * Unlocks the modem, if needed.
     */
    void Unlock (const std::string pin);

    /**
     * EnablePin:
     * @enable: %true to enable PIN request, %false to disable it.
     * @pin: (in) current PIN.
     *
     * Enables or disables PIN request.
     */
    void EnablePin (bool              enable,
                    const std::string pin);

    /**
     * ChangePin:
     * @pin: (in) current PIN.
     * @new_pin: (in) new PIN.
     *
     * Changes the PIN.
     */
    void ChangePin (const std::string pin,
                    const std::string new_pin);

    /**
     * GetPowerStatus:
     *
     * Get radio power status.
     *
     * Returns: a #PowerStatus value.
     */
    PowerStatus GetPowerStatus (void);

    /**
     * SetPowerStatus:
     * @power_status: (in) radio power status.
     *
     * Set radio power status.
     */
    void SetPowerStatus (PowerStatus power_status);

    /**
     * GetPowerInfo:
     *
     * Get the radio power information.
     *
     * This information is given separately for each available radio access
     * technology (e.g. GSM, UMTS or LTE).
     *
     * Returns: a vector of #RadioPowerInfo structs.
     */
    std::vector<RadioPowerInfo> GetPowerInfo (void);

    /**
     * GetSignalInfo:
     * @signalInfo: (out)
     *
     * Get the signal quality information.
     *
     * This information is given separately for each available radio access
     * technology (e.g. GSM, UMTS or LTE).
     *
     * Returns: a vector of #RadioSignalInfo structs.
     */
    std::vector<RadioSignalInfo> GetSignalInfo (void);

    /**
     * GetRegistrationStatus:
     * @operatorDescription: (out) description string of the operator, or empty
     *                        string if unknown.
     * @operatorMcc: (out) Mobile Country Code of the operator, or 0 if unknown.
     * @operatorMnc: (out) Mobile Network Code of the operator, or 0 if unknown.
     * @lac: (out) Location Area Code, or 0 if unknown.
     * @cid: (out) Cell ID, or 0 if unknown.
     *
     * Get the network registration (serving system) info.
     *
     * Returns: the status of the registration.
     */
    RegistrationStatus GetRegistrationStatus (std::string   &operatorDescription,
                                              uint16_t      &operatorMcc,
                                              uint16_t      &operatorMnc,
                                              uint16_t      &lac,
                                              uint32_t      &cid);

    /**
     * GetConnectionStatus:
     *
     * Get connection status.
     *
     * Returns: the status of the connection.
     */
    ConnectionStatus GetConnectionStatus (void);

    /**
     * GetConnectionStats:
     * @txPacketsOk: (out) amount of packets transmitted without error.
     * @rxPacketsOk: (out) amount of packets received without error.
     * @txPacketsError: (out) amount of outgoing packets with framing errors.
     * @rxPacketsError: (out) amount of incoming packets with framing errors.
     * @txPacketsOverflow: (out) amount of packes dropped because transmitter
     *                       buffer overflowed.
     * @rxPacketsOverflow: (out) amount of packes dropped because receiver
     *                       buffer overflowed.
     * @txBytesOk: (out) amount of bytes transmitted without error.
     * @rxBytesOk: (out) amount of bytes received without error.
     *
     * Get connection stats.
     *
     * Returns: %true if the connection stats are valid, %false otherwise.
     */
    bool GetConnectionStats (uint32_t &txPacketsOk,
                             uint32_t &rxPacketsOk,
                             uint32_t &txPacketsError,
                             uint32_t &rxPacketsError,
                             uint32_t &txPacketsOverflow,
                             uint32_t &rxPacketsOverflow,
                             uint64_t &txBytesOk,
                             uint64_t &rxBytesOk);

    /**
     * Connect:
     * @apn: (in) Access Point Name.
     * @user: (in) username to use when authenticating in the access point, if needed.
     * @password: (in) password to use when authenticating in the access point, if needed.
     *
     * Request connection to the network using IPv4.
     */
    void Connect (const std::string apn,
                  const std::string user,
                  const std::string password);

    /**
     * Disconnect:
     *
     * Request disconnection from the network.
     */
    void Disconnect (void);

    /**
     * IsAvailable:
     *
     * Gets whether a modem is available.
     */
    uint8_t IsAvailable (void);
}

#endif /* _RMF_OPERATIONS_H_ */

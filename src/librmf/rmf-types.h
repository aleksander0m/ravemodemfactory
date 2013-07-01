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

#ifndef _RMF_TYPES_H_
#define _RMF_TYPES_H_

#include <stdint.h>

/**
 * Modem:
 *
 * Rave Modem Factory namespace
 */
namespace Modem {

    /**
     * RegistrationStatus:
     * @Idle: Modem is not registered to any network, and not looking for one.
     * @Searching: Modem is searching networks.
     * @Home: Modem is registered in the home network.
     * @Roaming: Modem is registered in a roaming network.
     *
     * Registration status of the modem.
     */
    enum RegistrationStatus {
        Idle,
        Searching,
        Home,
        Roaming
    };

    /**
     * ConnectionStatus:
     * @Disconnected: Modem is disconnected.
     * @Disconnecting: Modem is transitioning from @Connected to @Disconnected.
     * @Connecting: Modem is transitioning from @Disconnected to @Connected.
     * @Connected: Modem is connected.
     *
     * Connection status of the modem.
     */
    enum ConnectionStatus {
        Disconnected,
        Disconnecting,
        Connecting,
        Connected
    };

    /**
     * PowerStatus:
     * @Full: Full power.
     * @Low: Low power status (radio off).
     *
     * Power status of the modem.
     */
    enum PowerStatus {
        Full,
        Low
    };

    /**
     * RadioInterface:
     * @Gsm: GSM/GPRS/EDGE (2G) interface.
     * @Umts: UMTS/HSPA (3G) interface
     * @Lte: LTE (4G) interface.
     *
     * Radio interface of the modem.
     */
    enum RadioInterface {
        Gsm,
        Umts,
        Lte
    };

    /**
     * RadioPowerInfo:
     * @radioInterface: Radio interface to which this value applies.
     * @inTraffic: whether the device is in traffic.
     * @txPower: transmission power, in dBm, only if @inTraffic is %true.
     * @rx0RadioTuned: whether the receiver in channel 0 is tuned to a channel.
     * @rx0Power: reception power in the channel 0, in dBm, only if
     *            @rx0RadioTuned is %true.
     * @rx1RadioTuned: whether the receiver in channel 1 is tuned to a channel.
     * @rx1Power: reception power in the channel 1, in dBm, only if
     *            @rx1RadioTuned is %true.
     *
     * Radio power information.
     */
    struct RadioPowerInfo {
        RadioInterface radioInterface;
        bool           inTraffic;
        double         txPower;
        bool           rx0RadioTuned;
        double         rx0Power;
        bool           rx1RadioTuned;
        double         rx1Power;
    };

    /**
     * RadioSignalInfo:
     * @radioInterface: Radio interface to which this value applies.
     * @rssi: RSSI, in dBm (-125 dBm or lower indicates no signal)
     * @quality: quality in percentage [0,100].
     *
     * Radio signal information.
     */
    struct RadioSignalInfo {
        RadioInterface radioInterface;
        int32_t        rssi;
        uint32_t       quality;
    };
}

#endif /* _RMF_TYPES_H_ */

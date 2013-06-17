// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 *  librmf
 *
 *  Copyright (C) 2013 Zodiac Inflight Innovations
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
        bool           rx0RadioRuned;
        double         rx0Power;
        bool           rx1RadioTuned;
        double         rx1Power;
    };

    /**
     * RadioSignalInfo:
     * @radioInterface: Radio interface to which this value applies.
     * @rssi: RSSI, in dBm (-125.0 dBm or lower indicates no signal)
     * @quality: quality in percentage [0,100].
     *
     * Radio signal information.
     */
    struct RadioSignalInfo {
        RadioInterface radioInterface;
        double         rssi;
        uint8_t        quality;
    };
}

#endif /* _RMF_TYPES_H_ */

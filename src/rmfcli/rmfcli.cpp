// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * rmfcli
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2013 Zodiac Inflight Innovations
 */

#include "config.h"

#include <iostream>
#include <sstream>
#include <getopt.h>

#include <rmf-types.h>
#include <rmf-operations.h>

#define PROGRAM_NAME    "rmfcli"
#define PROGRAM_VERSION PACKAGE_VERSION

static void
printHelp (void)
{
    std::cout << std::endl;
    std::cout << "Usage: " << PROGRAM_NAME << " <option>" << std::endl;
    std::cout << "Options: " << std::endl;
    std::cout << "\t-h, --help" << std::endl;
    std::cout << "\t-v, --version" << std::endl;
    std::cout << "\t-f, --get-manufacturer" << std::endl;
    std::cout << "\t-d, --get-model" << std::endl;
    std::cout << "\t-j, --get-software-revision" << std::endl;
    std::cout << "\t-k, --get-hardware-revision" << std::endl;
    std::cout << "\t-e, --get-imei" << std::endl;
    std::cout << "\t-i, --get-imsi" << std::endl;
    std::cout << "\t-o, --get-iccid" << std::endl;
    std::cout << "\t-U, --unlock=\"pin\"" << std::endl;
    std::cout << "\t-E, --enable-pin=\"pin\"" << std::endl;
    std::cout << "\t-G, --disable-pin=\"pin\"" << std::endl;
    std::cout << "\t-F, --change-pin=\"pin newpin\"" << std::endl;
    std::cout << "\t-p, --get-power-status" << std::endl;
    std::cout << "\t-P, --set-power-status=\"[Full|Low]\"" << std::endl;
    std::cout << "\t-a, --get-power-info" << std::endl;
    std::cout << "\t-s, --get-signal-info" << std::endl;
    std::cout << "\t-r, --get-registration-status" << std::endl;
    std::cout << "\t-c, --get-connection-status" << std::endl;
    std::cout << "\t-C, --connect=\"apn user password\"" << std::endl;
    std::cout << "\t-D, --disconnect" << std::endl;
    std::cout << std::endl;
}

static void
printVersion (void)
{
    std::cout << std::endl;
    std::cout << PROGRAM_NAME << " " PROGRAM_VERSION << std::endl;
    std::cout << "Copyright (2013) Zodiac Inflight Innovations" << std::endl;
    std::cout << std::endl;
}

static int
getManufacturer (void)
{
    std::string manufacturer;

    try {
        manufacturer = Modem::GetManufacturer ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "Manufacturer: "  << manufacturer << std::endl;
    return 0;
}

static int
getModel (void)
{
    std::string model;

    try {
        model = Modem::GetModel ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "Model: "  << model << std::endl;
    return 0;
}

static int
getSoftwareRevision (void)
{
    std::string softwareRevision;

    try {
        softwareRevision = Modem::GetSoftwareRevision ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "Software Revision: "  << softwareRevision << std::endl;
    return 0;
}

static int
getHardwareRevision (void)
{
    std::string hardwareRevision;

    try {
        hardwareRevision = Modem::GetHardwareRevision ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "Hardware Revision: "  << hardwareRevision << std::endl;
    return 0;
}

static int
getImei (void)
{
    std::string imei;

    try {
        imei = Modem::GetImei ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "IMEI: "  << imei << std::endl;
    return 0;
}

static int
getImsi (void)
{
    std::string imsi;

    try {
        imsi = Modem::GetImsi ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "IMSI: "  << imsi << std::endl;
    return 0;
}

static int
getIccid (void)
{
    std::string iccid;

    try {
        iccid = Modem::GetIccid ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "ICCID: "  << iccid << std::endl;
    return 0;
}

static int
unlock (const std::string str)
{
    try {
        Modem::Unlock (str);
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "PIN successfully unlocked" << std::endl;
    return 0;
}

static int
enablePin (const std::string str)
{
    try {
        Modem::EnablePin (true, str);
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "PIN successfully enabled" << std::endl;
    return 0;
}

static int
disablePin (const std::string str)
{
    try {
        Modem::EnablePin (false, str);
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "PIN successfully disabled" << std::endl;
    return 0;
}

static int
changePin (const std::string str)
{
    std::istringstream iss (str);
    std::string pin;
    std::string newPin;

    iss >> pin;
    if (!iss) {
        std::cout << "New pin missing" << std::endl;
        return -1;
    }
    iss >> newPin;
    if (iss) {
        std::string rest;

        iss >> rest;
        if (rest != "") {
            std::cout << "Too many arguments given" << std::endl;
            return -1;
        }
    }

    try {
        Modem::ChangePin (pin, newPin);
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "PIN successfully changed" << std::endl;
    return 0;
}

static int
getPowerStatus (void)
{
    Modem::PowerStatus powerStatus;

    try {
        powerStatus = Modem::GetPowerStatus ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    switch (powerStatus) {
    case Modem::Full:
        std::cout << "Power status: Full" << std::endl;
        break;
    case Modem::Low:
        std::cout << "Power status: Low" << std::endl;
        break;
    default:
        std::cout << "Power status: Unknown" << std::endl;
        break;
    }

    return 0;
}

static int
setPowerStatus (const std::string str)
{
    Modem::PowerStatus powerStatus;

    if (str.compare ("Full") == 0 || str.compare ("full") == 0)
        powerStatus = Modem::Full;
    else if (str.compare ("Low") == 0 || str.compare ("low") == 0)
        powerStatus = Modem::Low;
    else {
        std::cout << "Unknown power status given: " << str << std::endl;
        return -1;
    }

    try {
        Modem::SetPowerStatus (powerStatus);
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "Power status successfully changed" << std::endl;
    return 0;
}

static int
getPowerInfo (void)
{
    std::vector<Modem::RadioPowerInfo> infoVector;

    try {
        infoVector = Modem::GetPowerInfo ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    for (std::vector<Modem::RadioPowerInfo>::iterator it = infoVector.begin(); it != infoVector.end(); ++it) {
        switch (it->radioInterface) {
        case Modem::Gsm:
            std::cout << "GSM:" << std::endl;
            break;
        case Modem::Umts:
            std::cout << "UMTS:" << std::endl;
            break;
        case Modem::Lte:
            std::cout << "LTE:" << std::endl;
            break;
        default:
            std::cout << "Unknown:" << std::endl;
            break;
        }

        std::cout << "\tIn traffic: " << (it->inTraffic ? "yes" : "no") << std::endl;
        if (it->inTraffic)
            std::cout << "\tTX power: " << it->txPower << " dBm" << std::endl;
        std::cout << "\tRX 0 tuned: " << (it->rx0RadioTuned ? "yes" : "no") << std::endl;
        if (it->rx0RadioTuned)
            std::cout << "\tRX 0 power: " << it->rx0Power << " dBm" << std::endl;
        std::cout << "\tRX 1 tuned: " << (it->rx1RadioTuned ? "yes" : "no") << std::endl;
        if (it->rx1RadioTuned)
            std::cout << "\tRX 1 power: " << it->rx1Power << " dBm" << std::endl;
    }

    return 0;
}

static int
getSignalInfo (void)
{
    std::vector<Modem::RadioSignalInfo> infoVector;

    try {
        infoVector = Modem::GetSignalInfo ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    for (std::vector<Modem::RadioSignalInfo>::iterator it = infoVector.begin(); it != infoVector.end(); ++it) {
        switch (it->radioInterface) {
        case Modem::Gsm:
            std::cout << "GSM:" << std::endl;
            break;
        case Modem::Umts:
            std::cout << "UMTS:" << std::endl;
            break;
        case Modem::Lte:
            std::cout << "LTE:" << std::endl;
            break;
        default:
            std::cout << "Unknown:" << std::endl;
            break;
        }

        std::cout << "\tRSSI: " << it->rssi << " dBm" << std::endl;
        std::cout << "\tQuality: " << it->rssi << "%" << std::endl;
    }

    return 0;
}

static int
getRegistrationStatus (void)
{
    Modem::RegistrationStatus registrationStatus;
    std::string operatorDescription;
    uint16_t operatorMcc;
    uint16_t operatorMnc;
    uint16_t lac;
    uint32_t cid;

    try {
        registrationStatus = Modem::GetRegistrationStatus (operatorDescription,
                                                           operatorMcc,
                                                           operatorMnc,
                                                           lac,
                                                           cid);
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    switch (registrationStatus) {
    case Modem::Idle:
        std::cout << "Registration status: Idle" << std::endl;
        break;
    case Modem::Searching:
        std::cout << "Registration status: Searching" << std::endl;
        break;
    case Modem::Home:
        std::cout << "Registration status: Home" << std::endl;
        break;
    case Modem::Roaming:
        std::cout << "Registration status: Roaming" << std::endl;
        break;
    default:
        std::cout << "Registration status: Unknown" << std::endl;
        break;
    }

    if (registrationStatus == Modem::Home || registrationStatus == Modem::Roaming) {
        std::cout << "MCCMNC: " << operatorMcc << operatorMnc << std::endl;
        std::cout << "LAC: " << lac << std::endl;
        std::cout << "cid: " << cid << std::endl;
    }

    return 0;
}

static int
getConnectionStatus (void)
{
    Modem::ConnectionStatus connectionStatus;

    try {
        connectionStatus = Modem::GetConnectionStatus ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    switch (connectionStatus) {
    case Modem::Disconnected:
        std::cout << "Connection status: Disconnected" << std::endl;
        break;
    case Modem::Disconnecting:
        std::cout << "Connection status: Disconnecting" << std::endl;
        break;
    case Modem::Connecting:
        std::cout << "Connection status: Connecting" << std::endl;
        break;
    case Modem::Connected:
        std::cout << "Connection status: Connected" << std::endl;
        break;
    default:
        std::cout << "Connection status: Unknown" << std::endl;
        break;
    }

    return 0;
}

static int
getConnectionStats (void)
{
    uint32_t txPacketsOk;
    uint32_t rxPacketsOk;
    uint32_t txPacketsError;
    uint32_t rxPacketsError;
    uint32_t txPacketsOverflow;
    uint32_t rxPacketsOverflow;
    uint64_t txBytesOk;
    uint64_t rxBytesOk;

    try {
        Modem::GetConnectionStats (txPacketsOk,
                                   rxPacketsOk,
                                   txPacketsError,
                                   rxPacketsError,
                                   txPacketsOverflow,
                                   rxPacketsOverflow,
                                   txBytesOk,
                                   rxBytesOk);
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "Connection stats:" << std::endl;
    if (txPacketsOk != 0xFFFFFFFF)
        std::cout << "\tTX Packets Ok: " << txPacketsOk << std::endl;
    if (rxPacketsOk != 0xFFFFFFFF)
        std::cout << "\tRX Packets Ok: " << rxPacketsOk << std::endl;
    if (txPacketsError != 0xFFFFFFFF)
        std::cout << "\tTX Packets Error: " << txPacketsError << std::endl;
    if (rxPacketsError != 0xFFFFFFFF)
        std::cout << "\tRX Packets Error: " << rxPacketsError << std::endl;
    if (txPacketsOverflow != 0xFFFFFFFF)
        std::cout << "\tTX Packets Overflow: " << txPacketsOverflow << std::endl;
    if (rxPacketsOverflow != 0xFFFFFFFF)
        std::cout << "\tRX Packets Overflow: " << rxPacketsOverflow << std::endl;
    std::cout << "\tTX Bytes Ok: " << txBytesOk << std::endl;
    std::cout << "\tRX Bytes Ok: " << rxBytesOk << std::endl;

    return 0;
}

static int
connect (const std::string str)
{
    std::istringstream iss (str);
    std::string apn;
    std::string user = "";
    std::string password = "";

    iss >> apn;
    if (iss) {
        iss >> user;
        if (iss) {
            iss >> password;
            if (iss) {
                std::string rest;

                iss >> rest;
                if (rest != "") {
                    std::cout << "Too many arguments given" << std::endl;
                    return -1;
                }
            }
        }
    }

    try {
        Modem::Connect (apn, user, password);
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "PIN successfully connected" << std::endl;
    return 0;
}

static int
disconnect (void)
{
    try {
        Modem::Disconnect ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "PIN successfully disconnected" << std::endl;
    return 0;
}

int
main (int argc, char **argv)
{
    int index;
    int iarg = 0;

    const struct option longopts[] = {
        { "version",                 no_argument, 0, 'v' },
        { "help",                    no_argument, 0, 'h' },
        { "get-manufacturer",        no_argument, 0, 'f' },
        { "get-model",               no_argument, 0, 'd' },
        { "get-software-revision",   no_argument, 0, 'j' },
        { "get-hardware-revision",   no_argument, 0, 'k' },
        { "get-imei",                no_argument, 0, 'e' },
        { "get-imsi",                no_argument, 0, 'i' },
        { "get-iccid",               no_argument, 0, 'o' },
        { "unlock",                  required_argument, 0, 'U' },
        { "enable-pin",              required_argument, 0, 'E' },
        { "disable-pin",             required_argument, 0, 'G' },
        { "change-pin",              required_argument, 0, 'F' },
        { "get-power-status",        no_argument,       0, 'p' },
        { "set-power-status",        required_argument, 0, 'P' },
        { "get-power-info",          no_argument,       0, 'a' },
        { "get-signal-info",         no_argument,       0, 's' },
        { "get-registration-status", no_argument,       0, 'r' },
        { "get-connection-status",   no_argument,       0, 'c' },
        { "get-connection-stats",    no_argument,       0, 'x' },
        { "connect",                 required_argument, 0, 'C' },
        { "disconnect",              no_argument      , 0, 'D' },
        { 0,                         0,                 0, 0   },
    };

    if (argc != 2) {
        printHelp ();
        return -1;
    }

    // turn off getopt error message
    opterr = 1;

    while (iarg != -1) {
        iarg = getopt_long (argc, argv, "vhfdjkeioU:E:G:C:pP:asrcxC:D", longopts, &index);

        switch (iarg) {
        case 'h':
            printHelp ();
            return 0;
        case 'v':
            printVersion ();
            return 0;
        case 'f':
            return getManufacturer ();
        case 'd':
            return getModel ();
        case 'j':
            return getSoftwareRevision ();
        case 'k':
            return getHardwareRevision ();
        case 'e':
            return getImei ();
        case 'i':
            return getImsi ();
        case 'o':
            return getIccid ();
        case 'U':
            return unlock (optarg);
        case 'E':
            return enablePin (optarg);
        case 'G':
            return disablePin (optarg);
        case 'F':
            return changePin (optarg);
        case 'p':
            return getPowerStatus ();
        case 'P':
            return setPowerStatus (optarg);
        case 'a':
            return getPowerInfo ();
        case 's':
            return getSignalInfo ();
        case 'r':
            return getRegistrationStatus ();
        case 'c':
            return getConnectionStatus ();
        case 'x':
            return getConnectionStats ();
        case 'C':
            return connect (optarg);
        case 'D':
            return disconnect ();
        }
    }

    return 0;
}

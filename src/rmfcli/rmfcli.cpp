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
 * Copyright (C) 2013-2015 Zodiac Inflight Innovations
 *
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#include "config.h"

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <getopt.h>
#include <assert.h>
#include <string.h>

#include <rmf-types.h>
#include <rmf-operations.h>

#define PROGRAM_NAME    "rmfcli"
#define PROGRAM_VERSION PACKAGE_VERSION

static void
printHelp (void)
{
    std::cout << std::endl;
    std::cout << "Usage: " << PROGRAM_NAME << " <action>" << std::endl;
    std::cout << "Actions:" << std::endl;
    std::cout << "\t-f, --get-manufacturer" << std::endl;
    std::cout << "\t-d, --get-model" << std::endl;
    std::cout << "\t-j, --get-software-revision" << std::endl;
    std::cout << "\t-k, --get-hardware-revision" << std::endl;
    std::cout << "\t-e, --get-imei" << std::endl;
    std::cout << "\t-i, --get-imsi" << std::endl;
    std::cout << "\t-o, --get-iccid" << std::endl;
    std::cout << "\t-z, --get-sim-info" << std::endl;
    std::cout << "\t-L, --is-locked" << std::endl;
    std::cout << "\t-U, --unlock=\"pin\"" << std::endl;
    std::cout << "\t-E, --enable-pin=\"pin\"" << std::endl;
    std::cout << "\t-G, --disable-pin=\"pin\"" << std::endl;
    std::cout << "\t-F, --change-pin=\"pin newpin\"" << std::endl;
    std::cout << "\t-p, --get-power-status" << std::endl;
    std::cout << "\t-P, --set-power-status=\"[Full|Low]\"" << std::endl;
    std::cout << "\t-Z, --power-cycle" << std::endl;
    std::cout << "\t-a, --get-power-info" << std::endl;
    std::cout << "\t-s, --get-signal-info" << std::endl;
    std::cout << "\t-r, --get-registration-status" << std::endl;
    std::cout << "\t-t, --get-registration-timeout" << std::endl;
    std::cout << "\t-T, --set-registration-timeout=\"timeout\"" << std::endl;
    std::cout << "\t-c, --get-connection-status" << std::endl;
    std::cout << "\t-x, --get-connection-stats" << std::endl;
    std::cout << "\t-C, --connect=\"apn user password\"" << std::endl;
    std::cout << "\t-D, --disconnect" << std::endl;
    std::cout << "\t-b, --get-data-port" << std::endl;
    std::cout << "\t-A, --is-available" << std::endl;
    std::cout << std::endl;
    std::cout << "Common actions:" << std::endl;
    std::cout << "\t-h, --help" << std::endl;
    std::cout << "\t-v, --version" << std::endl;
    std::cout << std::endl;
}

static void
printVersion (void)
{
    std::cout << std::endl;
    std::cout << PROGRAM_NAME << " " PROGRAM_VERSION << std::endl;
    std::cout << "Copyright (2013-2015) Zodiac Inflight Innovations" << std::endl;
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
getSimInfo (void)
{
    uint16_t operatorMcc;
    uint16_t operatorMnc;
    std::vector<Modem::PlmnInfo> plmns;
    uint32_t c = 0;

    try {
        Modem::GetSimInfo (operatorMcc, operatorMnc, plmns);
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "MCC: " << operatorMcc << std::endl;
    std::cout << "MNC: " << operatorMnc << std::endl;
    if (plmns.size () == 0) {
        std::cout << "No additional PLMN info available" << std::endl;
        return 0;
    }

    std::cout << "Additional PLMN information:" << std::endl;
    for (std::vector<Modem::PlmnInfo>::iterator it = plmns.begin (); it != plmns.end (); ++it, ++c) {
        std::cout << "[" << c << "]" << std::endl;
        std::cout << "\tMCC:  " << it->mcc << std::endl;
        std::cout << "\tMNC:  " << it->mnc << std::endl;
        std::cout << "\tGSM:  " << (it->gsm  ? "yes" : "no") << std::endl;
        std::cout << "\tUMTS: " << (it->umts ? "yes" : "no") << std::endl;
        std::cout << "\tLTE:  " << (it->lte  ? "yes" : "no") << std::endl;
    }

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
isSimLocked (void)
{
    bool locked;

    try {
        locked = Modem::IsSimLocked ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    if (!locked)
        std::cout << "PIN is unlocked" << std::endl;
    else
        std::cout << "PIN is locked" << std::endl;

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
powerCycle (void)
{
    try {
        Modem::PowerCycle ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "Power cycle successfully requested" << std::endl;
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

    if (infoVector.size () == 0) {
        std::cout << "No power info available" << std::endl;
        return 0;
    }

    for (std::vector<Modem::RadioPowerInfo>::iterator it = infoVector.begin (); it != infoVector.end (); ++it) {
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

    if (infoVector.size() == 0) {
        std::cout << "No signal information available" << std::endl;
        return 0;
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
        std::cout << "\tQuality: " << it->quality << "%" << std::endl;
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
    case Modem::Scanning:
        std::cout << "Registration status: Scanning" << std::endl;
        break;
    default:
        std::cout << "Registration status: Unknown" << std::endl;
        break;
    }

    if (registrationStatus == Modem::Home || registrationStatus == Modem::Roaming) {
        std::cout << "MCC: " << operatorMcc << std::endl;
        std::cout << "MNC: " << operatorMnc << std::endl;
        std::cout << "Operator: " << operatorDescription << std::endl;
        std::cout << "Location Area code: " << lac << std::endl;
        std::cout << "Cell ID: " << cid << std::endl;
    }

    return 0;
}

static int
getRegistrationTimeout (void)
{
    uint32_t timeout;

    try {
        timeout = Modem::GetRegistrationTimeout ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "Registration timeout: " << timeout << std::endl;
    return 0;
}

static int
setRegistrationTimeout (const std::string str)
{
    int32_t timeout;

    timeout = atoi (str.c_str());
    if (timeout <= 0) {
        std::cout << "Invalid timeout value given: " << str << std::endl;
        return -1;
    }

    try {
        Modem::SetRegistrationTimeout ((uint32_t)timeout);
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "Registration timeout correctly updated " << std::endl;
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

    std::cout << "Modem successfully connected" << std::endl;
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

    std::cout << "Modem successfully disconnected" << std::endl;
    return 0;
}

static int
getDataPort (void)
{
    std::string data_port;

    try {
        data_port = Modem::GetDataPort ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "Data port: "  << data_port << std::endl;
    return 0;
}

static int
isAvailable (void)
{
    bool available;

    try {
        available = Modem::IsModemAvailable ();
    } catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return -1;
    }

    if (!available)
        std::cout << "Modem is unavailable" << std::endl;
    else
        std::cout << "Modem is available" << std::endl;

    return 0;
}

//-----------------------------------------------------------------------------

static const struct option longopts[] = {
    { "version",                  no_argument,       0, 'v' },
    { "help",                     no_argument,       0, 'h' },
    { "get-manufacturer",         no_argument,       0, 'f' },
    { "get-model",                no_argument,       0, 'd' },
    { "get-software-revision",    no_argument,       0, 'j' },
    { "get-hardware-revision",    no_argument,       0, 'k' },
    { "get-imei",                 no_argument,       0, 'e' },
    { "get-imsi",                 no_argument,       0, 'i' },
    { "get-iccid",                no_argument,       0, 'o' },
    { "get-sim-info",             no_argument,       0, 'z' },
    { "is-locked",                no_argument,       0, 'L' },
    { "unlock",                   required_argument, 0, 'U' },
    { "enable-pin",               required_argument, 0, 'E' },
    { "disable-pin",              required_argument, 0, 'G' },
    { "change-pin",               required_argument, 0, 'F' },
    { "get-power-status",         no_argument,       0, 'p' },
    { "set-power-status",         required_argument, 0, 'P' },
    { "power-cycle",              no_argument,       0, 'Z' },
    { "get-power-info",           no_argument,       0, 'a' },
    { "get-signal-info",          no_argument,       0, 's' },
    { "get-registration-status",  no_argument,       0, 'r' },
    { "get-registration-timeout", no_argument,       0, 't' },
    { "set-registration-timeout", required_argument, 0, 'T' },
    { "get-connection-status",    no_argument,       0, 'c' },
    { "get-connection-stats",     no_argument,       0, 'x' },
    { "connect",                  required_argument, 0, 'C' },
    { "disconnect",               no_argument,       0, 'D' },
    { "get-data-port",            no_argument,       0, 'b' },
    { "is-available",             no_argument,       0, 'A' },
    { 0,                          0,                 0, 0   },
};

static const char *
get_iarg_long_opt (int iarg)
{
    int i;

    for (i = 0; longopts[i].val != 0; i++) {
        if (longopts[i].val == iarg)
            return longopts[i].name;
    }
    assert (0);
    return NULL;
}

static void
enable_arg_int (unsigned int & arg,
                int iarg)
{
    if (arg) {
        std::cerr << "error: " << get_iarg_long_opt (iarg) << " option specified multiple times" << std::endl;
        exit (-1);
    }
    arg = 1;
}

static void
enable_arg_str (char * & arg,
                const char *value,
                int iarg)
{
    if (arg) {
        std::cerr << "error: " << get_iarg_long_opt (iarg) << " option specified multiple times" << std::endl;
        exit (-1);
    }

    /* Note: we use C strings because we want to use "!!" to convert the string to a boolean */
    arg = strdup (value);
}

int
main (int argc, char **argv)
{
    int i;
    int iarg = 0;
    unsigned int action_get_manufacturer = 0;
    unsigned int action_get_model = 0;
    unsigned int action_get_software_revision = 0;
    unsigned int action_get_hardware_revision = 0;
    unsigned int action_get_imei = 0;
    unsigned int action_get_imsi = 0;
    unsigned int action_get_iccid = 0;
    unsigned int action_get_sim_info = 0;
    unsigned int action_is_sim_locked = 0;
    char *action_unlock = NULL;
    char *action_enable_pin = NULL;
    char *action_disable_pin = NULL;
    char *action_change_pin = NULL;
    unsigned int action_get_power_status = 0;
    char *action_set_power_status = NULL;
    unsigned int action_power_cycle = 0;
    unsigned int action_get_power_info = 0;
    unsigned int action_get_signal_info = 0;
    unsigned int action_get_registration_status = 0;
    unsigned int action_get_registration_timeout = 0;
    char *action_set_registration_timeout = NULL;
    unsigned int action_get_connection_status = 0;
    unsigned int action_get_connection_stats = 0;
    char *action_connect = NULL;
    unsigned int action_disconnect = 0;
    unsigned int action_get_data_port = 0;
    unsigned int action_is_available = 0;
    unsigned int n_actions;
    int result;

    // turn off getopt error message
    opterr = 1;

    while (iarg != -1) {
        iarg = getopt_long (argc, argv, "vhfdjkeiozLU:E:G:C:pP:ZasrtT:cxC:DbA", longopts, &i);

        switch (iarg) {
        case 'h':
            printHelp ();
            return 0;
        case 'v':
            printVersion ();
            return 0;
        case 'f':
            enable_arg_int (action_get_manufacturer, iarg);
            break;
        case 'd':
            enable_arg_int (action_get_model, iarg);
            break;
        case 'j':
            enable_arg_int (action_get_software_revision, iarg);
            break;
        case 'k':
            enable_arg_int (action_get_hardware_revision, iarg);
            break;
        case 'e':
            enable_arg_int (action_get_imei, iarg);
            break;
        case 'i':
            enable_arg_int (action_get_imsi, iarg);
            break;
        case 'o':
            enable_arg_int (action_get_iccid, iarg);
            break;
        case 'z':
            enable_arg_int (action_get_sim_info, iarg);
            break;
        case 'L':
            enable_arg_int (action_is_sim_locked, iarg);
            break;
        case 'U':
            enable_arg_str (action_unlock, optarg, iarg);
            break;
        case 'E':
            enable_arg_str (action_enable_pin, optarg, iarg);
            break;
        case 'G':
            enable_arg_str (action_disable_pin, optarg, iarg);
            break;
        case 'F':
            enable_arg_str (action_change_pin, optarg, iarg);
            break;
        case 'p':
            enable_arg_int (action_get_power_status, iarg);
            break;
        case 'P':
            enable_arg_str (action_set_power_status, optarg, iarg);
            break;
        case 'Z':
            enable_arg_int (action_power_cycle, iarg);
            break;
        case 'a':
            enable_arg_int (action_get_power_info, iarg);
            break;
        case 's':
            enable_arg_int (action_get_signal_info, iarg);
            break;
        case 'r':
            enable_arg_int (action_get_registration_status, iarg);
            break;
        case 't':
            enable_arg_int (action_get_registration_timeout, iarg);
            break;
        case 'T':
            enable_arg_str (action_set_registration_timeout, optarg, iarg);
            break;
        case 'c':
            enable_arg_int (action_get_connection_status, iarg);
            break;
        case 'x':
            enable_arg_int (action_get_connection_stats, iarg);
            break;
        case 'C':
            enable_arg_str (action_connect, optarg, iarg);
            break;
        case 'D':
            enable_arg_int (action_disconnect, iarg);
            break;
        case 'b':
            enable_arg_int (action_get_data_port, iarg);
            break;
        case 'A':
            enable_arg_int (action_is_available, iarg);
            break;
        }
    }

    n_actions = (
        action_get_manufacturer +
        action_get_model +
        action_get_software_revision +
        action_get_hardware_revision +
        action_get_imei +
        action_get_imsi +
        action_get_iccid +
        action_get_sim_info +
        action_is_sim_locked +
        !!action_unlock +
        !!action_enable_pin +
        !!action_disable_pin +
        !!action_change_pin +
        action_get_power_status +
        !!action_set_power_status +
        action_power_cycle +
        action_get_power_info +
        action_get_signal_info +
        action_get_registration_status +
        action_get_registration_timeout +
        !!action_set_registration_timeout +
        action_get_connection_status +
        action_get_connection_stats +
        !!action_connect +
        action_disconnect +
        action_get_data_port +
        action_is_available);

    if (n_actions == 0) {
        std::cerr << "error: no actions specified" << std::endl;
        return -1;
    }

    if (n_actions > 1) {
        std::cerr << "error: too many actions specified" << std::endl;
        return -1;
    }

    if (action_get_manufacturer)
        result = getManufacturer ();
    else if (action_get_model)
        result = getModel ();
    else if (action_get_software_revision)
        result = getSoftwareRevision ();
    else if (action_get_hardware_revision)
        result = getHardwareRevision ();
    else if (action_get_imei)
        result = getImei ();
    else if (action_get_imsi)
        result = getImsi ();
    else if (action_get_iccid)
        result = getIccid ();
    else if (action_get_sim_info)
        result = getSimInfo ();
    else if (action_is_sim_locked)
        result = isSimLocked ();
    else if (action_unlock)
        result = unlock (action_unlock);
    else if (action_enable_pin)
        result =  enablePin (action_enable_pin);
    else if (action_disable_pin)
        result = disablePin (action_disable_pin);
    else if (action_change_pin)
        result = changePin (action_change_pin);
    else if (action_get_power_status)
        result = getPowerStatus ();
    else if (action_set_power_status)
        result = setPowerStatus (action_set_power_status);
    else if (action_power_cycle)
        result = powerCycle ();
    else if (action_get_power_info)
        result = getPowerInfo ();
    else if (action_get_signal_info)
        result = getSignalInfo ();
    else if (action_get_registration_status)
        result = getRegistrationStatus ();
    else if (action_get_registration_timeout)
        result = getRegistrationTimeout ();
    else if (action_set_registration_timeout)
        result = setRegistrationTimeout (action_set_registration_timeout);
    else if (action_get_connection_status)
        result = getConnectionStatus ();
    else if (action_get_connection_stats)
        result = getConnectionStats ();
    else if (action_connect)
        result = connect (action_connect);
    else if (action_disconnect)
        result = disconnect ();
    else if (action_get_data_port)
        result = getDataPort ();
    else if (action_is_available)
        result = isAvailable ();
    else
        assert (0);

    /* Clean exit, for a clean memleak report */
    free (action_unlock);
    free (action_enable_pin);
    free (action_disable_pin);
    free (action_change_pin);
    free (action_set_power_status);
    free (action_set_registration_timeout);
    free (action_connect);
    return 0;
}

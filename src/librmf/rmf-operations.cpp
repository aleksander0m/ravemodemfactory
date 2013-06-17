// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 *  librmf
 *
 *  Copyright (C) 2013 Zodiac Inflight Innovations
 */

#include "rmf-operations.h"

using namespace std;
using namespace Modem;

/*****************************************************************************/

string
GetManufacturer (void)
{

}

/*****************************************************************************/

string
GetModel (void)
{

}

/*****************************************************************************/

string
GetSoftwareRevision (void)
{

}

/*****************************************************************************/

string
GetHardwareRevision (void)
{

}

/*****************************************************************************/

string
GetImei (void)
{

}

/*****************************************************************************/

string
GetImsi (void)
{

}

/*****************************************************************************/

string
GetIccid (void)
{

}

/*****************************************************************************/

void
Unlock (const string pin)
{
}

/*****************************************************************************/

PowerStatus
GetPowerStatus (void)
{

}

/*****************************************************************************/

void
SetPowerStatus (PowerStatus status)
{
}

/*****************************************************************************/

vector<RadioPowerInfo>
GetPowerInfo (void)
{

}

/*****************************************************************************/

vector<RadioSignalInfo>
GetSignalInfo (void)
{
}

/*****************************************************************************/

RegistrationStatus
GetRegistrationStatus (string   &operatorDescription,
                       uint16_t &operatorMcc,
                       uint16_t &operatorMnc,
                       uint16_t &lac,
                       uint32_t &cid)
{
}

/*****************************************************************************/

ConnectionStatus
GetConnectionStatus (void)
{
    return Disconnected;
}

/*****************************************************************************/

bool
GetConnectionStats (uint32_t &txPacketsOk,
                    uint32_t &rxPacketsOk,
                    uint32_t &txPacketsError,
                    uint32_t &rxPacketsError,
                    uint32_t &txPacketsOverflow,
                    uint32_t &rxPacketsOverflow)
{
    return false;
}


/*****************************************************************************/

void
Connect (const string apn,
         const string user,
         const string password)
{
}

/*****************************************************************************/

void
Disconnect (void)
{
}

// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 *  rmfcli
 *
 *  Copyright (C) 2013 Zodiac Inflight Innovations
 */

#include <iostream>

#include <rmf-operations.h>

using namespace std;
using namespace Modem;


int
main (int argc, char **argv)
{
    string manufacturer;


    manufacturer = GetManufacturer ();

    cout << manufacturer;

    return 1;
}

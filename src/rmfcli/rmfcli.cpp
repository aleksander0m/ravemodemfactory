// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 *  rmfcli
 *
 *  Copyright (C) 2013 Zodiac Inflight Innovations
 */

#include "config.h"

#include <iostream>
#include <getopt.h>

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
    std::cout << "\t-M, --get-manufacturer" << std::endl;
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

int
main (int argc, char **argv)
{
    int index;
    int iarg = 0;

    const struct option longopts[] = {
        {"version",          no_argument, 0, 'v' },
        {"help",             no_argument, 0, 'h' },
        {"get-manufacturer", no_argument, 0, 'M' },
        {0,0,0,0},
    };

    if (argc != 2) {
        printHelp ();
        return -1;
    }

    // turn off getopt error message
    opterr = 1;

    while (iarg != -1)
    {
        iarg = getopt_long (argc, argv, "Mvh", longopts, &index);

        switch (iarg) {
        case 'h':
            printHelp ();
            return 0;

        case 'v':
            printVersion ();
            return 0;

        case 'M':
            return getManufacturer ();
        }
    }

    return 0;
}

// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * Small librmf client example
 *
 * Assuming RMF was installed in /usr, compile with:
 *   $> g++ -o example -I /usr/include/librmf -lrmf example.cpp
 */

#include <iostream>
#include <rmf-operations.h>

int
main (int argc, char **argv)
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

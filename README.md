# RaveModemFactory

-------------------------------------------------------------------------------
What is the RaveModemFactory?
-------------------------------------------------------------------------------

The RaveModemFactory is a simple connection manager for modems which speak the
Qualcomm MSM Interface (QMI) protocol. This connection manager is suitable for
embedded systems with simple mobile broadband requirements.

The current version of RaveModemFactory supports a wide range of devices,
including but not limited to:

  * Sierra Wireless 9200 family (MC7700, MC7710...): These devices will always
    work with the link layer protocol set to 802.3.

  * Sierra Wireless 9x15 family (MC7304, MC7330, MC7354...): On kernels before
    4.5, these devices will work with the link layer protocol set to 802.3. On
    newer kernels (4.5 included) the devices will work with the link layer
    protocol preconfigured in the device (either 802.3 or raw-ip).

  * Sierra Wireless 9x30 family (MC7430, MC7455...): These devices only allow
    the link layer protocol set to raw-ip, so they will only be supported since
    kernel 4.5.


-------------------------------------------------------------------------------
How does it work?
-------------------------------------------------------------------------------

The 'rmfd' daemon will take care of finding the modem's QMI (/dev/cdc-wdm) port
as well as its associated NET/WWAN port. The daemon expects only ONE modem in
the system. If multiple modems are found, it may not work, or it may just choose
the last one found. The daemon will send QMI requests and receive QMI responses,
using the QMI support provided by 'libqmi' [1].

The 'librmf' library provides a C++ interface to run operations in the daemon.
All the actions in the interface are blocking; i.e. the thread running the
action will be halted until a response is received from the 'rmfd' daemon, or
until the specified timeout expires.

The 'rmfcli' command line tool allows to run all the different actions exposed
by the 'librmf' library.

The communication between the 'librmf' library and the 'rmfd' daemon is
performed via local UNIX sockets, using a custom protocol to embed the request
and responses.

When a 3GPP connection is requested, specifying at least the APN, and the
connection succeeds, the 'rmfd' daemon will execute the 'rmfd-wwan-service'
script. This script takes care of bringing up the net interface and configuring
it as required (e.g. with DHCP if using 802.3 link layer protocol and with static
IP addressing if using raw-ip). The script will also take care of setting up the
reported DNS servers and routes.

Upon disconnection, the same script will bring down the net interface.


-------------------------------------------------------------------------------
Operation
-------------------------------------------------------------------------------

Unlocking:

The first thing to do when wanting to get the modem connected is to make sure
that the modem is unlocked. There is no explicit command to query whether the
modem is unlocked, you just need to run Unlock() passing the PIN. The operation
will succeed in all these cases:
 * If the modem was already unlocked.
 * If the modem had the PIN request disabled.
 * If the unlock with the given PIN was successful.

Power on:

Once unlocked, you need to make sure that the modem has the radio powered on,
by running SetPowerStatus(Full). This operation will succeed in all these cases:
 * If the modem was already in Full power mode.
 * If the power mode was changed from Low to Full.

During the power on, a explicit request to register in the network will be
issued. This means that you can use the SetPowerStatus(Full) to re-request
automatic registration in the home network if needed.

Connection:

The Connect() and Disconnect() methods will get the modem connected or
disconnected. The Connect() methods performs the whole connection process,
including the network interface setup.


-------------------------------------------------------------------------------
Dependencies
-------------------------------------------------------------------------------

The 'rmfd' daemon requires:
 * glib & gio >= 2.48
 * libqmi >= 1.26
 * gudev >= 147 (and therefore, the udev daemon)
 * dhclient (only if link layer protocol is 802.3)

The 'librmf' library and the 'rmfcli' utility don't have any external
dependency.


-------------------------------------------------------------------------------
Notes
-------------------------------------------------------------------------------

 [1] Since RaveModemFactory v1.28.0, the SIM management commands are based on
     the complex "UIM" QMI service, instead of the simpler "DMS" QMI service
     specific "DMS UIM" commands, as these were obsoleted in the new Sierra
     Wireless MC74xx devices.


-------------------------------------------------------------------------------
References
-------------------------------------------------------------------------------

[1] http://www.freedesktop.org/wiki/Software/libqmi

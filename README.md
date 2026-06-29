# velserv
A TCP to Velbus gateway for Linux

For use with the http://www.velbus.eu range of digital building control hardware.

This TCP server offers a way to share the USB connection so that various software platforms to connect to the Velbus network simultaneously.
For example, VelbusLink, OpenHab2 www.openhab.org , www.OpenRemote.org , HomeAssistant

# HowTo
Use GCC to compile the C code for your system

sudo apt-get install gcc

Or

sudo apt-get install build-essential

Use this command line to compile for your system

gcc -o velserv velserv.c -lpthread

# Docker on Raspberry Pi 4

The Dockerfile uses the official Alpine Linux image and builds velserv in a
separate builder stage. On a Raspberry Pi 4 running 64-bit Raspberry Pi OS, use
the ARM64 platform when cross-building:

docker buildx build --platform linux/arm64 -t velserv --load .

Run the container with the Velbus USB serial device passed through:

docker run --rm --device /dev/ttyACM0 -p 3788:3788 velserv -f -v -d /dev/ttyACM0 -p 3788

The container healthcheck verifies that `/dev/ttyACM0` is available, velserv is
accepting local TCP connections, and at least one valid Velbus frame is observed
within five seconds. If you override the serial device or port, pass matching
environment variables so the healthcheck follows the same settings:

docker run --rm --device /dev/ttyACM1 -e VELSERV_DEVICE=/dev/ttyACM1 -e VELSERV_PORT=6000 -p 6000:6000 velserv -f -v -d /dev/ttyACM1 -p 6000

This is a passive bus-live check: it proves the bridge is seeing real Velbus
traffic, but a completely idle bus can make the container report unhealthy until
a module transmits a frame.

# TLS probes

velserv is a plain TCP Velbus bridge and does not support TLS. If a client first
sends a TLS ClientHello, velserv returns a fatal TLS handshake failure alert and
closes that probe connection. Clients that support fallback should then open a
new plain TCP connection; TLS cannot be downgraded in-place on the same socket.

Usage: ./velserv -csfvhV] -d DEVICE] -a ADDRESS] -p PORT]

Tip : try to run as root

-s --server act as server only, gateway will be disabled
when in server mode, the address is always 127.0.0.1

-c --client act as client only, server wil be disabled

-d --device INTERFACE device where the Velbus interface is connected to
default device is: /dev/ttyACM0

-a --address HOST IP address or hostname where to connect to as client
default is 127.0.0.1

-p --port port where to connect
default is 3788

-f --foreground do not run in background

-v --verbose verbose operation, repeat for debugging output
1 general debug, 2-3 com to socket debug, 4-6 server socket debug

-h --help print this help and exit

-V --version print version information and exit

An example

To start VelServ and use /dev/ttyACM0 and port 6000, the following command line should be used

sudo ./velserv -d /dev/ttyACM0 -p 6000

If you have issues with Linux assigning different tty**** ports, you can run VelServ and point it directly to the Velleman USB device

./velserv -d /dev/serial/by-id/usb-Velleman_Projects_VMB1USB_Velbus_USB_interface-if00 -p 6000

# VelServ as a Linux Service

When enabled, the velserv.service file starts, stops and restarts VelServ as a service in Linux systems that support SystemD.

The velserv.service file is set to use device /dev/ttyACM0 and port 6000, if you wish a different configuration, please edit the service file, you will find the parameters in a line towards the end that starts ExecStart=/opt/velserv

Move the velserv.service file into /etc/systemd/system/

{You can create a symlink if you wish}

with the compiled velserv application in /opt/velserv/	{Edit the verserv.service file if your ./velserv application is in a different folder}

run the following commands to activate velserv.service

sudo systemctl daemon-reload

sudo systemctl enable velserv.service

reboot to get VelServ to load on boot, or to start the service now :-

sudo systemctl start velserv

you can also use

sudo systemctl restart velserv

sudo systemctl stop velserv

or any user can use

systemctl status velserv

# Link to the Velbus forum where Stuart (MDAR) explains how you can get this to work: 
https://forum.velbus.eu/t/how-to-install-and-run-velserv-a-velbus-tcp-gateway/15422

# RPi Setup

This document details how to set up a Raspberry Pi for the HIL project.

## Flashing the OS

Use the [Raspberry Pi Imager](https://www.raspberrypi.com/software/) software to flash an OS to a micro sd card. We would suggest using Ubuntu server. [This](https://www.raspberrypi.com/documentation/computers/getting-started.html) documentation from Raspberry Pi describes how to use the imager software, but it's pretty intuitive.

Some things to remember:
1. Set a good user/device name
    - Something like nmsuser@nms-server, though don't make every single pi have the same name.
    - Also the password should be simple
2. Enable SSH
3. It's good practice to set it up to automatically connect to your phones hotspot, in case ethernet decides to break.

## Connecting with Tailscale

TODO

## Current Testbenches

| User | Device Name | Sudo Password | Active |
|---|---|---|---|
| nmsuser | nms-hil-server1 | password | Yes
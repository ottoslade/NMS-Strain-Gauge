# Nittany Motorsports Telematics Control Unit (TCU)

Welcome to the Nittany Motorsports Over-The-Air Host Project! This repository contains the codebase for the ESP32, the host microcontroller that drives the remote firmware updates

## Table of Contents

- [Introduction](#introduction)
- [Folder Structure](#folder-structure)
- [Setup and Installation](#setup-and-installation)
- [STM32F446RE Update over CAN Logic](#stm32f446re-update-over-can-Logic)
- [OTA Firmware Update Setup Process](#ota-firmware-update-setup-process)
- [Contributing](#contributing)
- [License](#license)

## Introduction

The **TCU** is a vehicle controller that acts a bridge between the vehicle and the engineers, allowing data collection and Over-The-Air (OTA) firmware updates.

### Features

- **OTA Firmware Updates**: OTA firmware updates to all vehicle ECUs
- **Live Telemetry**: Collect live data from CAN bus and send it to a receiving station using ESP-NOW (see [Telemetry Station README](../telemetry-station/README.md) for more details on the system architecture)

### OTA Update Nodes

1. **ESP32**:
   - Acts as the central controller managing OTA updates over CAN.
   - Runs an HTTP server for file uploads and flashing control.
   
2. **VEHICLE ECUs**:
   - Microcontrollers receiving firmware updates from the ESP32.


## Folder Structure

- `/components/update_server`: HTTP server that allows a client to initialize a firmware update
- `/components/telemetry`: contains ESP-NOW related tasks and function to communicate live data to the telemetry-station
- `/components/stm_flash`: library that contains all the function used to complete the firmware update process
- `/components/can`: contains CAN related tasks and functions
- `components/util`: contains general utility functions/structs/enums

## Setup and Installation

### `esp-idf-v5.5` is not included in this repository

1. Follow the installation instruction in https://github.com/espressif/esp-idf/releases/tag/v5.2.5 to download the zip file

2. Extract `esp-idf-v5.2.5` contents from the downloaded zip file

2. Read the "Setup Build Environment" section in https://github.com/espressif/esp-idf/blob/master/README.md to initialize the ESP_IDF development framework


### `esp-idf` is included in this repository as a submodule:
1. Install python on your local machine (https://www.python.org/downloads/)

   To verify python is installed on your machine run `python --version`

2. Pull the latest version of the [esp-idf](../../shared/esp-idf/) submodule
   ```
   # Navigate to the repo in your terminal
   cd <path>/<to>/nms-electronics

   # Update all submodules
   git submodule update --init --recursive
   ```

2. Run the following commands based on your platform to initialize development framework:
   ```
   # Navigate to the esp-idf folder in the repo
   cd <path>/<to>/nms-electronics/shared/esp-idf
   ```

   On Windows Command Prompt:
   ```
   # Installs ESP-IDF locally (will only need to be ran once)
   ./install.bat

   # Initialize the ESP-IDF environment (run once for every terminal session)
   ./export.bat
   ```

   On Windows Powershell:
   ```
   # Authorize Powershell to run these files (will only need to be ran once)
   Unblock-File ./export.ps1

   # Installs ESP-IDF locally (will only need to be ran once)
   ./install.bat

   # Initialize the ESP-IDF environment (run once for every terminal session)
   ./export.ps1
   ```

   On Unix systems (macOS/Linux) - NOT TESTED YET!:
   ```
   # Installs ESP-IDF locally (will only need to be ran once)
   ./install.sh

   # Initialize the ESP-IDF environment (run once for every terminal session)
   . ./export.sh
   ```
3. Now run `idf.py` commands:
   ```
   # build project
   idf.py build

   # flash project to ESP32 (needs to be connected via USB)
   idf.py flash

   # monitor ESP32 (will display serial output produced by ESP32)
   idf.py monitor
   ```

## STM32F446RE Update over CAN Logic
Bootloader command protocol: https://www.st.com/resource/en/application_note/an3154-how-to-use-can-protocol-in-bootloader-on-stm32-mcus-stmicroelectronics.pdf

1. Reset STM32 by driving the reset pin low while holding the boot pin high to launch bootloader firmware

2. Send "Get Id" command to retrieve the ID of the bootloader and validate it (also used as a metric to validate that the bootloader is running)

3. Send "Erase" command to erase STM32 application

4. Write the new application to the STM32 flash memory using the "Write Memory" command (in blocks of 256)

5. Read back the STM32's FLASH memory through the "Read Memory" command (in blocks of 256) validating that each block matches the corresponding block in the `.bin` file

6. Pull boot pin low and reset STM32 by driving the reset pin low to launch the new application

Failure to complete any of the steps in this process will result in an update failure, after which the ESP32 will send the "Erase" command to erase any imcomplete application that was written to the STM32 flash memory.

## OTA Firmware Update Setup Process

1. Pulled newest files from git.
2. Install ESP-IDF 5.2.5
3. run export.bat in a cmd prompt
4. Hooked up power, gnd, CAN1H, CAN1L on both boards
5. Connected front UART_RX_conn to rear UART_TX_conn for boot0 pin
6. Connected resets the same as previous
7. Set up jumpers for CAN
8. Flash the same bootloader code in bootloaders/stm32f44re... on both boards
9. Change the STM32F44RETx_FLASH.ld file on both boards
   ```
   RAM (xrw) : ORIGIN = 0x20008000, LENGTH = 96K
   FLASH (rx) : ORIGIN = 0x800C000, LENGTH = 464K
   ```

10. Clean Build and flash to the STMs initially
11. Flash the ESP32 chip by hooking up tx and rx to rx and tx respectively on the UART device
12. hold boot and reset to flash with idf.py build flash
13. Connect to ESP32 wifi "NittanyMotorsports" pass: longlivechris
14. Visit 192.168.4.1:8080 for live telemetry
15. Visit 192.168.4.1. for flashing
16. Upload the .bin files in build/debug/-MCU.bin
17. Click Front Flash or Rear Flash


## Contributing

Please follow these guidelines in `nms-electronics/README.md`

## License

- ESP-IDF License: Apache 2.0 (`LICENSES/ESP-IDF_LICENSE.txt`)
- OTA_update_STM32_using_ESP32 License: MIT (`LICENSES/OTA_LIB_LICENSE.txt`)

---

**Note:** This README was written by ChatGPT, thanks ChatGPT.

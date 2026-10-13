# Bootloaders

## Table of Contents

- [Introduction](#introduction)
- [Acronyms Explained](#acronyms-explained)
- [Folder Structure](#folder-structure)
- [Contributing](#contributing)

## Introduction

This folder contains the projects for the firmware of all the controllers included.

## Acronyms Explained
- `ECU`: Electronic Control Unit (https://en.wikipedia.org/wiki/Engine_control_unit)
- `HV`: High Voltage (> 60V)
- `LV`: Low Voltage (< 60V)
- `BMS`: Battery Management System (https://en.wikipedia.org/wiki/Battery_management_system)
- `TCU`: Telematics Control Unit (https://en.wikipedia.org/wiki/Telematic_control_unit, https://www.st.com/en/applications/telematics-and-networking/telematics-and-connectivity-control-unit.html)

## Folder Structure

- `/bootloaders`: This folder contains the firmware bootloaders for all the microcontrollers that are used in the vehicle.
- `/ecu-front`: This folder contains the firmware for the front ECU in the vehicle, an STM32F446RE responsible for sensor data acquisition and driver interface
- `/ecu-rear`: This folder contains the firmware for the rear ECU in the vehicle, an STM32F446RE responsible for sensor data acquisition
- `/hv-bms`: This folder contains the firmware for the BMS of the HV battery
- `/lv-bms`: This folder contains the firmware for the BMS of the LV battery
- `/tcu`: This folder contains the firmware for the vehicle's TCU, an ESP32 responsible for CAN data logging, live telemetry, and Over-The-Air (OTA) firmware updates
- `/telemetry-station`: This folder contains the firmware for the Telemetry Station, an ESP32 that receives live telemetry from the vehicle's TCU and transmits it to a connected laptop over serial

## Contributing

Follow the guidelines highlighed in `nms-electronics/README.md`

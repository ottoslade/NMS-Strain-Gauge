# Nittany Motorsports Electronics - STM Software
Welcome! This is an overview of how to work with the software architecture running on the STM32 microcontroller on our **either** board, to help you in maintaining and developing further functionality.
## Table of Contents
- [CAN Drivers/DBC](#can-drivers-and-dbc-files)
- [I/O Pin Assignment & Referencing](#io-pin-assignment-and-reference)
- [Real-Time Operating System (RTOS)](#real-time-operating-system)
- [Good Coding Practices](#coding-standards)
## CAN Drivers and DBC Files
### CAN/DBC overview
The Controller Area Network (CAN) protocol is an industry-standard communication protocol in the automotive industry (your car definitely runs CAN). We adopted it within our system to carry all relevant information within our car: sensor readings, electric motor status, battery temperatures, etc.

The basics of this network is that each **message**, describing a specific device, is labelled with a unique **CAN ID** that differentiates it from all other messages on the bus. Associated with each message can be a variety of **signals** that each describe a different value associated with that device. For example, our motor controller sends a CAN message onto our bus every 10 ms. or so, with its own particular CAN ID. Attached to that message are a variety of signals that represent values such as motor speed, instantaneous voltage, and torque demanded from the motor.

Every device **receiving** CAN messages needs some way of interpreting the signals it takes in- a key, of sorts. This is what the **DBC** file achieves. You'll find a particular STM's under `your~STM's~repo/Drivers_Custom/dbc_to_c/program/VEHICLE.dbc` and  `your~STM's~repo/Drivers_Custom/dbc_to_c/program/CHARGER.dbc`. As we want messages to be shared between both front and rear processors, the DBCs for each should be identical- they'll each need to read all messages going on the bus. If you're adding a sensor and its accompanying functionality, make sure to add it to the DBC (see KVASER software solutions for editing a DBC).

### DBC-C Generation
Intuitively, we want to be able to "call" the values contained on the CANbus like any other variable in our code. An open-source solution, the DBC-to-C library, allows us to do this. By feeding in our custom DBC, we can output a set of header and main files which we can call within our main firmware to grab values off the CANbus! The process for doing is pretty complicated, and involves a good bit of familiarity with Windows System for Linux (WSL) and some C code compilation. You will only need to go about all of this if you're setting up a CANbus testbench of your own. Instructions will be added here if needed, but until then, feel free to reach out to your PM for help in doing so!
The source code for dbcc is located at `shared-resources/dbcc`, but the output code generated that we reference is found at `lv-electronics/megaboard-front/software/stm-front/Drivers_Custom/dbc_to_c`.

## IO Pin Assignment And Reference
Pin assignment for our STMs is done through **STM32CubeMX**, which you should already have as a software developer on LV. You'll be working with the .ioc file within your specific repo (either `Front-MCU` or `Rear-MCU`). Within CubeMX, you can view and edit each pin's assignment and configuration, including everything from setting input/output (for general-purpose pins), to time scaling (for timer-based chips), and so forth.

**Important Note**: Pinouts are likely different between branches! This means that before editing an .ioc file, check to make sure you're in the correct branch and that your edits aren't going to conflict with other people's existing code in that branch. It's also good practice to compare your pinout to those in our stable branches, `main` and `develop`, to see how far your pinouts deviate. At some point, those pinouts will be referenced by our megaboard developments and traces will be printed, so be careful!

When finished editing your pinout in CubeMX, make sure to 'Generate Code' so that the updated pinout is reflected in the repository and you're calling the correct values in your code.

## Real Time Operating System
The RTOS is the series of constant tasks that run on the STM32 as long as it has power. You can add and configure the RTOS' tasks within CubeMX, and you'll see them reflected as functions for you to provide functionality for within the `freertos.c/freertos.h` files. Various critical commands, such as throttle input, braking status, and launch control, are all contained within this file. 

## Coding Standards
See the "Contributing" guidelines in the overall monorepo's `README.md`. Feel free to reach out to your PM or the LV lead with any questions about best practices!

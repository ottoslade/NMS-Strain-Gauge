# Nittany Motorsports OTA Bootloader

Welcome to the Nittany Motorsports Telemetry App Project!

## Table of Contents

- [Introduction](#introduction)
- [Rationale](#rationale)
- [Folder Structure](#folder-structure)
- [Setup and Installation](#setup-and-installation)
- [Contributing](#contributing)
- [License](#license)

## Introduction

The **Telemetry App** is a small python application that hosts a local WebSocket server and serves a WS client on the browser to display live telemetry data received over UART from the Telemetry Station. See [Telemetry Station README](../../../components/telemetry-station/README.md) for more details on the system architecture and how the data that this application receives over UART is formatted.

## Folder Structure

- Each file is pretty self explanatory, feel free to take a look inside!

## Setup and Installation

1. Install python on your local machine (https://www.python.org/downloads/)

   To verify python is installed on your machine run `python --version`

2. Create a python virtual environment and activate it:

   On Windows Command Prompt:
   ```
   # Create virtual enviroment in venv folder
   python -m venv venv

   # Initialize virtual environment
   ./venv/Scripts/activate
   ```

   On Unix systems (macOS/Linux):
   ```
   # Create virtual enviroment in venv folder
   pytohon -m venv venv

   # Initialize virtual environment
   source ./venv/bin/activate
   ```

3. Intall all the required python modules:
   ```
   pip install -r requirements.txt
   ```

4. Run the python application:
   ```
   python telemetry-app.py
   ```

5. OPTIONAL: Create the application executable (`.exe` file for Windows):
   ```
   # Only works on Windows
   ./create_exe.bat

   # OR

   pyinstaller --onefile --add-data "ws_home.html;." --add-data "chart.min.js;." --add-data "favicon.ico;." --add-data "ws_home.js;."  --add-data "..\..\..\shared-resources\dbc_to_c\generated_code\VEHICLE_CAN.json;." .\telemetry-app.py
   ```


## Contributing

Please follow these guidelines in `nms-electronics/README.md`

## License

- Chart.js License: MIT (`LICENSES/CHARTJS_LICENSE.txt`)

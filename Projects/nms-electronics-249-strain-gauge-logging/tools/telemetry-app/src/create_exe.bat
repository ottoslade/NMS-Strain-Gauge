@echo off
set /p VERSION=Enter version number: 

pyinstaller --onefile --add-data "ws_home.html;." --add-data "chart.min.js;." --add-data "favicon.ico;." --add-data "ws_home.js;."  --add-data "..\..\..\shared\dbc_to_c\generated_code\VEHICLE_CAN.json;." .\telemetry-app.py

set "OUTFILE=telemetry-app-v%VERSION%.exe"
copy /Y "dist\telemetry-app.exe" "..\release\%OUTFILE%"
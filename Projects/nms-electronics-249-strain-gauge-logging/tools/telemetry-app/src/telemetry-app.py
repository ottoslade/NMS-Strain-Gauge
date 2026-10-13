import asyncio
import serial
import threading
import websockets
import os
import http.server
import socketserver
import webbrowser
import sys

# WebSocket port
WS_PORT             = 8765
SERIAL_BAUD         = 1000000
SLCAN_DATA_LENGTH   = 29

# Global variable to store active WebSocket connections
connected_clients = set()

# Function to read data from the serial port
# SLCAN protocol: 't' + <CAN ID> + <DLC> + <DATA> + <TIMESTAMP> + '\r'
# Find more about this protocol in the README.md file of telemetry-station 
def serial_reader(serial_port, loop):
    try:
        ser = serial.Serial(serial_port, SERIAL_BAUD, timeout=0.1)
        while True:
            # Read data from the serial line and process it
            data = b''
            count = 0
            if ser.read(1) == b't':
                count += 1
                while (read := ser.read(1)) != b'\r':
                    data += read
                    count += 1
                    if count > SLCAN_DATA_LENGTH:
                        break
                if count <= SLCAN_DATA_LENGTH:
                    handle_serial_data(data, loop)
    except serial.SerialException as e:
        sys.exit(0)
        print(f"Error reading serial port {serial_port}: {e}")

def slcan_to_byte_array(hex_str):
    hex_str = hex_str.decode()
    can_id = int(hex_str[:3], 16)
    dlc = int(hex_str[3], 16)
    data_bytes = bytearray.fromhex(hex_str[4:4 + dlc * 2])
    timestamp_bytes = bytearray.fromhex(hex_str[4 + dlc * 2:])
    can_id_bytes = can_id.to_bytes(2, byteorder='little')

    # Combine all parts
    return can_id_bytes + bytearray([dlc]) + data_bytes + timestamp_bytes

def handle_serial_data(data, loop):
    packet = slcan_to_byte_array(data)
    for websocket in connected_clients:
        # Correct way to call from a different thread
        asyncio.run_coroutine_threadsafe(send_data_to_ws(websocket, packet), loop)

async def send_data_to_ws(websocket, data):
    try:
        # You can customize the data encoding here
        # For now, we'll send the raw bytes (as binary frame)
        await websocket.send(data)
    except websockets.exceptions.ConnectionClosed:
        print(f"Connection closed for WebSocket client: {websocket}")
        # Remove client from the set if connection is closed
        connected_clients.discard(websocket)
        sys.exit(0)

# WebSocket handler
async def handle_ws(websocket):
    # Add the new WebSocket client to the set of connected clients
    connected_clients.add(websocket)
    print(f"New WebSocket client connected: {websocket}")
    
    try:
        while True:
            # Wait for any messages from the client (if needed)
            data = await websocket.recv()
            print(f"Received WebSocket message: {data}")
    except websockets.ConnectionClosed:
        print(f"WebSocket connection closed: {websocket}")
    finally:
        # Ensure that the client is removed from the set when the connection is closed
        connected_clients.discard(websocket)
        print(f"WebSocket client disconnected: {websocket}")
        sys.exit(0)

# Async function to start WebSocket server
async def start_websocket_server():
    server = await websockets.serve(handle_ws, '0.0.0.0', WS_PORT)
    print(f"WebSocket server running on ws://localhost:{WS_PORT}")
    await server.wait_closed()

# HTTP server to serve static files (e.g., ws_home.html)
class SimpleHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def translate_path(self, path):
        # Determine base directory
        if getattr(sys, 'frozen', False):
            # Running from PyInstaller bundle
            base_path = sys._MEIPASS
        else:
            # If requesting VEHICLE_CAN.json, serve from shared-resources
            if path.lstrip('/') == "VEHICLE_CAN.json":
                base_path = os.path.abspath(
                    os.path.join(
                        os.path.dirname(__file__),
                        "..", "..", "..", "shared", "dbc_to_c", "generated_code"
                    )
                )
            else:
                base_path = os.path.dirname(__file__)

        # Clean up the incoming URL path
        path = path.lstrip('/')
        return os.path.join(base_path, path)

    def do_GET(self):
        if self.path in ('/', '/index.html'):
            self.path = '/ws_home.html'
        return super().do_GET()

# Main function to start both servers
def main():
    port = None
    while not port:
        port = input("Enter the serial port (e.g., COM3 or /dev/ttyUSB0): ")
        
    # Create a new event loop
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)

    # Start serial reader in a background thread
    threading.Thread(target=serial_reader, args=(port, loop,), daemon=True).start()

    # Start HTTP server to serve static files
    http_port = 8000
    http_handler = SimpleHTTPRequestHandler
    http_server = socketserver.TCPServer(("", http_port), http_handler)
    print(f"Serving HTTP on http://localhost:{http_port}")
    
    # Run HTTP server in the background
    http_thread = threading.Thread(target=http_server.serve_forever, daemon=True)
    http_thread.start()

    webbrowser.open("http://localhost:8000/")

    # Start WebSocket server using the created event loop
    loop.run_until_complete(start_websocket_server())
    loop.run_forever()

if __name__ == "__main__":
    main()

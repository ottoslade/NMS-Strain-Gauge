#!/usr/bin/env python3

"""
Flashes an STM32 microcontroller using a CAN bootloader protocol.

This script supports multiple 'python-can' bustypes, including:
- 'socketcan' (e.g., CANAble/Candlelight, 'can0')
- 'slcan' (e.g., USB-to-serial CAN adapter, '/dev/ttyUSB0')
"""

import sys
import time
import logging
import argparse
import can
from can import BusABC
from typing import List, Optional

# --- Protocol Constants ---
ACK: int = 0x79
NACK: int = 0x1F
CAN_TIMEOUT: float = 2.0  # seconds
CAN_ERASE_TIMEOUT: float = 10.0 # seconds
STM_BOOT_ADDRESS: int = 0x0800C000
PAGE_SIZE: int = 256
EXPECTED_BOOTLOADER_ID: List[int] = [0x04, 0x82]

# --- CAN Command IDs ---
ID_CMD_ID: int = 0x02
ERASE_CMD_ID: int = 0x43
WRITE_CMD_ID: int = 0x31
WRITE_DATA_ID: int = 0x04
READ_CMD_ID: int = 0x11
NACK_MSG_ID: int = 0x1F # Specific NACK message ID seen in C code

# Setup basic logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)
log = logging.getLogger(__name__)


class FirmwareFlasher:
    """
    Handles the STM32 flashing process over CAN.
    """

    def __init__(self, bus: BusABC):
        """
        Initializes the Flasher with an *existing* CAN bus.

        :param bus: An initialized 'python-can' Bus object.
        """
        if bus:
            self.bus = bus
            log.info("FirmwareFlasher initialized with provided CAN bus.")
        else:
            log.error("FirmwareFlasher failed to initialize with provided CAN bus.")
            sys.exit(1)

    def close(self):
        """Shuts down the CAN bus interface."""
        if self.bus:
            self.bus.shutdown()
            log.info("CAN bus closed.")

    def _send_msg(self, can_id: int, data: List[int]) -> None:
        """Helper function to send a CAN message."""
        try:
            msg = can.Message(arbitration_id=can_id, data=data, is_extended_id=False)
            self.bus.send(msg)
        except Exception as e:
            log.error(f"Failed to send CAN message: {e}")

    def _wait_for_ack(self, expected_id: int, timeout: float) -> bool:
        """
        Waits for a specific ACK response from the bootloader.
        This replicates the C 'waitForCanACK' function.
        """
        start_time = time.time()
        while time.time() - start_time < timeout:
            # Use a short recv timeout to remain responsive
            rx_msg = self.bus.recv(timeout=0.1)
            if rx_msg:
                # Check for ACK
                if (rx_msg.arbitration_id == expected_id and 
                    rx_msg.dlc > 0 and 
                    rx_msg.data[0] == ACK):
                    return True
                
                # Check for NACK in data (as per C code)
                if rx_msg.dlc > 0 and rx_msg.data[0] == NACK:
                    log.error(f"Received NACK (0x{NACK:02X}) in data from ID {hex(rx_msg.arbitration_id)}")
                    return False
        
        log.error(f"Timeout waiting for ACK from ID {hex(expected_id)}")
        return False

    def _address_to_bytes(self, address: int) -> List[int]:
        """Converts a 32-bit integer address to 4 big-endian bytes."""
        return [
            (address >> 24) & 0xFF,
            (address >> 16) & 0xFF,
            (address >> 8) & 0xFF,
            address & 0xFF
        ]
        
    def _pad_data(self, data: bytes, length: int, pad_byte: int = 0xFF) -> bytes:
        """Pads a byte string to a specific length."""
        return data + bytes([pad_byte] * (length - len(data)))

    def cmd_id(self) -> bool:
        """
        Sends the ID command and verifies the bootloader.
        Replicates the C 'cmdId' function.
        """
        log.debug("Sending ID command (0x02)")
        self._send_msg(ID_CMD_ID, [0x00])

        ID_CMD_ID_TIMEOUT = 0.1

        # 1. Wait for first ACK
        if not self._wait_for_ack(ID_CMD_ID, ID_CMD_ID_TIMEOUT):
            log.warning("ID CMD FAILED: did not receive first ACK")
            return False

        # 2. Wait for the ID data response
        id_msg = self.bus.recv(CAN_TIMEOUT)
        if not id_msg:
            log.warning("ID CMD FAILED: did not receive bootloader ID response")
            return False

        if id_msg.arbitration_id != ID_CMD_ID:
            log.warning(f"ID CMD FAILED: response has invalid CAN ID {hex(id_msg.arbitration_id)}")
            return False

        received_id = list(id_msg.data[:id_msg.dlc])
        if received_id != EXPECTED_BOOTLOADER_ID:
            log.warning(f"ID CMD FAILED: invalid bootloader ID. Got {received_id}, expected {EXPECTED_BOOTLOADER_ID}")
            return False

        # 3. Wait for second ACK
        if not self._wait_for_ack(ID_CMD_ID, CAN_TIMEOUT):
            log.warning("ID CMD FAILED: did not receive second ACK")
            return False

        log.info("Bootloader ID check successful")
        return True

    def cmd_erase(self) -> bool:
        """
        Sends the Erase command.
        Replicates the C 'cmdErase' function.
        """
        log.info("Sending Erase command (0x43)...")
        self._send_msg(ERASE_CMD_ID, [0xFF])

        # 1. Wait for first ACK
        if not self._wait_for_ack(ERASE_CMD_ID, CAN_TIMEOUT):
            log.error("ERASE CMD FAILED: did not receive first ACK")
            return False

        # 2. Wait for second ACK (long erase timeout)
        log.info("Erase in progress (waiting up to 10s for 2nd ACK)...")
        if not self._wait_for_ack(ERASE_CMD_ID, CAN_ERASE_TIMEOUT):
            log.error("ERASE CMD FAILED: did not receive second ACK")
            return False
            
        # 3. Wait for third ACK
        if not self._wait_for_ack(ERASE_CMD_ID, CAN_TIMEOUT):
            log.error("ERASE CMD FAILED: did not receive third ACK")
            return False

        log.info("Erase complete.")
        return True

    def _flash_page(self, address: int, data: bytes) -> bool:
        """
        Writes a single 256-byte page to flash.
        Replicates the C 'flashPage' function.
        """
        addr_bytes = self._address_to_bytes(address)
        
        # 1. Send Write command
        write_cmd_data = addr_bytes + [255] # 255 bytes to write (0xFF)
        self._send_msg(WRITE_CMD_ID, write_cmd_data)
        if not self._wait_for_ack(WRITE_CMD_ID, CAN_TIMEOUT):
            log.error(f"WRITE CMD FAILED: no ACK for address {hex(address)}")
            return False
            
        # 2. Send data blocks (32 blocks of 8 bytes)
        for i in range(0, PAGE_SIZE, 8):
            chunk = data[i : i + 8]
            # C code always sends 8 bytes, padded with 0xFF
            padded_chunk = self._pad_data(chunk, 8, 0xFF)
            
            self._send_msg(WRITE_DATA_ID, list(padded_chunk))
            
            # Wait for ACK from the *Write command ID* (0x31)
            if not self._wait_for_ack(WRITE_CMD_ID, CAN_TIMEOUT):
                log.error(f"WRITE DATA FAILED: no ACK for data at {hex(address + i)}")
                return False

        # 3. Wait for final ACK
        if not self._wait_for_ack(WRITE_CMD_ID, CAN_TIMEOUT):
            log.error(f"WRITE CMD FAILED: no final ACK for page {hex(address)}")
            return False

        return True

    def _read_page(self, address: int, expected_data: bytes) -> bool:
        """
        Reads a 256-byte page and verifies it against expected data.
        Replicates the C 'readPage' function.
        """
        addr_bytes = self._address_to_bytes(address)

        # 1. Send Read command
        read_cmd_data = addr_bytes + [255] # 255 bytes to read (0xFF)
        self._send_msg(READ_CMD_ID, read_cmd_data)
        if not self._wait_for_ack(READ_CMD_ID, CAN_TIMEOUT):
            log.error(f"READ CMD FAILED: no ACK for address {hex(address)}")
            return False
            
        # 2. Receive data blocks
        byte_count = 0
        while byte_count < PAGE_SIZE:
            rx_msg = self.bus.recv(CAN_TIMEOUT)
            if not rx_msg:
                log.error(f"READ DATA FAILED: Timeout waiting for data at {hex(address + byte_count)}")
                return False

            # Check for NACK message (as per C code)
            if rx_msg.arbitration_id == NACK_MSG_ID:
                log.error("READ DATA FAILED: Received NACK message while reading memory")
                return False
                
            if rx_msg.arbitration_id == READ_CMD_ID:
                received_chunk = rx_msg.data[:rx_msg.dlc]
                expected_chunk = expected_data[byte_count : byte_count + len(received_chunk)]
                
                if received_chunk != expected_chunk:
                    log.error(f"VERIFICATION FAILED at address {hex(address + byte_count)}")
                    log.error(f"  Expected: {expected_chunk.hex()}")
                    log.error(f"  Received: {received_chunk.hex()}")
                    # C code would re-erase here, we will just fail
                    return False
                
                byte_count += len(received_chunk)
            else:
                log.warning(f"Received unexpected CAN ID {hex(rx_msg.arbitration_id)} during read")

        # 3. Wait for final ACK
        if not self._wait_for_ack(READ_CMD_ID, CAN_TIMEOUT):
            log.error(f"READ CMD FAILED: no final ACK for page {hex(address)}")
            return False

        return True

    def flash_firmware(self, bin_file_path: str) -> bool:
        """
        Runs the full flashing procedure.
        Replicates the C 'flashSTM' function.
        """
        
        # --- 1. ID Command with Retry ---
        log.info("Attempting to contact bootloader (5s retry)...")
        start_retry_time = time.time()
        id_ok = False
        while time.time() - start_retry_time < 5.0:
            if self.cmd_id():
                id_ok = True
                break
            log.debug("ID retry...")

        if not id_ok:
            log.error("Failed to get bootloader ID after 5 seconds.")
            return False
            
        # --- 2. Erase Command ---
        if not self.cmd_erase():
            log.error("Failed to erase device.")
            return False

        # --- 3. Write Task ---
        log.info("Starting write task...")
        try:
            with open(bin_file_path, 'rb') as f:
                firmware_data = f.read()
        except FileNotFoundError:
            log.error(f"File not found: {bin_file_path}")
            return False
        except Exception as e:
            log.error(f"Failed to read file: {e}")
            return False
            
        address = STM_BOOT_ADDRESS
        for i in range(0, len(firmware_data), PAGE_SIZE):
            page_data = firmware_data[i : i + PAGE_SIZE]
            # Pad the last page to 256 bytes with 0xFF, as C code implies
            padded_page = self._pad_data(page_data, PAGE_SIZE, 0xFF)
            
            log.info(f"Writing page at {hex(address)} ({i // PAGE_SIZE + 1} / {len(firmware_data) // PAGE_SIZE + 1})")
            if not self._flash_page(address, padded_page):
                log.error(f"Failed to write page at {hex(address)}")
                return False
            address += PAGE_SIZE
        log.info("Write task complete.")

        # --- 4. Read/Verify Task ---
        log.info("Starting read & verification task...")
        address = STM_BOOT_ADDRESS
        for i in range(0, len(firmware_data), PAGE_SIZE):
            page_data = firmware_data[i : i + PAGE_SIZE]
            padded_page = self._pad_data(page_data, PAGE_SIZE, 0xFF)
            
            log.info(f"Verifying page at {hex(address)} ({i // PAGE_SIZE + 1} / {len(firmware_data) // PAGE_SIZE + 1})")
            if not self._read_page(address, padded_page):
                log.error(f"Failed to verify page at {hex(address)}")
                return False
            address += PAGE_SIZE
        log.info("Verification complete.")

        log.info("=" * 30)
        log.info(" STM32 Flashed Successfully!!!")
        log.info("=" * 30)
        return True


# --- THIS SECTION IS MODIFIED ---
def main():
    parser = argparse.ArgumentParser(
        description="STM32 CAN Bootloader Flasher.",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog="""
Usage Examples:

1. For socketcan (e.g., CANAble on 'can0'):
   (First, run: sudo ip link set can0 type can bitrate 500000 up)
   python3 stm_flasher.py -t socketcan -c can0 -f firmware.bin

2. For slcan (e.g., USB-to-serial adapter on '/dev/ttyUSB0'):
   python3 stm_flasher.py -t slcan -c /dev/ttyUSB0 -b 500000 -f firmware.bin
"""
    )
    # --bustype argument
    parser.add_argument(
        "-t", "--bustype",
        required=True,
        choices=['socketcan', 'slcan'],
        help="The 'python-can' bustype to use."
    )
    # --channel argument
    parser.add_argument(
        "-c", "--channel",
        required=True,
        help="The interface channel (e.g., 'can0', '/dev/ttyUSB0', 'COM3')"
    )
    # --file argument
    parser.add_argument(
        "-f", "--file",
        required=True,
        help="Path to the .bin firmware file to flash"
    )
    # --bitrate argument (now optional, but required by slcan)
    parser.add_argument(
        "-b", "--bitrate",
        type=int,
        help="CAN bus bitrate (e.g., 500000). Required for 'slcan'."
    )
    # --verbose argument
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable debug logging"
    )
    
    args = parser.parse_args()
    
    if args.verbose:
        log.setLevel(logging.DEBUG)

    # Check for conditional requirement: bitrate is needed for slcan
    if args.bustype == 'slcan' and args.bitrate is None:
        parser.error("--bitrate is required when --bustype is 'slcan'")
    
    bustype = args.bustype
    channel = args.channel
    bitrate = args.bitrate
    bus = None

    try:
        if bustype == 'socketcan':
            log.info(f"Using 'socketcan' on channel '{channel}'.")
            log.info(f"Ensure you have set the bitrate externally (e.g., 'sudo ip link set {channel} type can bitrate ... up')")
            bus = can.interface.Bus(channel=channel, bustype=bustype)
        
        elif bustype == 'slcan':
            if not bitrate:
                log.error(f"Bitrate is required for 'slcan' bustype. Please provide --bitrate.")
                sys.exit(1)
            log.info(f"Using 'slcan' on channel '{channel}' at {bitrate} bps.")
            bus = can.interface.Bus(
                channel=channel, bustype=bustype, bitrate=bitrate
            )
        
        else:
            log.error(f"Unsupported bustype: {bustype}")
            sys.exit(1)

        # Clear any stale messages in the buffer
        while bus.recv(timeout=0):
            pass

        log.info(f"CAN bus opened successfully.")

    except Exception as e:
        log.error(f"Failed to open CAN bus on {channel} (bustype: {bustype}): {e}")
        log.error(f"Please ensure the device is connected and drivers are installed.")
        sys.exit(1)

    # Pass all relevant args to the flasher
    flasher = FirmwareFlasher(bus=bus)
    
    try:
        success = flasher.flash_firmware(args.file)
        if not success:
            sys.exit(1) # Exit with error code if flashing failed
            
    except KeyboardInterrupt:
        log.warning("\nFlashing process interrupted by user.")
        sys.exit(1)
    except Exception as e:
        log.error(f"An unhandled error occurred: {e}")
        sys.exit(1)
    finally:
        flasher.close()
# --- END OF MODIFIED SECTION ---

if __name__ == "__main__":
    main()
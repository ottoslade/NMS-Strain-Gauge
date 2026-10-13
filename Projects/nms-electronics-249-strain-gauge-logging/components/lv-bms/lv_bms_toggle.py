import argparse
import time

import can
import logging
import sys

# Setup basic logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)
log = logging.getLogger(__name__)

null_msg = can.Message(
    arbitration_id=0x7f2,
    is_extended_id=False,
    data=[0b00000000]
)
chg_msg = can.Message(
    arbitration_id=0x7f2,
    is_extended_id=False,
    data=[0b00000001]
)
dsg_msg = can.Message(
    arbitration_id=0x7f2,
    is_extended_id=False,
    data=[0b00000010]
)
shutdown_msg = can.Message(
    arbitration_id=0x7f2,
    is_extended_id=False,
    data=[0b10000000]
)

def main():
    parser = argparse.ArgumentParser(
        description="LV BMS CAN controller",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog="""
Usage Examples:

1. For socketcan (e.g., CANAble on 'can0'):
   (First, run: sudo ip link set can0 type can bitrate 500000 up)
   python3 lv_bms_toggle.py -t socketcan -c can0 charge

2. For slcan (e.g., USB-to-serial adapter on '/dev/ttyUSB0'):
   python3 lv_bms_toggle.py -t slcan -c /dev/ttyUSB0 -b 500000 charge
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
    # --bitrate argument (now optional, but required by slcan)
    parser.add_argument(
        "-b", "--bitrate",
        type=int,
        help="CAN bus bitrate (e.g., 500000). Required for 'slcan'."
    )
    # BMS state argument
    parser.add_argument(
        "bmsstate",
        choices=["charge", "discharge", "shutdown"]
    )

    args = parser.parse_args()

    # Check for conditional requirement: bitrate is needed for slcan
    if args.bustype == 'slcan' and args.bitrate is None:
        parser.error("--bitrate is required when --bustype is 'slcan'")

    bustype = args.bustype
    channel = args.channel
    bitrate = args.bitrate
    bmsstate = args.bmsstate
    bus = None

    try:
        if bustype == 'socketcan':
            log.info(f"Using 'socketcan' on channel '{channel}'.")
            log.info(
                f"Ensure you have set the bitrate externally (e.g., 'sudo ip link set {channel} type can bitrate ... up')")
            bus = can.Bus(channel=channel, interface=bustype)

        elif bustype == 'slcan':
            if not bitrate:
                log.error(f"Bitrate is required for 'slcan' bustype. Please provide --bitrate.")
                sys.exit(1)
            log.info(f"Using 'slcan' on channel '{channel}' at {bitrate} bps.")
            bus = can.Bus(
                channel=channel, interface=bustype, bitrate=bitrate
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

    try:
        if bmsstate == "charge":
            bus.send(chg_msg)
        elif bmsstate == "discharge":
            bus.send(dsg_msg)
        else:
            bus.send(shutdown_msg)
        time.sleep(1) # need to wait for the CAN task to read the prev msg before sending another
        bus.send(null_msg)
        log.info(f"{bmsstate} message sent")
    except can.CanError:
        log.error("Message failed to send!")

    bus.shutdown()


if __name__ == "__main__":
    main()

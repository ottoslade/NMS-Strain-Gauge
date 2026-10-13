import logging
import subprocess
import time
from pathlib import Path

from configs.ecu_config import ECUConfig
from firmware_flasher import FirmwareFlasher

from drivers.can_driver import CanDriver

log = logging.getLogger(__name__)


def reset_ecu(ecu: ECUConfig) -> None:
    """Function to reset an ECU"""
    ecu.reset_gpio.write(0)
    time.sleep(0.1)
    ecu.reset_gpio.write(1)
    time.sleep(0.5)
    log.info(f"Reset {ecu.ecu_name}")


def build_ecu(ecu: ECUConfig) -> bool:
    """Build firmware for an ECU"""
    file_directory = Path(__file__).parent
    ecu_directory = file_directory / "../../../../components" / ecu.ecu_name
    ecu_directory = ecu_directory.resolve()

    log.info(f"Building {ecu.ecu_name}...")

    try:
        subprocess.run(
            ["make"],
            cwd=ecu_directory,
            check=True,  # Raise CalledProcessError if the command returns a non-zero exit code
            capture_output=True,
            text=True,
            shell=False,
        )
        log.info(f"Built {ecu.ecu_name}")
        return True
    except subprocess.CalledProcessError as e:
        log.error(f"Error building firmware for {ecu.ecu_name}: 'make' failed.")
        log.error(f"Command: {' '.join(e.cmd)}")
        log.error(f"Return code: {e.returncode}")
        log.error(f"Stdout:\n{e.stdout}")
        log.error(f"Stderr:\n{e.stderr}")
        return False
    except FileNotFoundError:
        log.error("Error: 'make' command not found. Ensure make is installed and in PATH.")
        return False
    except Exception as e:
        log.error(f"An unexpected error occurred while running 'make': {e}")
        return False


def flash_ecu(ecu: ECUConfig, can_bus: CanDriver) -> bool:
    """Flash firmware to an ECU"""

    # 1. Get the directory containing the Python file that is running
    file_directory = Path(__file__).parent

    # 2. Go from that directory to your target path
    bin_name = ecu.ecu_name + ".bin"
    bin_file_path = file_directory / "../../../../components" / ecu.ecu_name / "build" / bin_name

    # 3. (Recommended) Resolve the path to get the clean, absolute path
    #    This removes all the ".." parts
    bin_file_path = bin_file_path.resolve()

    flasher = FirmwareFlasher(can_bus.bus)

    # Clear any stale messages in the buffer
    while can_bus.bus.recv(timeout=0):
        pass

    reset_ecu(ecu)

    ret = flasher.flash_firmware(str(bin_file_path))

    reset_ecu(ecu)

    return ret

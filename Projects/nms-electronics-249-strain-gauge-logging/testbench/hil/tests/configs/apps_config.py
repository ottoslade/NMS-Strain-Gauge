import logging
import random
import time

from cantools.database import Database

from configs.can_config import CANId
from drivers.can_driver import CanDriver
from drivers.dac_driver import DACDriver

log = logging.getLogger(__name__)

APPS_LEFT_MIN_BIT = 497
APPS_LEFT_MAX_BIT = 3520
APPS_RIGHT_MIN_BIT = 863
APPS_RIGHT_MAX_BIT = 4037
BITS_PER_PERCENT_LEFT = (
    APPS_LEFT_MAX_BIT - APPS_LEFT_MIN_BIT
) / 100.0  # number of bits per percent of pedal travel
BITS_PER_PERCENT_RIGHT = (APPS_RIGHT_MAX_BIT - APPS_RIGHT_MIN_BIT) / 100.0
PERCENT_OFFSET_INT = 5  # percent of deadzone we want on each end (10% deadzone)
APPS_UNPLUG_DEADZONE_PERCENT = -10
SOFTWARE_PEDAL_OFFSET_BIT_LEFT = (
    BITS_PER_PERCENT_LEFT * PERCENT_OFFSET_INT
)  # calc for that deadzone
SOFTWARE_PEDAL_OFFSET_BIT_RIGHT = (
    BITS_PER_PERCENT_RIGHT * PERCENT_OFFSET_INT
)  # calc for that deadzone
MAX_APPS_DIFF_PERCENT = 10
MAX_TORQUE = 230
MAX_POWER = 78
EFFICIENCY_FACTOR = 0.90


def apps_over_can_test(
    can_bus: CanDriver,
    vehicle_db: Database,
    apps_dac: DACDriver,
    apps_min: int,
    apps_max: int,
    apps_signal: str,
) -> None:
    """Generic function to test an APPS signal over CAN"""
    # Step 1: Randomly select DAC values to set the APPS sensor to
    apps_bits = random.randint(apps_min, apps_max)

    # Step 2: Set the APPS sensor to the previously selected value
    apps_dac.set_value(apps_bits)

    log.info(f"Set {apps_signal} DAC value to bits: {apps_bits}")

    time.sleep(0.1)

    # Step 3: Calculate APPS percentage
    apps_percent = (int)(((apps_bits - apps_min) / (apps_max - apps_min)) * 100)
    log.info(f"Expecting {apps_signal} signal on CAN to be {apps_percent}%")

    # Step 4: Check the APPS left percentage over CAN
    result = can_bus.read_value(
        id=CANId.APPS,
        signal=apps_signal,
        expected=apps_percent,
        db=vehicle_db,
        timeout_s=1,
        tolerance=5,
    )

    assert result.is_ok(), result.value

    log.info(f"{apps_signal} signal on CAN was correctly set to {apps_percent}%")

import logging
import random
import time

from cantools.database import Database

from configs.can_config import CANId
from drivers.can_driver import CanDriver
from drivers.dac_driver import DACDriver

log = logging.getLogger(__name__)

F_BRAKE_MIN_BIT = 503
F_BRAKE_MAX_BIT = 3686
R_BRAKE_MIN_BIT = 467
R_BRAKE_MAX_BIT = 3686
F_BRAKE_BPP_BIT = (F_BRAKE_MAX_BIT - F_BRAKE_MIN_BIT) // 100
R_BRAKE_BPP_BIT = (R_BRAKE_MAX_BIT - R_BRAKE_MIN_BIT) // 100
PERCENT_NOISE_BIT = 5
F_BRAKE_NOISE_BIT = F_BRAKE_BPP_BIT * PERCENT_NOISE_BIT
R_BRAKE_NOISE_BIT = R_BRAKE_BPP_BIT * PERCENT_NOISE_BIT

BRAKE_SENSOR_VREF = 5.0
ADC_TOTAL_BITS = 4095
BRAKE_SENSOR_RANGE_PSI = 2500
BRAKE_SENSOR_RANGE_VOLTAGE = 4


def brake_bits_to_pressure(bits: int, min_bit: int) -> float:
    """Function to return the brake pressure given the ADC/DAC bits"""
    volts_scaled = (bits - min_bit) * BRAKE_SENSOR_VREF / ADC_TOTAL_BITS

    return volts_scaled * BRAKE_SENSOR_RANGE_PSI / BRAKE_SENSOR_RANGE_VOLTAGE


def brake_over_can_test(
    can_bus: CanDriver,
    vehicle_db: Database,
    brake_dac: DACDriver,
    brake_min: int,
    brake_max: int,
    brake_signal: str,
) -> None:
    """Generic function to test an brake pressure sensor signal over CAN"""
    # Step 1: Randomly select DAC values to set the brake sensors to
    brake_pressure_bits = random.randint(brake_min, brake_max)

    # Step 2: Convert the DAC values
    brake_pressure_front = brake_bits_to_pressure(brake_pressure_bits, brake_min)

    # Step 3: Set the brake pressure sensors to the previously selected values
    brake_dac.set_value(brake_pressure_bits)

    log.info(f"Set {brake_signal} DAC value to bits: {brake_pressure_bits}")
    log.info(f"Expecting {brake_signal} signal on CAN to be ~{brake_pressure_front} psi")

    time.sleep(0.1)

    # Step 4: Check the brake pressures over CAN and check if the pressure matches
    # what we sent
    result = can_bus.read_value(
        id=CANId.BRAKE_PRESSURE,
        signal=brake_signal,
        expected=brake_pressure_front,
        db=vehicle_db,
        timeout_s=1,
        tolerance=50,
    )

    assert result.is_ok(), result.value

    log.info(f"{brake_signal} signal on CAN was correctly set to ~{brake_pressure_front} psi")

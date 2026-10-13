import logging
import random
import time

from cantools.database import Database
from configs.apps_config import *
from configs.brake_config import *
from configs.rpi_config import RPiDAC, RPiGPIO

from drivers.can_driver import CanDriver

log = logging.getLogger(__name__)


def test_eeprom_apps(can_bus: CanDriver, vehicle_db: Database) -> None:
    """Test if EEPROM sets APPS max bit correctly"""

    log.info(
        "Testing that EEPROM APPS button can correctly update APPS min and max bits in the EEPROM"
    )

    APPS_LEFT_RANDOM_MIN_BITS = random.randint(0, 4095 - 1)
    APPS_LEFT_RANDOM_MAX_BITS = random.randint(APPS_LEFT_RANDOM_MIN_BITS + 1, 4095)
    APPS_RIGHT_RANDOM_MIN_BITS = random.randint(0, 4095 - 1)
    APPS_RIGHT_RANDOM_MAX_BITS = random.randint(APPS_RIGHT_RANDOM_MIN_BITS + 1, 4095)

    RPiDAC.APPSLeft.set_value(APPS_LEFT_RANDOM_MIN_BITS)
    RPiDAC.APPSRight.set_value(APPS_RIGHT_RANDOM_MIN_BITS)

    log.info("Update APPS Left and Right DAC values to record min")
    log.info(f"Set APPS Left DAC to bits: {APPS_LEFT_RANDOM_MIN_BITS}")
    log.info(f"Set APPS Right DAC to bits: {APPS_RIGHT_RANDOM_MIN_BITS}")

    RPiGPIO.EEPROMWriteAPPS.press_button()
    log.info("Pressed EEPROM APPS button")

    log.info("Wait 1 second before updating the APPS Left and Right DAC values")
    time.sleep(1)

    RPiDAC.APPSLeft.set_value(APPS_LEFT_RANDOM_MAX_BITS)
    RPiDAC.APPSRight.set_value(APPS_RIGHT_RANDOM_MAX_BITS)
    log.info("Update APPS Left and Right DAC values to record max")
    log.info(f"Set APPS Left DAC to bits: {APPS_LEFT_RANDOM_MAX_BITS}")
    log.info(f"Set APPS Right DAC to bits: {APPS_RIGHT_RANDOM_MAX_BITS}")

    log.info("Wait 6 seconds because the EEPROM logic records the max value after 5 seconds")
    # wait 6 seconds because the EEPROM logic records the max
    # value after 5 seconds
    time.sleep(6)

    log.info("Testing APPS Left (APPS1) signal over CAN based on the new min-max range")
    apps_over_can_test(
        can_bus,
        vehicle_db,
        RPiDAC.APPSLeft,
        APPS_LEFT_RANDOM_MIN_BITS,
        APPS_LEFT_RANDOM_MAX_BITS,
        "APPS1",
    )

    log.info("Testing APPS Right (APPS2) signal over CAN based on the new min-max range")
    apps_over_can_test(
        can_bus,
        vehicle_db,
        RPiDAC.APPSRight,
        APPS_RIGHT_RANDOM_MIN_BITS,
        APPS_RIGHT_RANDOM_MAX_BITS,
        "APPS2",
    )


def test_eeprom_brake(can_bus: CanDriver, vehicle_db: Database) -> None:
    """Test if EEPROM sets brake min bit correctly"""

    log.info("Testing that EEPROM Brake button can correctly update Brake min bits in the EEPROM")

    F_BRAKE_RANDOM_MIN_BITS = random.randint(0, F_BRAKE_MAX_BIT - 1)
    R_BRAKE_RANDOM_MIN_BITS = random.randint(0, R_BRAKE_MAX_BIT - 1)

    RPiDAC.BrakesFront.set_value(F_BRAKE_RANDOM_MIN_BITS)
    RPiDAC.BrakesRear.set_value(R_BRAKE_RANDOM_MIN_BITS)

    log.info("Update Brake Front and Rear DAC values to record min")
    log.info(f"Set Brake Front DAC to bits: {F_BRAKE_RANDOM_MIN_BITS}")
    log.info(f"Set Brake Rear DAC to bits: {R_BRAKE_RANDOM_MIN_BITS}")

    RPiGPIO.EEPROMWriteBrakes.press_button()
    log.info("Pressed EEPROM Brake button")

    log.info("Wait 1 second to let the firmware store the values")
    time.sleep(1)

    log.info("Testing Brake Front (BrakeSensor1) signal over CAN based on the new min value")
    brake_over_can_test(
        can_bus,
        vehicle_db,
        RPiDAC.BrakesFront,
        F_BRAKE_RANDOM_MIN_BITS,
        F_BRAKE_MAX_BIT,
        "BrakeSensor1",
    )

    log.info("Testing Brake Rear (BrakeSensor2) signal over CAN based on the new min value")
    brake_over_can_test(
        can_bus,
        vehicle_db,
        RPiDAC.BrakesRear,
        R_BRAKE_RANDOM_MIN_BITS,
        R_BRAKE_MAX_BIT,
        "BrakeSensor2",
    )


def test_restore_eeprom_default(can_bus: CanDriver, vehicle_db: Database) -> None:
    """Restore all APPS and BRAKES default values on EEPROM"""

    log.info("Restoring default min and max values for both Brakes and APPS")

    RPiDAC.APPSLeft.set_value(APPS_LEFT_MIN_BIT)
    RPiDAC.APPSRight.set_value(APPS_RIGHT_MIN_BIT)
    time.sleep(0.1)

    # eeprom apps button press
    RPiGPIO.EEPROMWriteAPPS.press_button()

    time.sleep(2.0)

    RPiDAC.APPSLeft.set_value(APPS_LEFT_MAX_BIT)
    RPiDAC.APPSRight.set_value(APPS_RIGHT_MAX_BIT)

    time.sleep(4)

    RPiDAC.BrakesFront.set_value(F_BRAKE_MIN_BIT)
    RPiDAC.BrakesRear.set_value(R_BRAKE_MIN_BIT)

    RPiGPIO.EEPROMWriteBrakes.press_button()

    time.sleep(1)

    assert True

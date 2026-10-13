import logging
import time

from cantools.database import Database
from configs.brake_config import *
from configs.can_config import CANId
from configs.rpi_config import RPiDAC

from drivers.can_driver import CanDriver

log = logging.getLogger(__name__)


def test_front_brake_over_can(can_bus: CanDriver, vehicle_db: Database) -> None:
    """
    Test if ecu-front correctly reads voltage form the front brake pressure sensors,
    converts it to pressure (in psi), and correctly sends it over CAN
    """
    log.info("Testing Brake Front (BrakeSensor1) signal over CAN")

    brake_over_can_test(
        can_bus, vehicle_db, RPiDAC.BrakesFront, F_BRAKE_MIN_BIT, F_BRAKE_MAX_BIT, "BrakeSensor1"
    )


def test_rear_brake_over_can(can_bus: CanDriver, vehicle_db: Database) -> None:
    """
    Test if ecu-front correctly reads voltage form the rear brake pressure sensors,
    converts it to pressure (in psi), and correctly sends it over CAN
    """
    log.info("Testing Brake Rear (BrakeSensor2) signal over CAN")

    brake_over_can_test(
        can_bus, vehicle_db, RPiDAC.BrakesRear, R_BRAKE_MIN_BIT, R_BRAKE_MAX_BIT, "BrakeSensor2"
    )


def test_brakelight_on(can_bus: CanDriver, vehicle_db: Database) -> None:
    """Test if brakelight turns on if brake sensor is set to HIGH"""

    log.info("Testing that brake light signal on CAN is set if brakes are pressed")

    # Step 1: Send brake pressue (add 50 to make sure that we're not losing
    # bits over the DAC transfer)
    RPiDAC.BrakesFront.set_value(F_BRAKE_MIN_BIT + F_BRAKE_NOISE_BIT + 50)

    log.info(f"Set front brake DAC to bits: {F_BRAKE_MIN_BIT + F_BRAKE_NOISE_BIT + 50}")

    time.sleep(0.1)

    # Step 2: Check that brake light is on (timeout if no msg)
    result = can_bus.read_value_consistent(
        id=CANId.BRAKE_LIGHT,
        signal="BrakeLightActive",
        expected="ENABLE",
        db=vehicle_db,
        timeout_s=5,
    )
    assert result.is_ok(), result.value

    log.info("BrakeLightActive signal on CAN was correctly set to ENABLE")


def test_brakelight_off(can_bus: CanDriver, vehicle_db: Database) -> None:
    """Test if brakelight stays off if brake sensor is set to LOW"""

    log.info("Testing that brake light signal on CAN is not set if brakes are not pressed")

    # Logging this but this is really done in the DAC initialization fixture in rpi_fixtures.py
    log.info(f"Set front brake DAC to bits: {F_BRAKE_MIN_BIT}")

    # Step 1: Check that brake light is off (timeout if no msg)
    result = can_bus.read_value_consistent(
        id=CANId.BRAKE_LIGHT,
        signal="BrakeLightActive",
        expected="DISABLE",
        db=vehicle_db,
        timeout_s=5,
    )
    assert result.is_ok(), result.value

    log.info("BrakeLightActive signal on CAN was correctly set to DISABLE")

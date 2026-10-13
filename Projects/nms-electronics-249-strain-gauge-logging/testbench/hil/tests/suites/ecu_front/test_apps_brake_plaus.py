import logging
import random
import time

from cantools.database import Database
from configs.apps_config import *
from configs.brake_config import *
from configs.can_config import CANId
from configs.rpi_config import RPiDAC

from drivers.can_driver import CanDriver

log = logging.getLogger(__name__)


def test_apps_brake_plaus(can_bus: CanDriver, vehicle_db: Database) -> None:
    """
    Test APPS Plausibility:
    1) If brakes are pressed and APPS percent > 25, we should command 0 torque
    2) If brakes are released and APPS percent > 5, we should still command 0 torque
    3) If APPS percent < 5, we should command torque regularly
    """
    log.info("Testing that Torque_Command is set to 0 if APPS > 25% and brakes are pressed")

    # send apps over 25% and press brakes
    RPiDAC.BrakesFront.set_value(F_BRAKE_MIN_BIT + F_BRAKE_NOISE_BIT + 50)

    log.info("Set front brake value to simulate pressure")
    log.info(f"Set front brake DAC to bits: {F_BRAKE_MIN_BIT + F_BRAKE_NOISE_BIT + 50}")

    percent = random.randint(26, 100) / 100.0

    apps_bits = int((APPS_LEFT_MAX_BIT - APPS_LEFT_MIN_BIT) * percent) + APPS_LEFT_MIN_BIT

    RPiDAC.APPSLeft.set_value(int(apps_bits))

    log.info(f"Set APPS to a random value > 25%: {percent * 100}%")
    log.info(f"Set APPS left DAC to bits: {int(apps_bits)}")

    time.sleep(0.1)

    result = can_bus.read_value(
        CANId.M192_COMMAND_MESSAGE,
        "Torque_Command",
        0,
        vehicle_db,
        2,
    )

    assert result.is_ok(), result.value

    log.info("Torque_Command signal was correctly set to 0")

    log.info("Testing that Torque_Command is set to 0 if brakes are released but APPS value > 5%")

    # should still be 0 if apps is still > 5%
    RPiDAC.BrakesFront.set_value(F_BRAKE_MIN_BIT)

    log.info("Set front brake value to simulate no pressure")
    log.info(f"Set front brake DAC to bits: {F_BRAKE_MIN_BIT}")

    time.sleep(0.1)

    result = can_bus.read_value(
        CANId.M192_COMMAND_MESSAGE,
        "Torque_Command",
        0,
        vehicle_db,
        2,
    )

    assert result.is_ok(), result.value
    log.info("Torque_Command signal was correctly set to 0")

    # should stay at 0 if APPS < 5% because we only start commanding torque when APPS > 5%
    log.info("Testing that Torque_Command is set to 0 if APPS < 5%")

    percent = random.randint(0, 5) / 100.0

    apps_bits = int((APPS_LEFT_MAX_BIT - APPS_LEFT_MIN_BIT) * percent) + APPS_LEFT_MIN_BIT

    RPiDAC.APPSLeft.set_value(int(apps_bits))

    log.info(f"Set APPS to a random value < 5%: {percent * 100}%")
    log.info(f"Set APPS left DAC to bits: {int(apps_bits)}")

    time.sleep(0.1)

    result = can_bus.read_value(CANId.M192_COMMAND_MESSAGE, "Torque_Command", 0, vehicle_db, 2)

    assert result.is_ok(), result.value

    log.info("Torque_Command signal was correctly set to 0")

    time.sleep(1)

    # should allow torque after APPS was set to < 5% and then increased
    log.info("Testing that Torque_Command > 0 if torque is commanded")

    percent = random.randint(0, 100) / 100.0

    apps_bits = int(
        (APPS_LEFT_MAX_BIT - (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)) * percent
    ) + (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)

    torque_percent = (apps_bits - (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)) / (
        APPS_LEFT_MAX_BIT - (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)
    )

    torque_nm = int(torque_percent * MAX_TORQUE)

    RPiDAC.APPSLeft.set_value(int(apps_bits))
    log.info(
        f"Set APPS to a random value > 0% (based on the min offset needed \
        to generate torque): {percent * 100}%"
    )
    log.info(f"Set APPS left DAC to bits: {int(apps_bits)}")
    log.info(f"Expecting Torque_Command == {torque_nm}")

    time.sleep(0.1)

    result = can_bus.read_value(
        CANId.M192_COMMAND_MESSAGE, "Torque_Command", torque_nm, vehicle_db, 2, 5
    )

    assert result.is_ok(), result.value

    log.info(f"Torque_Command signal was correctly set to {torque_nm}")

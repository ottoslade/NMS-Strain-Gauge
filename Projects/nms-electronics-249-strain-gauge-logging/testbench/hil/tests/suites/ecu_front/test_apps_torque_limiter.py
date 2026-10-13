import logging
import random
import time

import pytest
from cantools.database import Database
from configs.apps_config import *
from configs.brake_config import *
from configs.can_config import CANId, populate_signals
from configs.rpi_config import RPiDAC

from drivers.can_driver import CanDriver

MESSAGE_DELAY = 2

log = logging.getLogger(__name__)

"""Values for current and power are chosen arbitrarily to satisfy the power constraint,
(current * voltage / 1000) ?= MAX_POWER. If you want to sample them u.a.r you need to solve
the problem of uniformly sampling from a convex polygon. Any algorithm to do this would
likely add a numpy dependency. If you're interested, look into the hit-and-run algo."""


def _send_can_msgs(rpm: int, current: int, voltage: int, bus: CanDriver, db: Database) -> None:
    """Send rpm, current, and voltage can msgs"""
    motor_pos_data = {"D2_Motor_Speed": rpm}
    data = populate_signals(CANId.M165_Motor_Position_Info, motor_pos_data, db)
    bus.write_periodic(CANId.M165_Motor_Position_Info, data, MESSAGE_DELAY)

    current_info_data = {"D4_DC_Bus_Current": current}
    data = populate_signals(CANId.M166_Current_Info, current_info_data, db)
    bus.write_periodic(CANId.M166_Current_Info, data, MESSAGE_DELAY)

    voltage_info_data = {"D1_DC_Bus_Voltage": voltage}
    data = populate_signals(CANId.M167_VOLTAGE_INFO, voltage_info_data, db)
    bus.write_periodic(CANId.M167_VOLTAGE_INFO, data, MESSAGE_DELAY)

    log.info(f"Sent rpm value {rpm}")
    log.info(f"Sent current value {current}")
    log.info(f"Send voltage value {voltage}")


@pytest.mark.wait(5, "Torque Limiter Logic")
def test_torque_limiter_on(can_bus: CanDriver, vehicle_db: Database) -> None:
    """Test whether max torque gets reduced to correct value
    when we satisfy (current * voltage / 1000) > MAX_POWER"""

    # send can necessary can msgs once at least every 5s
    rpm = random.randint(3000, 6000)
    current = 158
    voltage = 500
    _send_can_msgs(rpm, current, voltage, can_bus, vehicle_db)

    apps_percent = random.randint(0, 100) / 100.0

    apps_bits = int(
        (APPS_LEFT_MAX_BIT - (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)) * apps_percent
    ) + (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)

    torque_percent = (apps_bits - (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)) / (
        APPS_LEFT_MAX_BIT - (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)
    )

    RPiDAC.APPSLeft.set_value(int(apps_bits))
    log.info(f"Set apps percent to {apps_percent * 100}%")
    log.info(f"Set APPSLeft to {apps_bits}")
    log.info(f"Set torque percent to {torque_percent * 100}%")

    time.sleep(0.1)

    max_torque = (MAX_POWER * EFFICIENCY_FACTOR * 9550) / rpm
    log.info(f"Max Torque: {max_torque}")

    torque_nm = torque_percent * max_torque
    log.info(f"Expected torque is {torque_nm}")

    result = can_bus.read_value(
        CANId.M192_COMMAND_MESSAGE,
        "Torque_Command",
        torque_nm,
        vehicle_db,
        timeout_s=2,
        tolerance=5,
    )
    assert result.is_ok(), result.value


def test_torque_limiter_negative(can_bus: CanDriver, vehicle_db: Database) -> None:
    """Test whether max torque gets set to 0 when we supply negative RPMs
    and satisfy (current * voltage / 1000) > MAX_POWER. We don't need to wait
    here because torque should get set to 0 regardless of calculated torque."""

    # send can necessary can msgs once at least every 5s
    rpm = -1 * random.randint(3000, 6000)
    current = 158
    voltage = 500
    _send_can_msgs(rpm, current, voltage, can_bus, vehicle_db)

    apps_percent = random.randint(0, 100) / 100.0

    apps_bits = int(
        (APPS_LEFT_MAX_BIT - (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)) * apps_percent
    ) + (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)

    RPiDAC.APPSLeft.set_value(int(apps_bits))
    log.info(f"Set apps percent to {apps_percent * 100}%")
    log.info(f"Set APPSLeft to {apps_bits}")

    time.sleep(0.1)

    torque_nm = 0
    log.info(f"Expected torque is {torque_nm}")
    result = can_bus.read_value(
        CANId.M192_COMMAND_MESSAGE,
        "Torque_Command",
        torque_nm,
        vehicle_db,
        timeout_s=2,
        tolerance=5,
    )
    assert result.is_ok(), result.value


@pytest.mark.wait(8, "Ensure we don't reuse static `powerTimer` variable from last test")
def test_torque_limiter_normal(can_bus: CanDriver, vehicle_db: Database) -> None:
    """Max torque returns to MAX_TORQUE when we satisfy
    (current * voltage / 1000) <= MAX_POWER"""

    # send can necessary can msgs once at least every 5s
    rpm = random.randint(3000, 6000)
    current = 100
    voltage = 400
    _send_can_msgs(rpm, current, voltage, can_bus, vehicle_db)

    apps_percent = random.randint(0, 100) / 100.0

    apps_bits = int(
        (APPS_LEFT_MAX_BIT - (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)) * apps_percent
    ) + (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)

    torque_percent = (apps_bits - (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)) / (
        APPS_LEFT_MAX_BIT - (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)
    )

    RPiDAC.APPSLeft.set_value(int(apps_bits))
    log.info(f"Set apps percent to {apps_percent * 100}%")
    log.info(f"Set APPSLeft to {apps_bits}")
    log.info(f"Set torque percent to {torque_percent * 100}%")

    torque_nm = int(torque_percent * MAX_TORQUE)
    log.info(f"Expected torque is {torque_nm}")

    time.sleep(0.1)

    result = can_bus.read_value(
        CANId.M192_COMMAND_MESSAGE,
        "Torque_Command",
        torque_nm,
        vehicle_db,
        timeout_s=2,
        tolerance=5,
    )
    assert result.is_ok(), result.value


def test_torque_limiter_over_max(can_bus: CanDriver, vehicle_db: Database) -> None:
    """Max torque should never exceed MAX_TORQUE when we satisfy
    torque = (MAX_POWER * EFFICIENCY_FACTOR * 9550 / rpm) > MAX_TORQUE.
    Again, we don't need to wait here because we want to set the torque."""

    # send can necessary can msgs once at least every 5s
    rpm = random.randint(2000, 2500)
    current = 158
    voltage = 500
    _send_can_msgs(rpm, current, voltage, can_bus, vehicle_db)

    apps_percent = random.randint(0, 100) / 100.0

    apps_bits = int(
        (APPS_LEFT_MAX_BIT - (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)) * apps_percent
    ) + (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)

    torque_percent = (apps_bits - (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)) / (
        APPS_LEFT_MAX_BIT - (APPS_LEFT_MIN_BIT + SOFTWARE_PEDAL_OFFSET_BIT_LEFT)
    )

    RPiDAC.APPSLeft.set_value(int(apps_bits))
    log.info(f"Set apps percent to {apps_percent * 100}%")
    log.info(f"Set APPSLeft to {apps_bits}")
    log.info(f"Set torque percent to {torque_percent * 100}%")

    time.sleep(0.1)

    torque_nm = int(torque_percent * MAX_TORQUE)
    log.info(f"Expected torque is {torque_nm}")

    result = can_bus.read_value(
        CANId.M192_COMMAND_MESSAGE,
        "Torque_Command",
        torque_nm,
        vehicle_db,
        timeout_s=2,
        tolerance=5,
    )
    assert result.is_ok(), result.value

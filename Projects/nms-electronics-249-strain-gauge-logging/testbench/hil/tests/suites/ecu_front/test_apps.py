import logging

from cantools.database import Database
from configs.apps_config import *
from configs.rpi_config import RPiDAC

from drivers.can_driver import CanDriver

log = logging.getLogger(__name__)


def test_apps_left_over_can(can_bus: CanDriver, vehicle_db: Database) -> None:
    """
    Test if ecu-front correctly reads voltage form the APPS left sensor correctly,
    and if it sends the correct percentage over CAN
    """
    log.info("Testing APPS Left (APPS1) signal over CAN")
    apps_over_can_test(
        can_bus, vehicle_db, RPiDAC.APPSLeft, APPS_LEFT_MIN_BIT, APPS_LEFT_MAX_BIT, "APPS1"
    )


def test_apps_right_over_can(can_bus: CanDriver, vehicle_db: Database) -> None:
    """
    Test if ecu-front correctly reads voltage form the APPS right sensor correctly,
    and if it sends the correct percentage over CAN
    """
    log.info("Testing APPS Right (APPS2) signal over CAN")
    apps_over_can_test(
        can_bus, vehicle_db, RPiDAC.APPSRight, APPS_RIGHT_MIN_BIT, APPS_RIGHT_MAX_BIT, "APPS2"
    )

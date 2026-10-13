import logging
from collections.abc import Generator
from pathlib import Path

import cantools.database as candb
import pytest
from cantools.database import Database

from drivers.can_driver import CanDriver, CanInterface

log = logging.getLogger(__name__)

# Get the directory containing this file
FIXTURES_DIR = Path(__file__).parent
# Resolve the path relative to this file's location
VEHICLE_DBC = str(FIXTURES_DIR / "../../../../shared/dbc_to_c/dbc_files/VEHICLE.dbc")
CHARGER_DBC = str(FIXTURES_DIR / "../../../../shared/dbc_to_c/dbc_files/CHARGER.dbc")


@pytest.fixture(scope="session")
def can_bus() -> Generator[CanDriver]:
    """Physical CAN Bus wrapper around the `python-can` library"""
    iface = CanInterface(virtual=False)
    driver = CanDriver(iface)
    driver.connect()
    log.info("Connected to CAN bus")
    yield driver
    driver.disconnect()
    log.info("Disconnected to CAN bus")


@pytest.fixture(autouse=True)
def stop_periodic_sends(can_bus: CanDriver) -> Generator[None]:
    """Stop sending all periodic CAN msgs after every test"""
    yield None
    can_bus.bus.stop_all_periodic_tasks()
    log.info("Stopping all periodic tasks")


@pytest.fixture(scope="session")
def vcan_bus() -> Generator[Database]:
    """Virtual CAN Bus wrapper around the `python-can` library"""
    iface = CanInterface(virtual=True)
    driver = CanDriver(iface)
    driver.connect()
    log.info("Connected to VCAN bus")
    yield driver
    driver.disconnect()
    log.info("Disconnected to VCAN bus")


@pytest.fixture(scope="session")
def vehicle_db() -> Database:
    """Load the vehicle's CAN file"""
    # we know it's a dbc file so ignore the type warning
    log.info(f"Loading {VEHICLE_DBC}")
    return candb.load_file(VEHICLE_DBC)  # ty: ignore[invalid-return-type]


@pytest.fixture(scope="session")
def charger_db() -> Database:
    """Load the charger's CAN file"""
    # we know it's a dbc file so ignore the type warning
    log.info(f"Loading {CHARGER_DBC}")
    return candb.load_file(CHARGER_DBC)  # ty: ignore[invalid-return-type]

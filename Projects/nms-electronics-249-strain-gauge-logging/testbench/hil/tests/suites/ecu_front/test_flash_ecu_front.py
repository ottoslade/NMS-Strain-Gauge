import pytest
from configs.ecu_config import ECU
from plugins.flash import build_ecu, flash_ecu

from drivers.can_driver import CanDriver


def test_flash_ecu_front(request: pytest.FixtureRequest, can_bus: CanDriver) -> None:
    """
    Test that builds and flashes ECU FRONT firmware
    Sets a session flag on success to manage the execution of the other tests
    in the package.
    """
    # build and flash firmware
    assert build_ecu(ECU.FRONT)
    assert flash_ecu(ECU.FRONT, can_bus)

    # mark session as flashed so downstream tests know setup succeeded
    setattr(request.session, "ecu_front_flashed", True)

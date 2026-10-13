import logging

import pytest

log = logging.getLogger(__name__)


@pytest.fixture(autouse=True)
def require_firmware_flashing(request: pytest.FixtureRequest) -> None:
    """
    Run before every test in this package. If flashing hasn't occurred
    (session flag not set) skip the test to avoid false positives/negatives.
    Allow the flashing test itself to run (it sets the flag).
    """
    nodeid = (
        request.node.nodeid
    )  # e.g. 'tests/suites/ecu_front/test_flash_ecu_front.py::test_flash_ecu_front'
    # If this node is the flashing test, allow it
    if "test_flash_ecu_front" in nodeid:
        return

    # CLI override
    if request.config.getoption("--skip-flash"):
        return

    flashed = getattr(request.session, "ecu_front_flashed", False)
    if not flashed:
        pytest.skip("ECU FRONT firmware not flashed; skipping to avoid false positives/negatives.")

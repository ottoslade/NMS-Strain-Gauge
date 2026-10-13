"""Use this to import fixtures. Do `uv run pytest --fixtures` to see all available.
Ignore the warning about the RPi module if not running on the Pi.
"""

import logging

import pytest

log = logging.getLogger(__name__)

pytest_plugins = [
    "plugins.can",
    "plugins.flash",
    "plugins.test_ordering",
    "plugins.rpi",
    "plugins.wait",
]


def pytest_addoption(parser: pytest.Parser) -> None:
    """
    Function to add options to the pytest test set
    """

    parser.addoption(
        "--skip-flash",
        action="store_true",
        default=False,
        help="Allow running tests without requiring the flashing step",
    )

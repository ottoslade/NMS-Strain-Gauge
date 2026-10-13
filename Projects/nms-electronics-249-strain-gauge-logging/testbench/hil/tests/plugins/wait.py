import logging
import time

import pytest

log = logging.getLogger(__name__)


@pytest.hookimpl
def pytest_configure(config: pytest.Config) -> None:
    """Hook that adds the `wait(seconds, reason) marker"""
    config.addinivalue_line(
        "markers",
        "wait(seconds, reason): wait `seconds` seconds before running test because of `reason`",
    )


@pytest.hookimpl
def pytest_runtest_setup(item: pytest.Item) -> None:
    """Hook that implements the `wait(seconds, reason)` marker"""
    wait_marker = item.get_closest_marker("wait")
    if wait_marker is not None:
        seconds = wait_marker.args[0]
        if seconds is None:
            raise ValueError("`seconds` parameter must be passed to `wait()` marker")
        if not isinstance(seconds, int | float):
            raise TypeError(
                f"`seconds` parameter must be an int or float, was {seconds} a(n) {type(seconds)}"
            )
        if seconds < 0:
            raise ValueError(f"`seconds` parameter cannot be negative, was {seconds}")

        reason = wait_marker.args[1]
        if reason is None:
            raise ValueError("`reason` parameter must be passed to `wait()` marker")
        if not isinstance(reason, str):
            raise TypeError(f"`reason` parameter must be a str, was {reason} a(n) {type(reason)}")

        log.info(f"Waiting {seconds} seconds. Reason: {reason}")

        time.sleep(seconds)

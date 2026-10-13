import json
import logging
from collections.abc import Generator
from pathlib import Path
from typing import Any

import pytest

log = logging.getLogger(__name__)


class TestSetDirectory(pytest.Directory):
    def collect(self) -> Generator[Any]:
        """The standard pytest behavior is to loop over all `test_*.py` files and
        call `pytest_collect_file` on each file. This collector instead reads
        the `test_set.json` file and only calls `pytest_collect_file` for the
        files defined there."""
        test_set_path = self.path / "test_set.json"
        test_set = json.loads(test_set_path.read_text(encoding="utf-8"))
        ihook = self.ihook
        log.info(f"Ordering tests in {test_set_path}")
        for file in test_set["order"]:
            yield from ihook.pytest_collect_file(file_path=self.path / file, parent=self)


@pytest.hookimpl
def pytest_collect_directory(path: Path, parent: pytest.Collector) -> TestSetDirectory | None:
    """Use our custom collector for directories containing a `test_set.json` file."""
    if path.joinpath("test_set.json").is_file():
        return TestSetDirectory.from_parent(parent=parent, path=path)
    # Otherwise fallback to the standard behavior.
    return None

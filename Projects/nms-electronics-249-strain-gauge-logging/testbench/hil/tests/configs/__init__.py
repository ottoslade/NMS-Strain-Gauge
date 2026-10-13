from enum import Enum
from typing import Any


class ObjectEnum(Enum):
    """Enum that delegates attribute access to its value.
    Return `Any` so that the return value is callable."""

    def __getattribute__(self, name: str) -> Any:  # noqa: ANN401
        # Handle special names directly
        if name.startswith("_"):
            return super().__getattribute__(name)

        # For normal attributes, try the enum member first
        try:
            return super().__getattribute__(name)
        except AttributeError:
            # If not found, delegate to the wrapped value
            value = super().__getattribute__("_value_")
            return getattr(value, name)

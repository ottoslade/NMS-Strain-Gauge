import time
from typing import Literal

from RPi import GPIO

UNAVAILABLE_PINS = (1, 2, 4, 6, 9, 14, 17, 20, 25, 30, 34, 39)


class GPIODriver:
    """Generic GPIO Driver"""

    def __init__(
        self,
        pin: int,
        pin_config: Literal[0, 1] = GPIO.LOW,
        resistor_config: int = -1,
    ) -> None:
        if pin in UNAVAILABLE_PINS:
            raise ValueError(
                f"Pin:{pin} is not an available GPIO pin."
                f"Choose a pin number 1-40 not in {UNAVAILABLE_PINS}"
            )
        self.pin = pin
        self.pin_config = pin_config
        self.resistor_config = resistor_config

    def init(self) -> None:
        """Setup GPIO after board has been setup"""
        if self.resistor_config == -1:
            GPIO.setup(self.pin, self.pin_config)
        else:
            GPIO.setup(self.pin, self.pin_config, pull_up_down=self.resistor_config)

    def write(self, pin_direction: Literal[0, 1]) -> None:
        """Write to pin"""
        if self.pin_config is not GPIO.OUT:
            raise ValueError(f"Config not `GPIO.OUT`. self.pin_config: {self.pin_config}")
        GPIO.output(self.pin, pin_direction)

    def read(self) -> bool:
        """Read from pin"""
        if self.pin_config is not GPIO.IN:
            raise ValueError(f"Config not `GPIO.IN`. self.pin_config: {self.pin_config}")
        return GPIO.input(self.pin)

    def press_button(self) -> None:
        """Simulate button press"""
        self.write(GPIO.HIGH)
        time.sleep(0.1)
        self.write(GPIO.LOW)

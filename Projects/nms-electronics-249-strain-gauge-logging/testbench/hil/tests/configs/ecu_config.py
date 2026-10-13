from RPi import GPIO

from configs import ObjectEnum
from drivers.gpio_driver import GPIODriver


class ResetGPIO(ObjectEnum):
    """
    Class for all reset pins connected to ECUs because these need
    to be handled differently than regular GPIOs as they need to
    be active high. Only pins that are high by default on the
    Pi should be selected.
    """

    ECUFrontReset: GPIODriver = GPIODriver(7, GPIO.OUT)


class ECUConfig:
    """Class to define particular properties about ECUs"""

    def __init__(self, name: str, reset_gpio: GPIODriver) -> None:
        self.ecu_name = name
        self.reset_gpio = reset_gpio


class ECU(ObjectEnum):
    FRONT: ECUConfig = ECUConfig("ecu-front", ResetGPIO.ECUFrontReset)

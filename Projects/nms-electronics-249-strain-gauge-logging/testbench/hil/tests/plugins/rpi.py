import logging
import time
from collections.abc import Generator

import pytest
from configs.apps_config import *
from configs.brake_config import *
from configs.ecu_config import ResetGPIO
from configs.rpi_config import RPiDAC, RPiGPIO
from RPi import GPIO

from drivers.i2c_driver import I2CDriver

log = logging.getLogger(__name__)


@pytest.fixture(scope="session")
def i2c_bus() -> Generator[I2CDriver]:
    """i2c Bus for device /dev/i2c-1"""
    bus = I2CDriver(1)
    log.info("Initialized I2C Bus")
    yield bus
    bus.disconnect()
    log.info("Disconnected I2C Bus")


@pytest.fixture(scope="session", autouse=True)
def rpi_config(i2c_bus: I2CDriver) -> Generator[None]:
    """Configure RPi once per session and clean up GPIO at end of session"""

    GPIO.setmode(GPIO.BOARD)
    for pin in RPiGPIO:
        pin.init()
    log.info("Initialized all RPiGPIOs")

    for pin in ResetGPIO:
        pin.init()
    log.info("Initialized all ResetGPIOs")

    for dac in RPiDAC:
        dac.init(i2c_bus)

        # try to set the value of the DAC to 0 if it connected
        try:
            dac.set_value(0)
        except OSError:
            # deinitialize the DAC object if the DAC is not
            # physically connected
            dac.deinit()
    log.info("Initialized all connected DACs")

    yield None

    # reset all the DACs to output 0.0V and deinitialize them
    for dac in RPiDAC:
        dac.reset()
        dac.deinit()
    log.info("Reset and disconnected all DACs")

    # cleanup GPIO pins
    GPIO.cleanup()
    log.info("Cleaned up GPIOs")


@pytest.fixture(autouse=True)
def reset_hardware() -> Generator[None]:
    """Reset RPi hadware peripherals after every test"""

    RPiDAC.BrakesFront.set_value(F_BRAKE_MIN_BIT)
    RPiDAC.BrakesRear.set_value(R_BRAKE_MIN_BIT)
    RPiDAC.APPSLeft.set_value(APPS_LEFT_MIN_BIT)
    RPiDAC.APPSRight.set_value(APPS_RIGHT_MIN_BIT)
    log.info("Set all brake and APPS pins to min values.")

    yield None

    for pin in RPiGPIO:
        pin.write(GPIO.LOW)
    log.info("Set all GPIO pins to LOW")

    # reset all the DACs to output 0.0V
    for dac in RPiDAC:
        dac.reset()
    log.info("Set all DAC pins to 0.0V")

    time.sleep(0.5)

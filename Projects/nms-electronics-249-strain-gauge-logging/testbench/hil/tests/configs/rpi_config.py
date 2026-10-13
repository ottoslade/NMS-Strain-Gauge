from RPi import GPIO

from configs import ObjectEnum
from drivers.dac_driver import DACDriver
from drivers.gpio_driver import GPIODriver


class RPiGPIO(ObjectEnum):
    EEPROMWriteBrakes: GPIODriver = GPIODriver(10, GPIO.OUT)
    EEPROMWriteAPPS: GPIODriver = GPIODriver(11, GPIO.OUT)
    RTD: GPIODriver = GPIODriver(13, GPIO.OUT)


class RPiDAC(ObjectEnum):
    APPSLeft: DACDriver = DACDriver(0, 0x60)
    APPSRight: DACDriver = DACDriver(0, 0x61)
    BrakesFront: DACDriver = DACDriver(1, 0x60)
    BrakesRear: DACDriver = DACDriver(1, 0x61)

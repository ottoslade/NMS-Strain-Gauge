from typing import Literal, TypeAlias

from drivers.i2c_driver import I2CDriver
from drivers.mcp4725_driver import MCP4725
from drivers.tca9548a_driver import TCA9548A

Channel: TypeAlias = Literal[0, 1, 2, 3, 4, 5, 6, 7]


class DACDriver:
    """
    Driver for Digital-to-Analog Converters to simulate sensors
    and analog signals in the vehicle
    """

    def __init__(self, i2c_mux_channel: Channel, i2c_addr: int) -> None:
        self.addr = i2c_addr
        self.channel = i2c_mux_channel
        self.initialized = False

    def init(self, i2c: I2CDriver) -> None:
        self.mcp4725 = MCP4725(i2c=i2c, address=self.addr)
        self.tca9548a = TCA9548A(i2c=i2c)
        self.initialized = True

    def set_value(self, value: int) -> None:
        if not self.initialized:
            raise RuntimeError("DAC must be initialized with .init() before use.")

        self.tca9548a[self.channel].select_channel()
        self.mcp4725.raw_value = value
        self.tca9548a[self.channel].deselect_channel()

    def get_value(self) -> int:
        if not self.initialized:
            raise RuntimeError("DAC must be initialized with .init() before use.")

        self.tca9548a[self.channel].select_channel()
        ret_value = self.mcp4725.raw_value
        self.tca9548a[self.channel].deselect_channel()
        return ret_value

    def reset(self) -> None:
        if self.initialized:
            self.set_value(0)

    def deinit(self) -> None:
        self.initialized = False

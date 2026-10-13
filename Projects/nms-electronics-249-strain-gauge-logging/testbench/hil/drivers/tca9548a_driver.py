# SPDX-FileCopyrightText: 2018 Carter Nelson for Adafruit Industries
#
# SPDX-License-Identifier: MIT

"""
`adafruit_tca9548a`
====================================================

CircuitPython driver for the TCA9548A I2C Multiplexer.

* Author(s): Carter Nelson

Implementation Notes
--------------------

**Hardware:**

* `TCA9548A I2C Multiplexer
  <https://www.adafruit.com/product/2717>`_ (Product ID: 2717)


**Software and Dependencies:**

* Smbus3 Library:
  https://pypi.org/project/smbus3/
"""

"""
This Adafruit driver was modified to suit NMS's needs
"""

from typing import Literal

from drivers.i2c_driver import I2CDriver

_DEFAULT_ADDRESS = 0x70

__version__ = "0.0.0+auto.0"
__repo__ = "https://github.com/adafruit/Adafruit_CircuitPython_TCA9548A.git"


class TCA9548AChannel:
    """Helper class to represent an output channel on the TCA9548A and take care
    of the necessary I2C commands for channel switching. This class needs to
    behave like an I2CDevice."""

    def __init__(self, tca: "TCA9548A", channel: int) -> None:
        self.tca = tca
        self.channel_switch = bytearray([1 << channel])

    def select_channel(self) -> None:
        """Select a channel from the I2C Mux"""
        self.tca.i2c.write(self.tca.address, bytes(self.channel_switch))

    def deselect_channel(self) -> None:
        """Deselect a channel from the I2C Mux"""
        self.tca.i2c.write(self.tca.address, b"\x00")


class TCA9548A:
    """Class which provides interface to TCA9548A I2C multiplexer."""

    def __init__(self, i2c: I2CDriver, address: int = _DEFAULT_ADDRESS) -> None:
        self.i2c = i2c
        self.address = address
        self.channels = [None] * 8

    def __len__(self) -> Literal[8]:
        return 8

    def __getitem__(self, key: Literal[0, 1, 2, 3, 4, 5, 6, 7]) -> TCA9548AChannel:
        if not 0 <= key <= 7:
            raise IndexError("Channel must be an integer in the range: 0-7.")
        if self.channels[key] is None:
            self.channels[key] = TCA9548AChannel(self, key)
        # we know this won't be none so ignore the type warning
        return self.channels[key]  # ty: ignore[invalid-return-type]

    def read_selected_channel(self) -> bytes:
        return self.i2c.read(self.address, 1)

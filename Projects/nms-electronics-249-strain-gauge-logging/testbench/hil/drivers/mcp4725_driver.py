# SPDX-FileCopyrightText: 2017 Tony DiCola for Adafruit Industries
#
# SPDX-License-Identifier: MIT

"""
`adafruit_mcp4725` - MCP4725 digital to analog converter
========================================================

CircuitPython module for the MCP4725 digital to analog converter.  See
examples/mcp4725_simpletest.py for a demo of the usage.

* Author(s): Tony DiCola, Carter Nelson

Implementation Notes
--------------------

**Hardware:**

* Adafruit `MCP4725 Breakout Board - 12-Bit DAC w/I2C Interface
  <https://www.adafruit.com/product/935>`_ (Product ID: 935)
"""

"""
This Adafruit driver was modified to suit NMS's needs
"""

import time

from drivers.i2c_driver import I2CDriver

__version__ = "0.0.0+auto.0"
__repo__ = "https://github.com/adafruit/Adafruit_CircuitPython_MCP4725.git"


# Internal constants:
_MCP4725_DEFAULT_ADDRESS = 0x60
_MCP4725_WRITE_FAST_MODE = 0b00000000
_MCP4725_WRITE_DAC_EEPROM = 0b01100000


class MCP4725:
    """
    MCP4725 12-bit digital to analog converter.  This class has a similar
    interface as the CircuitPython AnalogOut class and can be used in place
    of that module.

    :param ~busio.I2C i2c: The I2C bus.
    :param int address: The address of the device if set differently from the default.
    """

    # Global buffer to prevent allocations and heap fragmentation.
    # Note this is not thread-safe or re-entrant by design!
    _BUFFER = bytearray(3)

    def __init__(self, i2c: I2CDriver, address: int = _MCP4725_DEFAULT_ADDRESS) -> None:
        # This device doesn't use registers and instead just accepts a single
        # command string over I2C.  As a result we don't use bus device or
        # other abstractions and just talk raw I2C protocol.
        self._i2c = i2c
        self._address = address

    def _write_fast_mode(self, val: int) -> None:
        # Perform a 'fast mode' write to update the DAC value.
        # Will not enter power down, update EEPROM, or any other state beyond
        # the 12-bit DAC value.
        assert 0 <= val <= 4095
        # Build bytes to send to device with updated value.
        val &= 0xFFF
        self._BUFFER[0] = _MCP4725_WRITE_FAST_MODE | (val >> 8)
        self._BUFFER[1] = val & 0xFF
        with self._i2c as i2c:
            i2c.write(self._address, self._BUFFER)

    def _read(self) -> int:
        # Perform a read of the DAC value.  Returns the 12-bit value.
        # Read 3 bytes from device.
        with self._i2c as i2c:
            self._BUFFER = bytearray(i2c.read(self._address, 3))

        # Grab the DAC value from last two bytes.
        dac_high = self._BUFFER[1]
        dac_low = self._BUFFER[2] >> 4
        # Reconstruct 12-bit value and return it.
        return ((dac_high << 4) | dac_low) & 0xFFF

    @property
    def value(self) -> int:
        """
        The DAC value as a 16-bit unsigned value compatible with the
        :py:class:`~analogio.AnalogOut` class.

        Note that the MCP4725 is still just a 12-bit device so quantization will occur.  If you'd
        like to instead deal with the raw 12-bit value use the ``raw_value`` property, or the
        ``normalized_value`` property to deal with a 0...1 float value.
        """
        raw_value = self._read()
        # Scale up to 16-bit range.
        return raw_value << 4

    @value.setter
    def value(self, val: int) -> None:
        assert 0 <= val <= 65535
        # Scale from 16-bit to 12-bit value (quantization errors will occur!).
        raw_value = val >> 4
        self._write_fast_mode(raw_value)

    @property
    def raw_value(self) -> int:
        """The DAC value as a 12-bit unsigned value.  This is the the true resolution of the DAC
        and will never peform scaling or run into quantization error.
        """
        return self._read()

    @raw_value.setter
    def raw_value(self, val: int) -> None:
        self._write_fast_mode(val)

    @property
    def normalized_value(self) -> float:
        """The DAC value as a floating point number in the range 0.0 to 1.0."""
        return self._read() / 4095.0

    @normalized_value.setter
    def normalized_value(self, val: float) -> None:
        assert 0.0 <= val <= 1.0
        raw_value = int(val * 4095.0)
        self._write_fast_mode(raw_value)

    def save_to_eeprom(self) -> None:
        """Store the current DAC value in EEPROM."""
        # get it and write it
        current_value = self._read()
        self._BUFFER[0] = _MCP4725_WRITE_DAC_EEPROM
        self._BUFFER[1] = (current_value >> 4) & 0xFF
        self._BUFFER[2] = (current_value << 4) & 0xFF
        with self._i2c as i2c:
            i2c.write(self._address, self._BUFFER)
        # wait for EEPROM write to complete
        self._BUFFER[0] = 0x00
        while not self._BUFFER[0] & 0x80:
            time.sleep(0.05)
            with self._i2c as i2c:
                # read a single byte
                self._BUFFER = i2c.read(self._address, 1)

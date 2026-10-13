from smbus3 import SMBus

_WRITE_FAST_MODE = 0x0000


class I2CDriver:
    """
    Driver for I2C Bus

    `i2c = I2CDriver(1)` sets up an i2c bus with device `/dev/i2c-1`
    """

    def __init__(self, bus_num: int) -> None:
        self.bus: SMBus = SMBus(f"/dev/i2c-{bus_num}")

    # Context manager support so `with self._i2c as i2c:` works:
    def __enter__(self):  # noqa: ANN204
        # Return the driver object to the with-block
        return self

    def __exit__(self, exc_type, exc, tb) -> bool:  # noqa: ANN001
        # Keep bus open for reuse. Return False to propagate exceptions.
        # If you want to close the bus when leaving the context, call self.disconnect()
        return False

    def read(self, addr: int, length: int) -> bytes:
        """
        Read length bytes from the I2C device with address addr
        """
        i2c_msg = self.bus.i2c_rd(addr, length)
        return bytes(i2c_msg)

    def write(self, addr: int, data: bytes) -> None:
        """
        Write data to the I2C device with address addr
        """
        self.bus.i2c_wr(addr, data)

    def disconnect(self) -> None:
        """
        Shutdown I2C bus
        """
        self.bus.close()
        self.bus = None  # ty: ignore[invalid-assignment]

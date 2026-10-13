import glob
import subprocess
import time
from dataclasses import dataclass

import can
from cantools.database import Database


def run_cmd(cmd: str) -> None:
    """Run cmd using subprocess"""
    subprocess.run(cmd, shell=True)


@dataclass
class Ok:
    value: can.Message | bool = True

    def is_ok(self) -> bool:
        return True

    def unwrap(self) -> can.Message | bool:
        return self.value


@dataclass
class Err:
    value: str

    def is_ok(self) -> bool:
        return False

    def unwrap(self) -> None:
        raise ValueError("Cannot call Unwrap on an 'Err' instance")


class CanInterface:
    """Manages low-level CAN connection"""

    def __init__(self, virtual: bool = False) -> None:
        self.virtual = virtual

    """ PRIVATE """

    def _is_canable_slcan(self) -> bool:
        """Check if ttyACM device exists (indicates slcan mode)"""
        acm_devices = glob.glob("/dev/ttyACM*")
        return bool(acm_devices)

    def _cleanup_slcan_interface(self) -> None:
        """Clean up slcan interface and daemon"""
        cleanup_commands = [
            "sudo ifconfig can0 down 2> /dev/null || true",
            "sudo killall slcand 2> /dev/null || true",
            "sudo ip link delete can0 2> /dev/null || true",
        ]

        for cmd in cleanup_commands:
            run_cmd(cmd)

    """ PRIVATE """

    def setup(self) -> can.interface.BusABC:
        if self.virtual:
            # create virtual socket
            # see https://python-can.readthedocs.io/en/stable/interfaces/virtual.html

            run_cmd("sudo modprobe vcan")
            run_cmd("sudo ip link add dev vcan0 type vcan")
            run_cmd("sudo ip link set up vcan0")

            return can.interface.Bus(
                channel="vcan0",
                interface="virtual",
                receive_own_messages=True,
            )
        else:
            # create socket for physical bus
            # see https://python-can.readthedocs.io/en/stable/interfaces/socketcan.html

            if self._is_canable_slcan():
                # Clean up any existing slcan interfaces
                run_cmd("sudo killall slcand 2> /dev/null || true")
                run_cmd("sudo ip link delete can0 2> /dev/null || true")

                run_cmd("sudo modprobe slcan")
                run_cmd("sudo slcand -o -c -s6 /dev/ttyACM0 can0")

            run_cmd("sudo ifconfig can0 down")
            run_cmd("sudo ip link set can0 type can bitrate 500000")
            run_cmd("sudo ip link set can0 qlen 1000")
            run_cmd("sudo ifconfig can0 up")

            return can.interface.Bus(
                interface="socketcan",
                channel="can0",
            )

    def cleanup(self) -> None:
        self._cleanup_slcan_interface()


class CanDriver:
    """
    CAN bus interface
    """

    def __init__(self, interface: CanInterface) -> None:
        self.bus: can.interface.BusABC
        self.iface: CanInterface = interface

    def connect(self) -> None:
        """Initialize and connect to the bus"""
        self.bus = self.iface.setup()

    def read(self, delay_s: float) -> can.Message | None:
        """Reads the bus and returns the first message received."""
        if self.bus is None:
            raise can.CanInitializationError("Connection not instantiated")
        message = self.bus.recv(delay_s)
        return message

    def read_id(self, id: int, timeout_s: float = 20) -> Ok | Err:
        """Reads the bus and returns the message with given id
        or times out after specified duration."""
        if self.bus is None:
            raise can.CanInitializationError("Connection not instantiated")
        start = time.time()
        buffered_reader = can.BufferedReader()
        with can.Notifier(self.bus, listeners=[buffered_reader]):
            start = time.time()
            while time.time() - start < timeout_s:
                message = buffered_reader.get_message()
                if message != None and message.timestamp < start:
                    continue
                if message != None and message.arbitration_id == id:
                    return Ok(message)
        return Err(f"Message {hex(id)} not received")

    def read_value(
        self,
        id: int,
        signal: str,
        expected: int | float | str,
        db: Database,
        timeout_s: float = 5,
        tolerance: int | float = 0,
    ) -> Ok | Err:
        """Reads the bus and returns Ok() if a message with given `id` has `expected`
        value in the specified `signal` is received within the timeout.
        If the message is never received, returns Err(). If the message is
        received but with the wrong value(s), returns an Err() with the value(s)."""
        message = None
        buffered_reader = can.BufferedReader()
        rec_vals = []
        with can.Notifier(self.bus, listeners=[buffered_reader]):
            start = time.time()
            while time.time() - start < timeout_s:
                message = buffered_reader.get_message()
                if message != None and message.timestamp < start:
                    continue
                if message != None and message.arbitration_id == id:
                    data = bytes(message.data)
                    # we know this returns a dict because of our dbc file, so ignore the error
                    decode_val = db.decode_message(id, data)[signal]  # ty: ignore[invalid-argument-type]
                    if decode_val == expected or (
                        isinstance(expected, int | float)
                        and isinstance(decode_val, int | float)
                        and (expected - tolerance) <= decode_val <= (expected + tolerance)
                    ):
                        return Ok()
                    elif decode_val not in rec_vals:
                        rec_vals += [decode_val]
        if message == None:
            return Err(f"Message {db.get_message_by_frame_id(id)} not received")
        else:
            return Err(f"{signal} value not {expected}, value(s) is/are {rec_vals}")

    def read_value_consistent(
        self,
        id: int,
        signal: str,
        expected: int | float | str,
        db: Database,
        timeout_s: float = 5,
        msgs_req: int = 10,
    ) -> Ok | Err:
        """Reads the bus and returns Ok() if a message with given `id` has
        `expected` value in specified `signal` is received `msgs_req` times
        before a timeout. If the message is never received, returns Err().
        If message is received but with the wrong value(s), return and Err() with the value(s)."""
        message = None
        buffered_reader = can.BufferedReader()
        rec_vals = []
        count = 0
        with can.Notifier(self.bus, listeners=[buffered_reader]):
            start = time.time()
            while time.time() - start < timeout_s:
                message = buffered_reader.get_message()
                if message != None and message.timestamp < start:
                    continue
                if message != None and message.arbitration_id == id:
                    data = bytes(message.data)
                    # we know this returns a dict because of our dbc file, so ignore the error
                    decode_val = db.decode_message(id, data)[signal]  # ty: ignore[invalid-argument-type]
                    if decode_val == expected and count != (msgs_req - 1):
                        start = time.time()
                        count += 1
                    elif decode_val == expected and count == (msgs_req - 1):
                        return Ok()
                    elif decode_val != expected and count != 0:
                        return Err(
                            f"{signal} value not {expected} after {count} messages,"
                            f"value was {decode_val}",
                        )
                    elif decode_val != expected and decode_val not in rec_vals:
                        rec_vals += [decode_val]
        if message == None:
            return Err(f"Message {db.get_message_by_frame_id(id)} not received")
        else:
            return Err(f"{signal} value not {expected}, value(s) is/are {rec_vals}")

    def read_value_time(
        self,
        id: int,
        signal: str,
        expected: int | float | str,
        db: Database,
        time_req: float = 10,
        timeout_s: float = 5,
    ) -> Ok | Err:
        """Reads the bus and returns Ok() if a message with given `id` has
        `expected` value in specified `signal` is received for `time_req` seconds
        before a timeout or a different value. If the message is never received, returns Err().
        If message is received but with the wrong value(s), return an Err() with the value(s)."""
        message = None
        buffered_reader = can.BufferedReader()
        rec_vals = []
        first_rec = None
        with can.Notifier(self.bus, listeners=[buffered_reader]):
            start = time.time()
            while time.time() - start < timeout_s:
                message = buffered_reader.get_message()
                if message != None and message.timestamp < start:
                    continue
                if message != None and message.arbitration_id == id:
                    data = bytes(message.data)
                    # we know this returns a dict because of our dbc file, so ignore the error
                    decode_val = db.decode_message(id, data)[signal]  # ty: ignore[invalid-argument-type]
                    if decode_val == expected and not first_rec:
                        first_rec = time.time()
                    elif (
                        decode_val == expected and first_rec and time.time() - first_rec > time_req
                    ):
                        return Ok()
                    elif decode_val != expected and first_rec:
                        return Err(
                            f"{signal} value not {expected}, {time.time() - first_rec}"
                            f"seconds after receiving first message value was {decode_val}"
                        )
                    elif decode_val not in rec_vals:
                        rec_vals += [decode_val]
        if message == None:
            return Err(f"Message {db.get_message_by_frame_id(id)} not received")
        else:
            return Err(f"{signal} value not {expected}, value(s) is/are {rec_vals}")

    def write(self, id: int, data: list[int]) -> None:
        """Writes specified message to the bus."""
        if self.bus is None:
            raise can.CanInitializationError("Connection not instantiated")
        can_m = can.Message(arbitration_id=id, data=data, is_extended_id=False)
        self.bus.send(msg=can_m)

    def write_periodic(
        self, id: int, data: list[int] | bytearray | bytes, period: float
    ) -> can.CyclicSendTaskABC:
        """Write specified message to bus periodically, returns a periodic task"""
        if self.bus is None:
            raise can.CanInitializationError("Connection not instantiated")
        can_m = can.Message(arbitration_id=id, data=data, is_extended_id=False)
        return self.bus.send_periodic(msgs=can_m, period=period)

    def disconnect(self) -> None:
        """Cleanup and disconnect from bus"""
        if hasattr(self, "bus") and self.bus is not None:
            self.bus.shutdown()
            self.bus = None  # ty: ignore[invalid-assignment]
        self.iface.cleanup()
        self.iface = None  # ty: ignore[invalid-assignment]

import can
import pytest

from drivers.can_driver import CanDriver


@pytest.mark.parametrize("id", [0x123])
@pytest.mark.parametrize("data", [[1, 2, 3]])
def test_can(can_bus: CanDriver, id: int, data: list[int]) -> None:
    """Test sending/receiving over can."""
    can_bus.write(id, data)

    read_msg = can_bus.read(5)

    assert read_msg, "message not received"


@pytest.mark.parametrize("id", [0x123])
@pytest.mark.parametrize("data", [[1, 2, 3]])
def test_can_read_id(can_bus: CanDriver, id: int, data: list[int]) -> None:
    """Test sending/receiving specific message"""
    sent_msg = can.Message(arbitration_id=id, data=data)
    can_bus.write_periodic(id, data, 1)

    fake_id = 0x567
    fake_data = [5, 6, 7]
    can_bus.write_periodic(fake_id, fake_data, 1)

    res = can_bus.read_id(id)
    assert res.is_ok(), res.value

    read_msg = res.unwrap()
    assert isinstance(read_msg, can.Message)
    assert sent_msg.arbitration_id == read_msg.arbitration_id, "Id's don't match"
    assert sent_msg.data == read_msg.data, "Data doesn't match"
    assert sent_msg.timestamp != read_msg.timestamp, "Timestamps match"

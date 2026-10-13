import can

from drivers.can_driver import CanDriver


def test_vcan(vcan_bus: CanDriver) -> None:
    """Test sending/receiving over vcan."""
    id = 0x123
    msg_data = [1, 2, 3]
    vcan_bus.write(id, msg_data)

    msg2 = vcan_bus.read(5)

    assert msg2


def test_vcan_read_id(vcan_bus: CanDriver) -> None:
    """Test sending/receiving specific id."""
    id = 0x123
    msg_data = [1, 2, 3]
    msg1 = can.Message(arbitration_id=id, data=msg_data)
    vcan_bus.write_periodic(id, msg_data, 1)

    fake_id = 0x567
    fake_data = [5, 6, 7]
    vcan_bus.write_periodic(fake_id, fake_data, 1)

    res = vcan_bus.read_id(id, 5)
    assert res.is_ok(), res.value

    msg2 = res.unwrap()

    assert isinstance(msg2, can.Message)
    assert msg1.arbitration_id == msg2.arbitration_id, "Id's don't match"
    assert msg1.data == msg2.data, "Data doesn't match"
    assert msg1.timestamp != msg2.timestamp, "Timestamps match"

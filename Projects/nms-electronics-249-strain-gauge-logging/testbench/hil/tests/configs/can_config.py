from enum import IntEnum

from cantools.database import Database


class CANId(IntEnum):
    M167_VOLTAGE_INFO = 0x0A7
    M192_COMMAND_MESSAGE = 0x0C0
    READY_TO_DRIVE = 0x1C0
    BMS_FAULT = 0x0C3
    BRAKE_LIGHT = 0x210
    APPS = 0x2B0
    BRAKE_PRESSURE = 0x230
    BRAKE_TEMP = 0x280
    STEERING_POT = 0x250
    M165_Motor_Position_Info = 0x0A5
    M166_Current_Info = 0x0A6


def populate_signals(id: int, data: dict[str, int], db: Database) -> bytes:
    """Fill unused CAN signals with 0 values. Necessary to appease CANTools"""
    db_msg = db.get_message_by_frame_id(id)

    for signal in db_msg.signals:
        if signal.name not in data:
            data[signal.name] = 0

    return db.encode_message(db_msg.name, data)

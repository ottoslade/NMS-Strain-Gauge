import logging
import time

from cantools.database import Database
from configs.apps_config import *
from configs.brake_config import *
from configs.can_config import CANId, populate_signals
from configs.rpi_config import RPiDAC, RPiGPIO

from drivers.can_driver import CanDriver

log = logging.getLogger(__name__)

MESSAGE_DELAY = 1


def test_ready_to_drive(can_bus: CanDriver, vehicle_db: Database) -> None:
    """This tests the functionality of the Ready To Drive (RTD) button on the vehicle.
    Once the STM32F4 recieves HVActive and the brakes are pressed to mechanical engage
    the driver can then press the RTD button. The STM32F4 should activate the buzzer
    and the throttle CAN message should contain the inverter enable bit flipped from 0 to 1.
    """

    # Step 1: Send DC Voltage over CAN periodically to indicate HVActive
    message = {
        "D1_DC_Bus_Voltage": 61,
    }
    data = populate_signals(CANId.M167_VOLTAGE_INFO, message, vehicle_db)
    can_bus.write_periodic(id=CANId.M167_VOLTAGE_INFO, data=data, period=MESSAGE_DELAY)

    log.info("Set D1_DC_Bus_Voltage CAN signal to 61 (value > 60 means HV is ON)")

    # Step 2: Send BMS fault message over CAN periodically to indicate no faults
    can_bus.write_periodic(id=CANId.BMS_FAULT, data=[0, 0, 0, 0], period=MESSAGE_DELAY)
    log.info("Set BMS Fault message to transmit NO FAULTS")

    # Step 3: Turn on GPIO on RPi for both brake inputs
    # TODO test values based on the threshold set in firmware, the max pressure is a bit unrealistic
    RPiDAC.BrakesFront.set_value(F_BRAKE_MAX_BIT)
    RPiDAC.BrakesRear.set_value(R_BRAKE_MAX_BIT)

    log.info("Set brake DACs to pressure > 0")
    log.info(f"Set front brake DAC to bits: {F_BRAKE_MAX_BIT}")
    log.info(f"Set rear brake DAC to bits: {R_BRAKE_MAX_BIT}")

    # Step 4: Delay to allow signal to reach STM32F4 before activating RTD
    time.sleep(0.1)

    # Step 5: Turn on GPIO on RPi for RTD button
    RPiGPIO.RTD.write(1)
    log.info("Set RTD switch GPIO to HIGH")

    log.info("Testing that Buzzer signal on CAN is set to ENABLE")
    # Step 6: Check that buzzer output from STM32F4 is on (timeout if no msg)
    result = can_bus.read_value_time(
        id=CANId.READY_TO_DRIVE,
        signal="Buzzer",
        expected="ENABLE",
        db=vehicle_db,
        time_req=1,
        timeout_s=5,
    )
    assert result.is_ok(), result.value

    log.info("Buzzer signal on CAN is set to ENABLE")

    log.info("Testing that Inverter_Enable signal on CAN is set to 1")
    # Step 7: Check that throttle CAN message contains correct inverter enable bit value
    result = can_bus.read_value_consistent(
        id=CANId.M192_COMMAND_MESSAGE,
        signal="Inverter_Enable",
        expected=1,
        db=vehicle_db,
        timeout_s=5,
    )
    assert result.is_ok(), result.value

    log.info("Inverter_Enable signal on CAN is set to 1")

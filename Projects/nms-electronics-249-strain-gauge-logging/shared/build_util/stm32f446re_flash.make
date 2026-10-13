OPENOCD = openocd

flash: all
ifndef BOOTLOADER_BUILD
	$(MAKE) -C ../bootloaders/stm32f446re-bootloader flash
endif
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program $(BUILD_DIR)/$(TARGET).elf verify reset exit"

debug: 
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f4x.cfg

flash-erase: 
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f4x.cfg -c "init" -c "halt" -c "flash erase_address 0x8000000 0x80000" -c "exit"
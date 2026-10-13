[**THE RULES!!!!**](https://www.fsaeonline.com/cdsweb/app/NewsItem.aspx?NewsItemID=379e4a8a-80a2-4a74-87c2-6f2de4212270)

- CRTL-F:
  - Ready to drive
  - APPS (Accelerator pedal position sensors)
- Try to find this stuff in our code, both ecu-front and hil implement it

**Software**

- [KiCad 8.0](https://www.kicad.org/download/) - **YOU HAVE TO USE THIS VERSION**
  - PCB design + viewing
- [SavvyCAN](https://www.savvycan.com/)
  - CAN bus viewer
  - DBC editor
  - Mac/windows/linux(?) download
  - [Tutorial video (kinda old but still good)](https://www.youtube.com/watch?v=kdA5Gdf3FAk&ab_channel=CollinKidder)
- [uv](https://docs.astral.sh/uv/) - python package management
- [Pre-commit](https://pre-commit.com/) - style checks on every commit
- [posh-git](https://github.com/dahlbyk/posh-git) - git autocomplete on windows
  - `$GitPromptSettings.EnablePromptStatus = $false` in your powershell profile if you don't like the custom prompt but do want autocomplete

**Libraries**

- [PyTest](https://docs.pytest.org/en/stable/contents.html) -- Testing framework that we use
- [CanTools](https://cantools.readthedocs.io/en/latest/) -- Parses our DBC files, allows us to refer to fields instead of bits when checking message contents
- [RPiGPIO](https://pypi.org/project/RPi.GPIO/) -- Control RPi gpio voltages (HIGH/LOW, PWM)
- [PythonCAN](https://python-can.readthedocs.io/en/stable/) -- Read messages off a CAN bus in python
- FreeRTOS: Not used in HIL tests but necessary for understanding code on STMs
  - [Introduction](https://www.freertos.org/Why-FreeRTOS/What-is-FreeRTOS)
  - [Tutorial book](https://www.freertos.org/media/2018/161204_Mastering_the_FreeRTOS_Real_Time_Kernel-A_Hands-On_Tutorial_Guide.pdf) (very long, would recommend just using as a reference book)
  - [Video series](https://www.youtube.com/watch?v=F321087yYy4&list=PLEBQazB0HUyQ4hAPU1cJED6t3DU0h34bz&ab_channel=DigiKey)

**Hardware**

- CAN -- [Wikipedia](https://en.wikipedia.org/wiki/CAN_bus)
  - Message protocol, noise resistant
  - [Video](https://www.youtube.com/watch?v=JZSCzRT9TTo&ab_channel=Electronoobs)
- STM32 -- [Wikipedia](https://en.wikipedia.org/wiki/STM32)
  - [Tutorial Video Series](https://www.youtube.com/watch?v=hyZS2p1tW-g&list=PLEBQazB0HUyRYuzfi4clXsKUSgorErmBv)
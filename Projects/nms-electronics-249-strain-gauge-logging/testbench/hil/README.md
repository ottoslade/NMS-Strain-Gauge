# Hardware In the Loop (HIL) test bench
Welcome the HIL project! The goal of this project is to simulate running the car using a Raspberry Pi in order to test the software running on our STMs. 

## Structure:
- `/docs`: Documentation
- `/drivers`: Drivers for the RPi
- `/tests`: All the code related to the HIL tests
    - `/configs`: Static config info and helper functions
    - `/fixtures`: Pytest fixtures
    - `/suites`: Test Categories

## Getting Started
To run our tests, first [configure the Pi](docs/rpi_setup.md), then install run the setup script:
```bash
# enable execute permissions on configure.sh
chmod +x ./scripts/configure.sh

# execute the configure.sh script
./scripts/configure.sh
```
and finally invoke pytest:
```bash
uv run pytest
```
See the [Ready To Drive Test](tests/suites/ecu_front/test_ready_to_drive.py) for a heavily commented example.

### Developing on Your Personal Laptop
As we only have 1 Raspberry Pi, a lot of development is done on your personal laptop before testing and finalizing on the Pi. Below are directions to get started:
1) First [Install uv](https://docs.astral.sh/uv/getting-started/installation/) for your chosen OS
2) Navigate to this directory and run
```bash
uv sync --dev
uv run pre-commit install
```
To install all necessary dependencies on your machine and the [git hooks](https://git-scm.com/book/ms/v2/Customizing-Git-Git-Hooks).
3) Start developing! See the [Ready To Drive Test](tests/suites/ecu_front/test_ready_to_drive.py) for a heavily commented example.

## Documentation
- [Pytest](docs/pytest.md)
- [RPi Setup](docs/rpi_setup.md)
- [Ruff - Python Linting](docs/ruff.md)
- [uv - Python Project Management](docs/uv.md)

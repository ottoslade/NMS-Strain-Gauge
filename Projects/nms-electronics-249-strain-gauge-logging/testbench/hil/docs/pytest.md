# Pytest

We're using the [Pytest](https://docs.pytest.org/en/stable/) framework to run our tests. This comes with a couple of stylistic quirks.

## Naming

Pytest automatically finds tests based on function and file names. Because of this, you need to name your test files in the form `test_*.py`. Your functions also have to be prefixed with `test_*`. For example:
```python
# test_feature.py

def test_feature():
    assert true, "this never fails"
```

See [here](https://docs.pytest.org/en/stable/explanation/goodpractices.html#test-discovery) for more details.

## Running Pytest
To run pytest on all tests, do
```bash
uv run pytest
```

To run on a selection of tests do
```bash
uv run pytest tests/test_file.py
```
See [here](https://docs.pytest.org/en/stable/how-to/usage.html) for more details.

## Asserts

Pytest primarily uses user defined `assert` statements to determine whether a test passes. Assert statements have a condition and then a string that is printed if that condition is false. This means you need to be able to turn STM behavior into a true/false condition, which can be tricky at times. In a lot of our tests, we need to receive a CAN message and check its contents, this is shown below.
```python
# test_stm_thing.py

def test_stm_thing():
    msg = receive_can_msg() # mock function, don't actually do this
    assert msg, "we didn't get the message"
    assert msg.id == 0x123 f"message id was {msg.id} not '0x123'"
    assert msg.data[0] == 5 f"message.data[0] was {msg.data[0]} not 5"
```

There's a lot more you can do with asserts, see [here](https://docs.pytest.org/en/stable/how-to/assert.html).

## Fixtures

Pytest uses a feature called fixtures to remove duplicating a lot of setup/teardown code in your tests. A fixture is basically just an object that you're going to use in your test. We use them to interact with our a CAN Bus. For example:

```python
# tests/fixtures/can_fixtures.py

from drivers.can_driver import CanDriver

@pytest.fixture(scope="session")
def vcan_bus():
    """Virtual CAN Bus wrapper around the `python-can` library"""
    driver = CanDriver(virtual=True)
    driver.connect()
    yield driver
    driver.disconnect()

# tests/test_vcan.py

def test_vcan(vcan_bus: CanDriver) -> None:
    """Test sending/receiving over vcan."""
    # vcan_bus is our fixture!
    ...

# tests/conftest.py

pytest_plugins = ["fixtures.can_fixtures"]
```
When you use the `@pytest.fixture` decorator, pytest will register your fixture and call it if you ever have the fixture name as a parameter. The `yield` keyword is used to give the fixture to the calling function and anything after is cleanup code to be run after the test finishes. The `scope` parameter is used to tell pytest how long we want the fixture to live. In this case, we're using `"session"` to tell pytest that we want to run the cleanup code once every test is done running. The default scope is per function, which means the cleanup code will be run after every test.

We also register our fixtures in the `tests/conftest.py` file so we don't have to import them.

You can also view a list of available fixtures.
```
uv run pytest --fixtures
```

Pytest fixtures can be pretty confusing at first because you can do so much with them, please read [this](https://docs.pytest.org/en/stable/how-to/fixtures.html#) and [this](https://docs.pytest.org/en/stable/explanation/fixtures.html#about-fixtures) as well for a much more in depth explanation.





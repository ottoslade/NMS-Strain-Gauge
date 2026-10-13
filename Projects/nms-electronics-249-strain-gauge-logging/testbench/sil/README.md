# Software in the Loop Testing

We'll be using the fff to mock aspects of FreeRTOS and just test our code.

The tests may not end up in this directory, may be easier to just put them along with the src, but we'll see.

## Fake Function Framework (fff)

- [Intro to Mocking](https://microsoft.github.io/code-with-engineering-playbook/automated-testing/unit-testing/mocking/) -- READ THIS FIRST
- [Source](https://github.com/meekrosoft/fff)
- Guy who wrote the book on test driven development for embedded C ([literally what it's called](https://www.amazon.com/Test-Driven-Development-Embedded-C-ebook/dp/B0DM6M5JT5?ref_=ast_author_mpb))
    - [Blog pt. 1](https://blog.wingman-sw.com/unit-testing-rtos-dependent-code-rtos-test-double-part-1)
    - [Blog pt. 2](https://blog.wingman-sw.com/unit-testing-rtos-dependent-code-rtos-test-double-part-2)
    - [Blog pt. 3](https://blog.wingman-sw.com/unit-testing-rtos-dependent-code-rtos-test-double-part-3)
- [Stack Overflow for our exact use case](https://stackoverflow.com/questions/66765044/fake-function-framework-not-working-with-stm32-hal-functions)

## Example

See the `\example` directory for a simple example. Use cmake/ctest to run.
```bash
cd example
cmake -B build && cmake --build build
ctest --test-dir build --output-on-failure
```
### Overview

```java
├── CMakeLists.txt          // build information
├── fff.h                   // fff library
├── inc
│   ├── hal.h               // function we're mocking [header]
│   └── main.h              // functionality we're testing [header]
├── src
│   ├── hal.c               // function we're mocking [impl]
│   └── main.c              // function we're testing [impl]
└── tests
    └── test_hello_world.c  // test
```

Note that we wrap `main()` in a check for the `NMS_TESTING` definition. This allows us to selectively compile the main function in `main.c` so that we don't have two main functions when we compile the test.
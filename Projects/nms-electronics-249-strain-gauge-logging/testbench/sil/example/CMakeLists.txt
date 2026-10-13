cmake_minimum_required(VERSION 3.15)
project(hello_world_tests C)

# enable testing infrastructure
enable_testing()

# set C standard 
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- fff: single-header mocking framework ---
add_library(fff INTERFACE)
target_include_directories(fff INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# regular executable
add_executable(hello_world src/main.c src/hal.c)
target_include_directories(hello_world PRIVATE inc)

# test executable
add_executable(hello_world_test tests/test_hello_world.c src/main.c)
target_compile_definitions(hello_world_test PUBLIC -DNMS_TESTING)
target_include_directories(hello_world_test PRIVATE inc)
target_link_libraries(hello_world_test PRIVATE fff)

# add tests to ctest
add_test(
    NAME hello_world_test
    COMMAND $<TARGET_FILE:hello_world_test>
)

# convenience target to run all tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS hello_world_test
)
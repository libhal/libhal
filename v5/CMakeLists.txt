# Copyright 2024 - 2025 Khalil Estell and the libhal contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.28)

# Generate compile commands for anyone using our libraries.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(hal LANGUAGES CXX)

# ==============================================================================
# Library
# ==============================================================================

find_package(strong_ptr REQUIRED)
find_package(async_context REQUIRED)

add_library(hal STATIC)
target_compile_features(hal PUBLIC cxx_std_23)
target_link_libraries(hal PUBLIC strong_ptr async_context)
target_sources(hal PUBLIC
    FILE_SET CXX_MODULES
    TYPE CXX_MODULES

    FILES
    modules/hal.cppm
    modules/adc.cppm
    modules/units.cppm
    modules/input_pin.cppm
    modules/accelerometer.cppm
    modules/distance_sensor.cppm
    modules/temperature_sensor.cppm
    modules/current_sensor.cppm
    modules/rotation_sensor.cppm
    modules/angular_velocity_sensor.cppm
    modules/output_pin.cppm
    modules/dac.cppm
    modules/motor.cppm
    modules/error.cppm
    modules/pwm.cppm
    modules/servo.cppm
    modules/gyroscope.cppm
    modules/magnetometer.cppm
    modules/steady_clock.cppm
    modules/inplace_function.cppm
    modules/callback.cppm
)

install(
    TARGETS hal
    EXPORT hal_targets
    FILE_SET CXX_MODULES DESTINATION "."
    LIBRARY DESTINATION "lib"
    ARCHIVE DESTINATION "lib"
    CXX_MODULES_BMI DESTINATION "bmi"
)

install(
    EXPORT hal_targets
    FILE "libhal-config.cmake"
    NAMESPACE libhal::
    DESTINATION "lib/cmake"
    CXX_MODULES_DIRECTORY "cxx-modules"
)

# ==============================================================================
# Unit testing
# ==============================================================================

if(CMAKE_CROSSCOMPILING)
    message(STATUS "Cross compiling, skipping unit test execution")
else()
    find_package(ut REQUIRED)

    add_executable(unit_test)

    # Add module files
    target_sources(unit_test
    PRIVATE
        tests/adc.test.cpp
        tests/main.test.cpp
        tests/callback.test.cpp
    )

    target_compile_features(unit_test PUBLIC cxx_std_23)
    target_link_libraries(unit_test PRIVATE hal Boost::ut)

    add_custom_target(run_tests ALL DEPENDS unit_test COMMAND unit_test)
endif()

# Always run this custom target by making it depend on ALL
add_custom_target(copy_compile_commands ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json
    DEPENDS ${CMAKE_BINARY_DIR}/compile_commands.json)

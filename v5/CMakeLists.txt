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

cmake_minimum_required(VERSION 4.0)

project(libhal LANGUAGES CXX)

# libhal cmake utilities from libhal-cmake-util
# Source: https://github.com/libhal/libhal-cmake-util
find_package(LibhalCMakeUtil REQUIRED)

find_package(strong_ptr REQUIRED)
find_package(mp-units REQUIRED)
find_package(async_context REQUIRED)

libhal_project_init()
libhal_add_library(libhal
    MODULES
        modules/hal.cppm
        modules/analog.cppm
        modules/units.cppm
        modules/gpio.cppm
        modules/power_sensors.cppm
        modules/motion_sensors.cppm
        modules/motor.cppm
        modules/error.cppm
        modules/pwm.cppm
        modules/servo.cppm
        modules/steady_clock.cppm
        modules/interrupts.cppm
)

target_link_libraries(libhal
    PRIVATE
        libhal::strong_ptr
        libhal::async_context
        mp-units::mp-units
)

libhal_apply_compile_options(libhal)
libhal_install_library(libhal NAMESPACE libhal)
libhal_add_tests(libhal
    TEST_NAMES
        adc
        interrupts
)

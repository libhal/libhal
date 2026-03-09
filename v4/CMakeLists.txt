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

# if(CMAKE_CROSSCOMPILING)
#     message(STATUS "Cross compiling, skipping unit test execution")
# else()

  find_package(tl-function-ref REQUIRED)

  libhal_project_init()

  add_library(libhal INTERFACE)
  target_include_directories(libhal INTERFACE include)

  libhal_add_tests(libhal
      TEST_NAMES
        can
        pwm
        timer
        i2c
        spi
        adc
        dac
        initializers
        input_pin
        interrupt_pin
        output_pin
        serial
        steady_clock
        motor
        timeout
        error
        accelerometer
        distance_sensor
        gyroscope
        magnetometer
        rotation_sensor
        temperature_sensor
        servo
        g_force
        io_waiter
        lengths
        angular_velocity_sensor
        current_sensor
        stream_dac
        lock
        usb
        zero_copy_serial
        pointers
        circular_buffer
        allocated_buffer

      PACKAGES
        tl-function-ref

      LINK_LIBRARIES
        tl::function-ref
  )
# endif()

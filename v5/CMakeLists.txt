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

project(libhal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Require Ninja or Visual Studio for modules
if(NOT CMAKE_GENERATOR MATCHES "Ninja|Visual Studio")
    message(FATAL_ERROR "C++20 modules require Ninja or Visual Studio generator")
endif()

# Create the library target
add_library(libhal)

# Add module files
target_sources(libhal PUBLIC
    FILE_SET "cxx_modules"
    TYPE "CXX_MODULES"
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
)

target_compile_features(libhal PUBLIC cxx_std_23)

# Compiler-specific validation and flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.0")
        message(FATAL_ERROR "GCC 14+ required for modules")
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16.0")
        message(FATAL_ERROR "Clang 16+ required for modules")
    endif()
    # May need libc++ on some systems
    target_compile_options(libhal PUBLIC
        $<$<PLATFORM_ID:Linux>:-stdlib=libc++>)
    target_link_options(libhal PUBLIC
        $<$<PLATFORM_ID:Linux>:-stdlib=libc++>)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.34")
        message(FATAL_ERROR "MSVC 14.34+ required for modules")
    endif()
endif()

# For Conan packaging - install the target with its module file set
install(TARGETS libhal
    EXPORT libhal-targets
    FILE_SET cxx_modules DESTINATION .
)

install(EXPORT libhal-targets
    DESTINATION .
    NAMESPACE libhal::
    FILE libhal-targets.cmake
)

# ==============================================================================
# Unit testing
# ==============================================================================

find_package(ut REQUIRED)

add_executable(unit_test)

# Add module files
target_sources(unit_test
  PRIVATE
      tests/adc.test.cpp
      tests/main.test.cpp
)

target_compile_features(unit_test PUBLIC cxx_std_23)
target_link_libraries(unit_test PRIVATE libhal boost-ext-ut::ut)

# Run unit tests
add_custom_target(run_tests ALL DEPENDS unit_test COMMAND unit_test)

// Copyright 2024 - 2025 Khalil Estell and the libhal contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

module;

#include <chrono>
#include <cstdint>

export module hal:units;

import mp_units;

using namespace mp_units;
using namespace mp_units::si::unit_symbols;

/**
 * @brief The foundation of libhal containing, interfaces, utilities and soft
 * drivers.
 *
 */
export namespace hal::inline v5 {

/// The standard time durations in libhal std::chrono::nanoseconds
using time_duration = std::chrono::nanoseconds;

/// Standard type for bytes in libhal.
/// Libhal does not use `std::byte` because it has a number of annoyances that
/// results in more verbose code, usually a lot of static_casts, without much
/// benefit. Thus hal::byte was created.
using byte = std::uint8_t;

// Shortened versions of the standard integers. Integer names adopted from the
// Rust and Zig.
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = std::uintptr_t;
using uptr = std::uintptr_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using isize = std::uintptr_t;
using iptr = std::uintptr_t;

using common_rep_t = float;

/// Type for frequency represented in hertz. We use u32 because sub 1-hertz
/// frequencies are seldom used in application. In general, the whole number
/// part of a frequency is more important than the decimal parts, so no need to
/// pay for floating point operations on frequency values.
using hertz = quantity<si::hertz, u32>;

/// Type for acceleration represented in the force applied by gravity at sea
/// level.
using acceleration = quantity<si::metre / pow<2>(si::second), common_rep_t>;
using velocity = quantity<si::metre / si::second, common_rep_t>;

/// Type for current represented in amps.
using amperes = quantity<si::ampere, common_rep_t>;

/// Type for voltage represented in volts.
using volts = quantity<si::volt, common_rep_t>;

/// Type for temperature represented in kelvin.
using kelvin = quantity<si::kelvin, common_rep_t>;

/// Type for angular velocity represented in degrees per second
using angular_velocity = quantity<si::degree / si::second, common_rep_t>;

/// Type for rotational velocity represented in RPMs.
using rpm = quantity<mag<360> * si::degree / si::minute, common_rep_t>;

/// Type for length represented in meters.
using meters = quantity<si::metre, common_rep_t>;

/// Type for angle represented in degrees.
using degrees = quantity<si::degree, common_rep_t>;

/// Type for magnetic field represented in gauss.
using teslas = quantity<si::tesla, common_rep_t>;

/// Type for torque represented in newton meters.
using newton_meter = quantity<si::newton * si::metre, common_rep_t>;

/**
 * @brief Set of possible pin mode resistor settings.
 *
 * See each enumeration to get more details about when and how these should be
 * used.
 *
 */
enum class pin_resistor : u8
{
  /// No pull up. This will cause the pin to float. This may be desirable if the
  /// pin has an external resistor attached or if the signal is sensitive to
  /// external devices like resistors.
  none = 0,
  /// Pull the pin down to devices GND. This will ensure that the voltage read
  /// by the pin when there is no signal on the pin is LOW (or false).
  pull_down,
  /// See pull down explanation, but in this case the pin is pulled up to VCC,
  /// also called VDD on some systems.
  pull_up,
};
}  // namespace hal::inline v5

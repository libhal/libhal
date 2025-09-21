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

#pragma once

#include <algorithm>

#include "units.hpp"

namespace hal {

/**
 * @brief 16-bit Digital to Analog Converter (DAC) hardware abstraction
 * interface.
 *
 * Use this interface for devices and peripherals that can create arbitrary
 * analog voltages between a defined Vss (negative reference) and Vcc (positive
 * reference) voltage.
 *
 * This interface can represent DAC's with precision 16-bits and below. See the
 * `hal::dac16::write()` API for more details.
 */
class dac16
{
public:
  /**
   * @brief Set the output voltage of the DAC.
   *
   * The input value `p_percentage` is a 16-bit unsigned number from 0 (0x0000)
   * to 65535 (0xFFFF).
   *
   * The floating point value is linearly proportional to the output voltage
   * relative to the Vss and Vcc such that if Vss is 0V (gnd) and Vcc is 5V
   * then:
   *
   *   - 0.000V (0.000%) is 65535 * 0.000 = 0
   *   - 1.250V (25.00%) is 65535 * 0.250 = 16383
   *   - 2.225V (44.50%) is 65535 * 0.445 = 29163
   *   - 5.000V (100.0%) is 65535 * 1.000 = 65535
   *
   * For drivers where the underlying hardware has a DAC precision below
   * 16-bits, the behavior of this API is to set the DAC output register value
   * to the most significant bits of the u16 value. This preserves the most
   * significant information about the intended dac percentage value.
   *
   * @param p_percentage - value from 0 (0x0000) to 65535 (0xFFFF) representing
   * the proportion of the output voltage from the Vss to Vcc.
   */
  void write(u16 p_percentage)
  {
    driver_write(p_percentage);
  }

  virtual ~dac16() = default;

private:
  virtual void driver_write(u16 p_percentage) = 0;
};

// NOTE: If a dac24, for dac precisions between 17 and 24, is desired by
// developers, we can add that interface in. Not adding it now as to not add
// more unused interfaces.

/**
 * @brief Digital to Analog Converter (DAC) hardware abstraction interface.
 *
 * Use this interface for devices and peripherals that can create arbitrary
 * analog voltages between a defined Vss (negative reference) and Vcc (positive
 * reference) voltage.
 *
 */
class dac
{
public:
  /**
   * @brief Set the output voltage of the DAC.
   *
   * The input value `p_percentage` is a 32-bit floating point value from 0.0f
   * to +1.0f.
   *
   * The floating point value is linearly proportional to the output voltage
   * relative to the Vss and Vcc such that if Vss is 0V (gnd) and Vcc is 5V then
   * 0.0 is 0V, 0.25 is 1.25V, 0.445 is 2.225V and 1.0 is 5V.
   *
   * This function clamps the input value between 0.0f and 1.0f and thus values
   * passed to driver implementations are guaranteed to be within this range.
   * Callers of this function do not need to clamp their values before passing
   * them into this function as it would be redundant. The rationale for doing
   * this at the interface layer is that it allows callers and driver
   * implementors to omit redundant clamping code, reducing code bloat.
   *
   * @param p_percentage - value from 0.0f to +1.0f representing the proportion
   * of the output voltage from the Vss to Vcc.
   */
  void write(float p_percentage)
  {
    auto clamped_percentage = std::clamp(p_percentage, 0.0f, 1.0f);
    driver_write(clamped_percentage);
  }

  virtual ~dac() = default;

private:
  virtual void driver_write(float p_percentage) = 0;
};
}  // namespace hal

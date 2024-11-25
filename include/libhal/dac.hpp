// Copyright 2024 Khalil Estell
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
#include <limits>

#include "units.hpp"

namespace hal {
/**
 * @brief Digital to Analog Converter (DAC) hardware abstraction interface.
 *
 * Use this interface for devices and peripherals that can create arbitrary
 * analog voltages between a defined Vss (negative reference) and Vcc (positive
 * reference) voltage.
 *
 */
class dac16
{
public:
  /**
   * @brief Set the output voltage of the DAC.
   *
   * The input value `p_percentage` is a 16-bit full-scale value from 0
   * to 65'535.
   *
   * The value is linearly proportional to the output voltage relative to the
   * Vss and Vcc such that if Vss is 0V (GND) and Vcc is 5V then:
   *
   *     - 0% = 0V = 0
   *     - 25% = 1.25V = 16'384
   *     - 44.5% = 2.225V = 29'163
   *     - 100% = 5V = 65'535
   *
   * @param p_percentage - value from 0 to 65'535 representing the proportion
   * of the output voltage from the device's Vss to Vcc.
   */
  void write(u16 p_percentage)
  {
    driver_write(p_percentage);
  }

  virtual ~dac16() = default;

private:
  virtual void driver_write(u16 p_percentage) = 0;
};
}  // namespace hal

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

#include "units.hpp"
#include <limits>

namespace hal {
/**
 * @brief 16-bit resolution Analog to Digital Converter (ADC) abstraction
 *
 * This interface represents ADCs with resolution 16-bit and below. This ADC
 * category is the most common between 16, 24 and 32.
 *
 * Use this interface for devices and peripherals that can convert analog
 * voltage signals into a digital number.
 *
 * ADC peripheral only know the proportion of a voltage signal relative to a Vss
 * (negative reference) and a Vcc (positive reference) and thus cannot describe
 * the voltage directly.
 */
class adc16
{
public:
  static constexpr auto max = std::numeric_limits<u16>::max();

  /**
   * @brief Sample the analog to digital converter and return the result
   *
   * Is guaranteed by the implementing driver to be between 0 and 65'535.
   * For devices with resolutions below 16-bit, the value will be up-scaled by
   * the implementation in order to reach the proportional full scale value.
   *
   * The value representing the voltage measured by the ADC from Vss (negative
   * reference) to Vcc (positive reference). For example, if Vss is 0V (gnd)
   * and Vcc is 5V and this returns is 32'767 (50% of the maximum value for a
   * 16-bit number), then the voltage measured is 2.5V.
   *
   * @return u16 - the sampled adc value
   */
  [[nodiscard]] u16 read()
  {
    return driver_read();
  }

  virtual ~adc16() = default;

private:
  virtual u16 driver_read() = 0;
};

/**
 * @brief 24-bit resolution Analog to Digital Converter (ADC) abstraction
 *
 * This interface represents ADCs with resolution between 17-bit and 24-bit.
 * This ADC category is meant for high precision ADCs.
 *
 * Use this interface for devices and peripherals that can convert analog
 * voltage signals into a digital number.
 *
 * ADC peripheral only know the proportion of a voltage signal relative to a Vss
 * (negative reference) and a Vcc (positive reference) and thus cannot describe
 * the voltage directly.
 */
class adc24
{
public:
  static constexpr u32 max = (1 << 24) - 1;

  /**
   * @brief Sample the analog to digital converter and return the result
   *
   * Is guaranteed by the implementing driver to be between 0 and 16'777'215.
   * * For devices with resolutions below 24-bit, the value will be up-scaled by
   * the implementation in order to reach the proportional full scale value.
   *
   * The value representing the voltage measured by the ADC from Vss (negative
   * reference) to Vcc (positive reference). For example, if Vss is 0V (gnd)
   * and Vcc is 5V and this returns is 8'388'608 (50% of the maximum value for a
   * 24-bit number), then the voltage measured is 2.5V.
   *
   * @return u32 - the sampled adc value
   */
  [[nodiscard]] u32 read()
  {
    return driver_read();
  }

  virtual ~adc24() = default;

private:
  virtual u32 driver_read() = 0;
};

/**
 * @brief 32-bit resolution Analog to Digital Converter (ADC) abstraction
 *
 * This interface represents ADCs with resolution between 25-bit and 32-bit.
 * This ADC category is meant for very high precision ADCs. These are the least
 * common ADCs around.
 *
 * ADC peripheral only know the proportion of a voltage signal relative to a Vss
 * (negative reference) and a Vcc (positive reference) and thus cannot describe
 * the voltage directly.
 */
class adc32
{
public:
  static constexpr u32 max = std::numeric_limits<u32>::max();

  /**
   * @brief Sample the analog to digital converter and return the result
   *
   * Is guaranteed by the implementing driver to be between 0 and 4'294'967'295.
   * For devices with resolutions below 32-bit, the value will be up-scaled by
   * the implementation in order to reach the proportional full scale value.
   *
   * The value representing the voltage measured by the ADC from Vss (negative
   * reference) to Vcc (positive reference). For example, if Vss is 0V (gnd)
   * and Vcc is 5V and this returns is 2'147'483'647 (50% of the maximum value
   * for a 32-bit number), then the voltage measured is 2.5V.
   *
   * @return u32 - the sampled adc value
   */
  [[nodiscard]] u32 read()
  {
    return driver_read();
  }

  virtual ~adc32() = default;

private:
  virtual u32 driver_read() = 0;
};
}  // namespace hal

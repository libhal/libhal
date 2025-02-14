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

namespace hal {
/**
 * @brief Analog to Digital Converter (ADC) hardware abstraction interface.
 * @deprecated libhal encourages developers to refrain from using this class and
 * to use `hal::adc16` and `hal::adc24` instead. This interface WILL be removed
 * in libhal 5.0.0.
 *
 * Use this interface for devices and peripherals that can convert analog
 * voltage signals into a digital number.
 *
 * ADC peripheral only know the proportion of a voltage signal relative to a Vss
 * (negative reference) and a Vcc (positive reference) and thus cannot describe
 * the voltage directly.
 */
class adc
{
public:
  /**
   * @brief Sample the analog to digital converter and return the result
   *
   * Is guaranteed by the implementing driver to be between 0.0f and +1.0f.
   * The value representing the voltage measured by the ADC from Vss (negative
   * reference) to Vcc (positive reference). For example, if Vss is 0V (gnd)
   * and Vcc is 5V and this value is 0.5f, then the voltage measured is 2.5V.
   *
   * @return float - the sampled adc value
   */
  [[nodiscard]] float read()
  {
    return driver_read();
  }

  virtual ~adc() = default;

private:
  virtual float driver_read() = 0;
};

/**
 * @brief 16-bit Analog to Digital Converter (ADC) hardware abstraction
 * interface.
 *
 * Use this interface for devices and peripherals that can convert analog
 * voltage signals into a digital number.
 *
 * ADC peripheral only know the proportion of a voltage signal relative to a Vss
 * (negative reference) and a Vcc (positive reference) and thus cannot describe
 * the voltage directly.
 *
 * This interface is meant for ADCs of 16-bits and below. Most common ADCs fall
 * into this category.
 */
class adc16
{
public:
  /**
   * @brief Sample the analog to digital converter and return the result
   *
   * Is guaranteed by the implementing driver to be between 0 and 65535.
   * The value representing the voltage measured by the ADC from Vss (negative
   * reference) to Vcc (positive reference).
   *
   * For example, if Vss is 0V (gnd) and Vcc is 5V then a value of 32767 (half
   * of 65535) would mean a measured voltage of 2.5V.
   *
   * In the case where there ADC's resolution is below 16-bits, the
   * implementation is required to perform bit upscaling via bit duplications.
   *
   * For example consider an 8-bit ADC reading a value of 0x7F. This would be
   * upscaled to 0x7F7F by shifting the value to the 15th bit then duplicating
   * the bits to lower 8-bits. This can be done with any bit width ADC.
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
 * @brief 24-bit Analog to Digital Converter (ADC) hardware abstraction
 * interface.
 *
 * Use this interface for devices and peripherals that can convert analog
 * voltage signals into a digital number.
 *
 * ADC peripheral only know the proportion of a voltage signal relative to a Vss
 * (negative reference) and a Vcc (positive reference) and thus cannot describe
 * the voltage directly.
 *
 * This interface is meant for ADCs with precision of 17-bits to 24-bits. This
 * is interface is meant for high precision ADCs
 *
 */
class adc24
{
public:
  /**
   * @brief Sample the analog to digital converter and return the result
   *
   * Is guaranteed by the implementing driver to be between 0 and 16777215.
   * The value representing the voltage measured by the ADC from Vss (negative
   * reference) to Vcc (positive reference).
   *
   * For example, if Vss is 0V (gnd) and Vcc is 5V then a value of 8388607 (half
   * of 16777215) would mean a measured voltage of 2.5V.
   *
   * See `hal::adc16` for details about how ADCs with precision below 24 are
   * upscaled to match the necessary precision.
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

// NOTE: libhal doesn't have any use cases or customers that require adc32, so
// that is not included yet. But if a user requested this in the github issues,
// we could consider adding it.
}  // namespace hal

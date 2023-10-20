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

namespace hal {
/**
 * @brief Analog to Digital Converter (ADC) hardware abstraction interface.
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
}  // namespace hal

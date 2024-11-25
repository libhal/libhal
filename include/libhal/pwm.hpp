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

#include "units.hpp"

namespace hal {
/**
 * @brief 16-bit resolution Pulse Width Modulation (PWM) abstraction
 *
 * This driver controls the waveform generation of a square wave and its
 * properties such as frequency and duty cycle.
 *
 * Frequency, meaning how often the waveform cycles from from low to high.
 *
 * Duty cycle, what proportion of the wavelength of the pulse is the voltage
 * HIGH.
 *
 *  ```
 *     ____________________       _
 *    |                    |     |
 *   _|                    |_____|
 *    ^                    ^     ^
 *    |<------ high ------>|<low>|
 *
 *    HIGH Duration = 18 segments
 *    LOW Duration =  5 segments
 *    Duty Cycle = 20 / (20 + 5) = 80%
 *
 *    If each segment is 1us then the wavelength is 25us
 *    Thus frequency is (1 / 25us) = 40kHz
 * ```
 *
 * PWM is used for power control like motor control, lighting, transmitting
 * signals to servos, sending telemetry and much more.
 *
 */
class pwm16
{
public:
  /**
   * @brief Set the pwm waveform frequency in hertz
   *
   */
  void frequency(u32 p_frequency)
  {
    return driver_frequency(p_frequency);
  }

  /**
   * @brief Set the pwm waveform duty cycle
   *
   * The input value `p_duty_cycle` is a 32-bit floating point value from 0.0f
   * to 1.0f.
   *
   * The floating point value is directly proportional to the duty cycle
   * percentage, such that 0.0f is 0%, 0.25f is 25%, 0.445f is 44.5% and 1.0f is
   * 100%.
   *
   * This function clamps the input value between 0.0f and 1.0f and thus  values
   * passed to driver implementations are guaranteed to be within this range.
   * Callers of this function do not need to clamp their values before passing
   * them into this function as it would be redundant. The rationale for doing
   * this at the interface layer is that it allows callers and driver
   * implementors to omit redundant clamping code, reducing code bloat.
   *
   * @param p_duty_cycle - a value from 0.0f to +1.0f representing the duty
   * cycle percentage.
   */
  void duty_cycle(u16 p_duty_cycle)
  {
    driver_duty_cycle(p_duty_cycle);
  }

  virtual ~pwm16() = default;

private:
  virtual void driver_frequency(u32 p_frequency) = 0;
  virtual void driver_duty_cycle(u16 p_duty_cycle) = 0;
};

/**
 * @brief 16-bit resolution Pulse Width Modulation (PWM) duty cycle abstraction
 *
 * Works just like pwm16 except frequency control is not available. This
 * interface supports the abstraction for pwm devices where multiple pwm duty
 * cycles are available but all controlled based on a single operating
 * frequency. Changing that sole frequency changes the frequency for ALL pwm
 * channels associated with that hardware.
 *
 */
class pwm_duty_cycle16
{
public:
  /**
   * @brief Get the pwm waveform frequency in hertz
   *
   */
  u32 frequency()
  {
    return driver_frequency();
  }

  /**
   * @brief Set the pwm waveform duty cycle
   *
   * The input value `p_duty_cycle` is a 32-bit floating point value from 0.0f
   * to 1.0f.
   *
   * The floating point value is directly proportional to the duty cycle
   * percentage, such that 0.0f is 0%, 0.25f is 25%, 0.445f is 44.5% and 1.0f is
   * 100%.
   *
   * This function clamps the input value between 0.0f and 1.0f and thus  values
   * passed to driver implementations are guaranteed to be within this range.
   * Callers of this function do not need to clamp their values before passing
   * them into this function as it would be redundant. The rationale for doing
   * this at the interface layer is that it allows callers and driver
   * implementors to omit redundant clamping code, reducing code bloat.
   *
   * @param p_duty_cycle - a value from 0.0f to +1.0f representing the duty
   * cycle percentage.
   */
  void duty_cycle(u16 p_duty_cycle)
  {
    driver_duty_cycle(p_duty_cycle);
  }

  virtual ~pwm_duty_cycle16() = default;

private:
  virtual u32 driver_frequency() = 0;
  virtual void driver_duty_cycle(u16 p_duty_cycle) = 0;
};
}  // namespace hal

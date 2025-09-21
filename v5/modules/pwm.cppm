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

#include <algorithm>

export module hal:pwm;

export import :units;

export namespace hal::inline v5 {
/**
 * @brief Pulse Width Modulation (PWM) channel hardware abstraction.
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
class pwm16_channel
{
public:
  /**
   * @brief Returns the frequency of this pwm channel
   *
   * @returns hertz - frequency in hertz as an unsigned integer
   */
  hertz frequency()
  {
    return driver_frequency();
  }

  /**
   * @brief Set the pwm waveform duty cycle
   *
   * The input value `p_duty_cycle` is a 16-bit unsigned value from 0 to 65535.
   *
   * The 16-bit value is directly proportional to the duty cycle
   * percentage, such that a value of:
   *
   *     - 0% would be 0 (65535 * 0.00)
   *     - 100% would be 65535 (65535 * 1.00)
   *     - 25% would be 16384 (65535 * 0.25)
   *     - 44.5% would be 29163 (65535 * 0.445)
   *
   * @param p_duty_cycle - a value from 0 to 65535 representing the duty
   * cycle percentage.
   */
  void duty_cycle(u16 p_duty_cycle)
  {
    driver_duty_cycle(p_duty_cycle);
  }

  virtual ~pwm16_channel() = default;

private:
  virtual hertz driver_frequency() = 0;
  virtual void driver_duty_cycle(u16 p_duty_cycle) = 0;
};

/**
 * @brief Pulse Width Modulation (PWM) manager hardware abstraction.
 *
 * This interface allows control over a single or group of pwm channels.
 * Currently this interface only provides support for frequency control.
 */
class pwm_group_manager
{
public:
  virtual ~pwm_group_manager() = default;

  /**
   * @brief Set the waveform frequency for pwm channels managed by this driver
   *
   * This function will set the pwm frequency of some hardware. Changing the pwm
   * frequency will effect ALL pwm channels within the pwm hardware's group. How
   * a pwm channel is grouped with a pwm manager is hardware specific. Consult
   * the implementation for more details regarding which channels are effected
   * by each pwm manager.
   *
   * The pwm device may not be capable of providing the exact frequency passed
   * to this function. Implementations must perform a best-effort approximation
   * given their input clocks. If software requires the frequency to be within a
   * specific range, then the `hal::pwm_channel::frequency()` should be called
   * to verify that the frequency is within specification.
   *
   * Implementations must ensure that, after frequency modification that the
   * duty cycles of channels managed by this driver remains approximately the
   * same. If the frequency is changed from 1kHz to 2kHz and the duty cycle for
   * a pwm channel under this manager is 50%, then the duty cycle must remain in
   * ~50% up to the limits that the hardware can provide. In the case where a
   * choosen frequency results in reduced duty cycle precision, the
   * implementation is allowed to reduce duty cycle precision for all pwm
   * channels under its management.
   *
   * Implementations are not guaranteed to ensure a smooth transition from one
   * frequency to the next. Software should be designed around the idea that
   * modifying the PWM frequency while pwm channels are active can cause their
   * duty cycle to be disrupted for a few cycles.
   *
   * @param p_frequency - the frequency to apply to the pwm hardware.
   */
  void frequency(hertz p_frequency)
  {
    driver_frequency(p_frequency);
  }

private:
  virtual void driver_frequency(hertz p_frequency) = 0;
};
}  // namespace hal::inline v5

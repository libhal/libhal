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

#include "functional.hpp"
#include "units.hpp"

namespace hal {
/**
 * @brief Digital interrupt pin hardware abstraction
 *
 * Use this to automatically call a function when a pin's state has
 * transitioned.
 *
 * The transition states are:
 *
 *   - falling edge: the pin reads a transitions from HIGH to LOW
 *   - rising edge: the pin reads a transitions from LOW to HIGH
 *   - both: the pin reads any state change
 */
class interrupt_pin
{
public:
  /**
   * @brief The condition in which an interrupt it's triggered.
   *
   */
  enum class trigger_edge
  {
    /**
     * @brief Trigger the interrupt when a pin transitions from HIGH voltage to
     * LOW voltage.
     *
     */
    falling = 0,
    /**
     * @brief Trigger the interrupt when a pin transitions from LOW voltage to
     * HIGH voltage.
     *
     */
    rising = 1,
    /**
     * @brief Trigger the interrupt when a pin transitions it state
     *
     */
    both = 2,
  };

  /**
   * @brief Generic settings for interrupt pins
   *
   */
  struct settings
  {
    /**
     * @brief Pull resistor for an interrupt pin.
     *
     * In general, it is highly advised to either set the pull resistor to
     * something other than "none" or to attach an external pull up resistor to
     * the interrupt pin in order to prevent random interrupt from firing.
     */
    pin_resistor resistor = pin_resistor::pull_up;

    /**
     * @brief The trigger condition that will signal the system to run the
     * callback.
     *
     */
    trigger_edge trigger = trigger_edge::rising;

    /**
     * @brief Enables default comparison
     *
     */
    bool operator<=>(settings const&) const = default;
  };

  /**
   * @brief Interrupt pin handler
   *
   * param p_state - if true state of the pin when the interrupt was triggered
   * was HIGH, otherwise LOW
   */
  using handler = void(bool p_state);

  /**
   * @brief Configure the interrupt pin to match the settings supplied
   *
   * @param p_settings - settings to apply to interrupt pin
   * @throws hal::operation_not_supported - if the settings could not be
   * achieved.
   */
  void configure(settings const& p_settings)
  {
    driver_configure(p_settings);
  }

  /**
   * @brief Set the callback for when the interrupt occurs
   *
   * Any state transitions before this function is called are lost.
   *
   * @param p_callback - function to execute when the trigger condition occurs.
   */
  void on_trigger(hal::callback<handler> p_callback)
  {
    driver_on_trigger(p_callback);
  }

  virtual ~interrupt_pin() = default;

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  virtual void driver_on_trigger(hal::callback<handler> p_callback) = 0;
};
}  // namespace hal

namespace hal::v5 {
}  // namespace hal::v5

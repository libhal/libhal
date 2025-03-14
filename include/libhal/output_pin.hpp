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

#include "libhal/error.hpp"
#include "units.hpp"

namespace hal {

// Forward declare this so that the invalidated implementation can see the class
class invalidated_output_pin;

/**
 * @brief Digital output pin hardware abstraction.
 *
 * Use this to drive a pin HIGH or LOW in order to send a control signal or turn
 * off or on an LED.
 *
 * Implementations of this interface can be backed by external devices such as
 * I/O expanders or other micro-controllers.
 *
 */
class output_pin
{
public:
  using invalidated = invalidated_output_pin;

  /// Generic settings for output pins
  struct settings
  {
    /// Pull resistor for the pin. This generally only helpful when open
    /// drain is enabled.
    pin_resistor resistor = pin_resistor::none;

    /// Starting level of the output pin. HIGH voltage defined as true and LOW
    /// voltage defined as false.
    bool open_drain = false;

    /**
     * @brief Enables default comparison
     *
     */
    bool operator<=>(settings const&) const = default;
  };

  /**
   * @brief Configure the output pin to match the settings supplied
   *
   * @param p_settings - settings to apply to output pin
   * @throws hal::operation_not_supported - if the settings could not be
   * achieved.
   */
  void configure(settings const& p_settings)
  {
    driver_configure(p_settings);
  }

  /**
   * @brief Set the state of the pin
   *
   * @param p_high - if true then the pin state is set to HIGH voltage. If
   * false, the pin state is set to LOW voltage.
   */
  void level(bool p_high)
  {
    driver_level(p_high);
  }

  /**
   * @brief
   *
   * Implementations must read the pin state from hardware and will not simply
   * cache the results from the execution of `level(bool)`.
   *
   * This pin may not equal the state set by `level(bool)` when the pin is
   * configured as open-drain.
   *
   * @return true - if the level of the pin is HIGH
   * @return false - if the level of the pin is LOW
   */
  [[nodiscard]] bool level()
  {
    return driver_level();
  }

  virtual ~output_pin() = default;

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  virtual void driver_level(bool p_high) = 0;
  virtual bool driver_level() = 0;
};

class invalidated_output_pin : public hal::output_pin
{
public:
  invalidated_output_pin() = default;
  ~invalidated_output_pin() override = default;

private:
  void driver_configure(settings const&) override
  {
    hal::safe_throw(hal::lifetime_violation(this));
  }
  void driver_level(bool) override
  {
    hal::safe_throw(hal::lifetime_violation(this));
  }
  bool driver_level() override
  {
    hal::safe_throw(hal::lifetime_violation(this));
  }
};
}  // namespace hal

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

#include "units.hpp"

namespace hal {
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
}  // namespace hal

namespace hal::v5 {
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
}  // namespace hal::v5

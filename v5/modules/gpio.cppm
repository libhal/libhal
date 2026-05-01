// Copyright 2026 Khalil Estell and the libhal contributors
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

export module hal:gpio;

export import async_context;
export import :units;

namespace hal::inline v5 {
/**
 * @brief Digital input pin hardware abstraction interface.
 *
 * Use this to read a pin and determine if the voltage on it is HIGH or LOW.
 *
 */
export class input_pin
{
public:
  /// Generic settings for input pins
  struct settings
  {
    /// Pull resistor for an input pin
    pin_resistor resistor = pin_resistor::pull_up;

    /**
     * @brief Enables default comparison
     *
     */
    bool operator<=>(settings const&) const = default;
  };

  /**
   * @brief Configure the input pin to match the settings supplied
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_settings - settings to apply to input pin
   * @throws hal::operation_not_supported - if the settings could not be
   * achieved.
   */
  [[nodiscard]] async::future<void> configure(async::context& p_context,
                                              settings const& p_settings)
  {
    return driver_configure(p_context, p_settings);
  }

  /**
   * @brief Read the state of the input pin
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<bool> - true indicates HIGH voltage level and false
   * indicates LOW voltage level
   */
  [[nodiscard]] async::future<bool> level(async::context& p_context)
  {
    return driver_level(p_context);
  }

  virtual ~input_pin() = default;

private:
  virtual async::future<void> driver_configure(async::context& p_context,
                                               settings const& p_settings) = 0;
  virtual async::future<bool> driver_level(async::context& p_context) = 0;
};

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
export class output_pin
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
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_settings - settings to apply to output pin
   * @throws hal::operation_not_supported - if the settings could not be
   * achieved.
   */
  [[nodiscard]] async::future<void> configure(async::context& p_context,
                                              settings const& p_settings)
  {
    return driver_configure(p_context, p_settings);
  }

  /**
   * @brief Set the state of the pin
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @param p_high - if true then the pin state is set to HIGH voltage. If
   * false, the pin state is set to LOW voltage.
   */
  [[nodiscard]] async::future<void> level(async::context& p_context,
                                          bool p_high)
  {
    return driver_level(p_context, p_high);
  }

  /**
   * @brief Read the current state of the output pin from hardware
   *
   * Implementations must read the pin state from hardware and will not simply
   * cache the results from the execution of `level(bool)`.
   *
   * This pin may not equal the state set by `level(bool)` when the pin is
   * configured as open-drain.
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<bool> - true if the level of the pin is HIGH, false
   * if LOW
   */
  [[nodiscard]] async::future<bool> level(async::context& p_context)
  {
    return driver_level(p_context);
  }

  virtual ~output_pin() = default;

private:
  virtual async::future<void> driver_configure(async::context& p_context,
                                               settings const& p_settings) = 0;
  virtual async::future<void> driver_level(async::context& p_context,
                                           bool p_high) = 0;
  virtual async::future<bool> driver_level(async::context& p_context) = 0;
};
}  // namespace hal::inline v5

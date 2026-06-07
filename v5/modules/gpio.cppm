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

    /// Set to true to configure the pin to be in open drain mode
    bool open_drain = false;

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

protected:
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
export class output_pin : public input_pin
{
public:
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

  ~output_pin() override = default;

private:
  // Needed to explain to the compiler that we are using the
  // `input_pin::driver_level` and that the overload is allowable and the API
  // below is not shadowing you.
  using input_pin::driver_level;
  virtual async::future<void> driver_level(async::context& p_context,
                                           bool p_high) = 0;
};

/// An enumeration representing a digital state transition
export enum class edge_trigger : u8 {
  /// Trigger the interrupt when a pin transitions from HIGH voltage to
  /// LOW voltage.
  falling = 0,
  /// Trigger the interrupt when a pin transitions from LOW voltage to
  /// HIGH voltage.
  rising = 1,
  /// Trigger the interrupt when a pin transitions it state
  both = 2,
};

export class awaitable_pin : public input_pin
{
public:
  [[nodiscard]] async::future<void> on_transition(async::context& p_context,
                                                  edge_trigger p_trigger)
  {
    return driver_on_transition(p_context, p_trigger);
  }

  ~awaitable_pin() override = default;

private:
  virtual async::future<void> driver_on_transition(async::context& p_context,
                                                   edge_trigger p_trigger) = 0;
};
}  // namespace hal::inline v5

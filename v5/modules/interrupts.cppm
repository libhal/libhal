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

export module hal:interrupts;

export import strong_ptr;
export import :units;

export namespace hal::inline v5 {

/// An enumeration representing a digital state transition
enum class edge_trigger : u8
{
  /// Trigger the interrupt when a pin transitions from HIGH voltage to
  /// LOW voltage.
  falling = 0,
  /// Trigger the interrupt when a pin transitions from LOW voltage to
  /// HIGH voltage.
  rising = 1,
  /// Trigger the interrupt when a pin transitions it state
  both = 2,
};

struct edge_triggered_callback
{
  /**
   * @brief The callback invoked after an edge trigger on a pin has occurred
   *
   * @param p_state - the current logical state of the pin with
   */
  virtual void callback(bool p_state) = 0;
};

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
class edge_triggered_interrupt
{
public:
  /// @brief Generic settings for interrupt pins
  struct settings
  {
    /**
     * @brief The trigger condition that will signal the system to run the
     * callback.
     *
     */
    edge_trigger trigger = edge_trigger::rising;

    /**
     * @brief Enables default comparison
     *
     */
    bool operator<=>(settings const&) const = default;
  };

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
  void on_trigger(mem::optional_ptr<edge_triggered_callback> const& p_callback)
  {
    driver_on_trigger(p_callback);
  }

  virtual ~edge_triggered_interrupt() = default;

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  virtual void driver_on_trigger(
    mem::optional_ptr<edge_triggered_callback> p_callback) = 0;
};

struct timed_callback
{
  /**
   * @brief Callback invoked by interrupt after scheduled timed interrupt is
   * invoked
   *
   */
  virtual void callback() = 0;
};

/**
 * @brief An abstraction for hardware timed interrupts.
 *
 * Use this interface for devices and peripherals that have timer like
 * capabilities, such that, when a timer's time has expired, an event,
 * interrupt, or signal is generated.
 *
 * Timer drivers tick period must be an integer multiple of 1 nanosecond,
 * meaning that the only tick period allowed are 1ns, 2ns, up to the maximum
 * holdable in a std::chrono::nanosecond type. sub-nanosecond tick periods are
 * not allowed.
 *
 */
class timed_interrupt
{
public:
  /**
   * @brief Determine if the timer is currently pending
   *
   * @return true - if a callback has been scheduled and has not been invoked
   * yet, false otherwise.
   */
  [[nodiscard]] bool scheduled()
  {
    return driver_scheduled();
  }

  /**
   * @brief Schedule an callback be be executed after the delay time
   *
   * If this is called and the timer has already scheduled an event (in other
   * words, `is_running()` returns true), then the previous scheduled event will
   * be canceled and the new scheduled event will be started.
   *
   * If the delay time result in a tick period of 0, then the timer will execute
   * after 1 tick period. For example, if the tick period is 1ms and the
   * requested time delay is 500us, then the event will be scheduled for 1ms.
   *
   * If the tick period is 1ms and the requested time is 2.5ms then the event
   * will be scheduled after 2 tick periods or in 2ms.
   *
   * @param p_callback - callback function to be called when the timer expires.
   * Pass nullptr to disable this timed interrupt
   * @param p_delay - the amount of time until the timer expires. If p_callback
   * is nullptr, this parameter is ignored.
   * @throws hal::argument_out_of_domain - if p_interval is greater than what
   * can be cannot be achieved.
   */
  void schedule(mem::optional_ptr<timed_callback> const& p_callback,
                time_duration p_delay)
  {
    driver_schedule(p_callback, p_delay);
  }

  virtual ~timed_interrupt() = default;

private:
  virtual bool driver_scheduled() = 0;
  virtual void driver_schedule(
    mem::optional_ptr<timed_callback> const& p_callback,
    time_duration p_delay) = 0;
};

}  // namespace hal::inline v5

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
// See the License for the specific language governing permissions and
// limitations under the License.

module;

#include <libhal/lifetime_macros.hpp>

export module hal:interrupts;

export import strong_ptr;
export import :units;
export import :lifetime;
export import :subscription;

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

/**
 * @brief Callback object invoked by an `edge_triggered_interrupt`
 *
 * The callback runs in interrupt context. It must be short, must not block,
 * and must not throw: an exception cannot unwind out of an interrupt service
 * routine. It must not call on_trigger(), or destroy its own subscription
 * or owner, from inside the callback. To change behavior, change the callback
 * object's own state.
 */
struct edge_triggered_callback
{
  /**
   * @brief The callback invoked after an edge trigger on a pin has occurred
   *
   * @param p_state - the current logical state of the pin
   */
  virtual void callback(bool p_state) noexcept = 0;

protected:
  ~edge_triggered_callback() = default;
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
 *
 * Contract: at most ONE subscriber at a time. Subscribing while one is active
 * throws hal::subscription_busy. Need several listeners on one pin? Put a
 * hal::edge_triggered_interrupt_multiplexer<N> on it and hand its slots out
 * instead.
 *
 * Interrupt context: on_trigger() must not be called from interrupt context,
 * including from inside a callback, and a subscription must not be destroyed
 * there either. To change what happens on an edge, change state inside the
 * callback itself rather than re-subscribing. This may be relaxed later.
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
   * @brief Register a callback for the rest of the program
   *
   * The callback lives forever and occupies the one subscriber slot for the
   * rest of the program. Any state transitions before this is called are lost.
   * Not callable from interrupt context (see class docs).
   *
   * @param p_callback - callback with static storage duration
   * @throws hal::subscription_busy - if a subscriber is already registered
   */
  void on_trigger(static_ref<edge_triggered_callback> p_callback)
  {
    driver_on_trigger(*p_callback);
  }

  /**
   * @brief Register a callback until the returned subscription is destroyed
   *
   * Any state transitions before this is called are lost. Not callable from
   * interrupt context, and the subscription must not be destroyed there (see
   * class docs).
   *
   * @param p_callback - callback to invoke when the trigger condition occurs
   * @return subscription - unregisters the callback when destroyed
   * @throws hal::subscription_busy - if a subscriber is already registered
   */
  [[nodiscard]] subscription on_trigger(
    edge_triggered_callback& p_callback HAL_LIFETIMEBOUND) HAL_LIFETIMEBOUND
  {
    driver_on_trigger(p_callback);
    return subscription::make<&edge_triggered_interrupt::driver_clear_trigger>(
      *this);
  }

protected:
  ~edge_triggered_interrupt() = default;

private:
  virtual void driver_configure(settings const& p_settings) = 0;
  /// Store the callback. Throw hal::subscription_busy if one is already set.
  virtual void driver_on_trigger(edge_triggered_callback& p_callback) = 0;
  /// Clear the callback. Called from a destructor; must not throw. The store
  /// must be atomic with respect to the ISR (single word store, or mask IRQ).
  virtual void driver_clear_trigger() noexcept = 0;
};

/**
 * @brief Callback object invoked by an `unstable_timed_interrupt`
 *
 * The callback runs in interrupt context. It must be short, must not block,
 * and must not throw: an exception cannot unwind out of an interrupt service
 * routine. It must not call schedule(), or destroy its own subscription or
 * owner, from inside the callback.
 */
struct timed_callback
{
  /**
   * @brief Callback invoked by interrupt after scheduled timed interrupt is
   * invoked
   *
   */
  virtual void callback() noexcept = 0;

protected:
  ~timed_callback() = default;
};

/**
 * @brief Determines whether a scheduled callback fires once or repeatedly
 */
enum class timer_mode : u8
{
  /// The callback is invoked a single time after the delay elapses, then
  /// the timer stops.
  one_shot,
  /// The callback is invoked after the delay elapses, then automatically
  /// re-armed with the same delay, repeating indefinitely until its
  /// subscription is destroyed.
  periodic
};

/**
 * @brief An abstraction for hardware timed interrupts. UNSTABLE.
 *
 * WARNING: this interface is expected to change soon, which is why its name
 * starts with `unstable_`. In particular, how a scheduled callback is replaced
 * by a new one is not settled.
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
 * Contract: at most ONE registered callback at a time. A callback stays
 * registered until its subscription is destroyed, even after a one-shot timer
 * has fired, so scheduling again while a subscription is alive throws
 * hal::subscription_busy. To schedule a new callback, destroy the previous
 * subscription first.
 *
 * Interrupt context: schedule() must not be called from interrupt context,
 * including from inside a callback, and a subscription must not be destroyed
 * there either.
 */
class unstable_timed_interrupt
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
   * @brief Schedule a callback for the rest of the program
   *
   * The callback lives forever and occupies the one registration slot for the
   * rest of the program, so this timer can't be scheduled again afterwards.
   * Not callable from interrupt context (see class docs).
   *
   * If the delay time result in a tick period of 0, then the timer will
   * execute after 1 tick period. For example, if the tick period is 1ms and
   * the requested time delay is 500us, then the event will be scheduled for
   * 1ms. If the tick period is 1ms and the requested time is 2.5ms then the
   * event will be scheduled after 2 tick periods or in 2ms.
   *
   * @param p_callback - callback with static storage duration
   * @param p_delay - the amount of time until the timer expires
   * @param p_mode - whether the callback fires once or repeatedly, see
   * `timer_mode`
   * @throws hal::subscription_busy - if a callback is already registered
   * @throws hal::argument_out_of_domain - if p_delay cannot be achieved
   */
  void schedule(static_ref<timed_callback> p_callback,
                time_duration p_delay,
                timer_mode p_mode = timer_mode::one_shot)
  {
    driver_schedule(*p_callback, p_delay, p_mode);
  }

  /**
   * @brief Schedule a callback until the returned subscription is destroyed
   *
   * Destroying the subscription cancels the timer if it is still pending.
   * Not callable from interrupt context, and the subscription must not be
   * destroyed there (see class docs). See the static_ref overload for how the
   * delay is rounded to the tick period.
   *
   * @param p_callback - callback to invoke when the timer expires
   * @param p_delay - the amount of time until the timer expires
   * @param p_mode - whether the callback fires once or repeatedly, see
   * `timer_mode`
   * @return subscription - cancels and unregisters the callback when destroyed
   * @throws hal::subscription_busy - if a callback is already registered
   * @throws hal::argument_out_of_domain - if p_delay cannot be achieved
   */
  [[nodiscard]] subscription schedule(timed_callback& p_callback
                                        HAL_LIFETIMEBOUND,
                                      time_duration p_delay,
                                      timer_mode p_mode = timer_mode::one_shot)
    HAL_LIFETIMEBOUND
  {
    driver_schedule(p_callback, p_delay, p_mode);
    return subscription::make<&unstable_timed_interrupt::driver_cancel>(*this);
  }

protected:
  ~unstable_timed_interrupt() = default;

private:
  virtual bool driver_scheduled() = 0;
  /// Store the callback and start the timer. Throw hal::subscription_busy if a
  /// callback is already registered.
  virtual void driver_schedule(timed_callback& p_callback,
                               time_duration p_delay,
                               timer_mode p_mode) = 0;
  /// Stop the timer and clear the callback. Called from a destructor; must not
  /// throw. The store must be atomic with respect to the ISR (single word
  /// store, or mask IRQ).
  virtual void driver_cancel() noexcept = 0;
};

}  // namespace hal::inline v5

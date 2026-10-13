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

export module hal:interrupts;

import :units;
import :lifetime;
import :subscription;

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
 * throws hal::subscription_busy.
 *
 * Interrupt context: never call on_trigger() from an interrupt service routine,
 * including from inside a callback, and never destroy a subscription there. To
 * change behavior, change state inside the callback object. To swap callbacks,
 * hold the subscription in a `std::optional<hal::subscription>`, then reset()
 * it and subscribe again from outside of interrupt context.
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
   * Any state transitions before this is called are lost.
   *
   * The registration is permanent: the subscriber slot stays taken for the rest
   * of the program.
   *
   * @warning Never call on_trigger() from an interrupt service routine,
   * including from inside a callback. It throws `hal::subscription_busy` if a
   * subscriber is already registered, and an exception cannot unwind out of
   * an interrupt service routine.
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
   * Any state transitions before this is called are lost.
   *
   * @warning Never call on_trigger() from an interrupt service routine,
   * including from inside a callback. It throws `hal::subscription_busy` if a
   * subscriber is already registered, and an exception cannot unwind out of
   * an interrupt service routine. Destroy the returned subscription outside
   * of interrupt context too.
   *
   * @param p_callback - callback to invoke when the trigger condition occurs
   * @return subscription - unregisters the callback when destroyed
   * @throws hal::subscription_busy - if a subscriber is already registered
   */
  [[nodiscard]] subscription on_trigger(edge_triggered_callback& p_callback
                                        [[clang::lifetimebound]])
    [[clang::lifetimebound]]
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
 * @brief Callback object invoked by a `timed_interrupt`
 *
 * The callback runs in interrupt context. It must be short, must not block,
 * and must not throw: an exception cannot unwind out of an interrupt service
 * routine. It may call `timed_interrupt::schedule()` and
 * `timed_interrupt::cancel()` to re-arm or stop the timer. It must not call
 * on_expire(), or destroy its own subscription or owner, from inside the
 * callback.
 */
struct timed_callback
{
  /**
   * @brief Invoked by the timer's interrupt each time the timer expires
   */
  virtual void callback() noexcept = 0;

protected:
  ~timed_callback() = default;
};

/**
 * @brief Determines whether an armed timer fires once or repeatedly
 */
enum class timer_mode : u8
{
  /// The timer expires a single time after the delay elapses, then stops.
  one_shot,
  /// The timer expires after the delay elapses, then automatically re-arms
  /// with the same delay, repeating until cancel() is called or the
  /// subscription is destroyed.
  periodic
};

/**
 * @brief An abstraction for hardware timed interrupts
 *
 * Use this interface for devices and peripherals that have timer like
 * capabilities, such that, when a timer's time has expired, an event,
 * interrupt, or signal is generated.
 *
 * Registering a callback and arming the timer are separate. on_expire()
 * registers the one callback the timer calls. schedule() and cancel() arm and
 * stop the timer as often as needed without touching the registration, so a
 * one-shot timer is re-armed simply by calling schedule() again.
 *
 * Timer drivers tick period must be an integer multiple of 1 nanosecond,
 * meaning that the only tick period allowed are 1ns, 2ns, up to the maximum
 * holdable in a std::chrono::nanosecond type. sub-nanosecond tick periods are
 * not allowed.
 *
 * Contract: at most ONE registered callback at a time. Registering while one
 * is active throws hal::subscription_busy.
 *
 * Interrupt context: schedule(), cancel() and scheduled() may be called from
 * an interrupt service routine, including from inside the callback, to re-arm
 * or stop the timer. Never call on_expire() from an interrupt service routine,
 * and never destroy a subscription there. To change behavior, change state
 * inside the callback object. To swap callbacks, hold the subscription in a
 * `std::optional<hal::subscription>`, then reset() it and register again from
 * outside of interrupt context.
 */
class timed_interrupt
{
public:
  /**
   * @brief Determine if the timer is currently armed
   *
   * Safe to call from interrupt context.
   *
   * @return true - if the timer is armed and has not expired yet, or is armed
   * in periodic mode. false otherwise.
   */
  [[nodiscard]] bool scheduled()
  {
    return driver_scheduled();
  }

  /**
   * @brief Arm the timer to expire after a delay
   *
   * If the timer is already armed, it is restarted with the new delay and
   * mode. The timer is armed even if no callback is registered; in that case
   * nothing is called when it expires.
   *
   * If the delay time result in a tick period of 0, then the timer will
   * execute after 1 tick period. For example, if the tick period is 1ms and
   * the requested time delay is 500us, then the event will be scheduled for
   * 1ms. If the tick period is 1ms and the requested time is 2.5ms then the
   * event will be scheduled after 2 tick periods or in 2ms.
   *
   * Safe to call from interrupt context, including from inside the callback,
   * as long as the delay is one the timer can achieve. An unachievable delay
   * throws, and an exception cannot unwind out of an interrupt service
   * routine.
   *
   * @param p_delay - the amount of time until the timer expires
   * @param p_mode - whether the timer expires once or repeatedly, see
   * `timer_mode`
   * @throws hal::argument_out_of_domain - if p_delay cannot be achieved
   */
  void schedule(time_duration p_delay, timer_mode p_mode = timer_mode::one_shot)
  {
    driver_schedule(p_delay, p_mode);
  }

  /**
   * @brief Stop the timer if it is armed
   *
   * Does nothing if the timer is not armed. The registered callback stays
   * registered. Safe to call from interrupt context, including from inside
   * the callback.
   */
  void cancel() noexcept
  {
    driver_cancel();
  }

  /**
   * @brief Register a callback for the rest of the program
   *
   * The registration is permanent: the callback slot stays taken for the rest
   * of the program. The timer can still be armed and stopped any number of
   * times with schedule() and cancel().
   *
   * @warning Never call on_expire() from an interrupt service routine,
   * including from inside a callback. It throws `hal::subscription_busy` if a
   * callback is already registered, and an exception cannot unwind out of an
   * interrupt service routine.
   *
   * @param p_callback - callback with static storage duration
   * @throws hal::subscription_busy - if a callback is already registered
   */
  void on_expire(static_ref<timed_callback> p_callback)
  {
    driver_on_expire(*p_callback);
  }

  /**
   * @brief Register a callback until the returned subscription is destroyed
   *
   * Destroying the subscription also cancels the timer if it is armed.
   *
   * @warning Never call on_expire() from an interrupt service routine,
   * including from inside a callback. It throws `hal::subscription_busy` if a
   * callback is already registered, and an exception cannot unwind out of an
   * interrupt service routine. Destroy the returned subscription outside of
   * interrupt context too.
   *
   * @param p_callback - callback to invoke each time the timer expires
   * @return subscription - cancels the timer and unregisters the callback when
   * destroyed
   * @throws hal::subscription_busy - if a callback is already registered
   */
  [[nodiscard]] subscription on_expire(timed_callback& p_callback
                                       [[clang::lifetimebound]])
    [[clang::lifetimebound]]
  {
    driver_on_expire(p_callback);
    return subscription::make<&timed_interrupt::driver_clear_expire>(*this);
  }

protected:
  ~timed_interrupt() = default;

private:
  virtual bool driver_scheduled() = 0;
  /// Arm or restart the timer. Must be callable from interrupt context.
  virtual void driver_schedule(time_duration p_delay, timer_mode p_mode) = 0;
  /// Stop the timer. Must be callable from interrupt context; must not throw.
  virtual void driver_cancel() noexcept = 0;
  /// Store the callback. Throw hal::subscription_busy if one is already set.
  virtual void driver_on_expire(timed_callback& p_callback) = 0;
  /// Stop the timer and clear the callback. Called from a destructor; must not
  /// throw. The store must be atomic with respect to the ISR (single word
  /// store, or mask IRQ).
  virtual void driver_clear_expire() noexcept = 0;
};
}  // namespace hal::inline v5

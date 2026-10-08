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

#include <chrono>
#include <exception>
#include <type_traits>
#include <utility>

export module hal:waiter;

export import :units;
export import :steady_clock;

namespace hal::inline v5 {
/**
 * @brief The notify-facing half of a waiter.
 *
 * Peripheral drivers never hold a waiter directly. They hold a
 * `hal::notifier`, which points at the waiter of whichever context is
 * currently blocked on the driver's operation. Whatever completes the
 * operation (an ISR, an RTOS deferred-interrupt task, a host thread) calls
 * `notify()` through that notifier.
 */
export class waiter
{
public:
  /**
   * @brief Wake the context blocked on this waiter.
   *
   * If no context is blocked yet, the notification latches and the next call
   * to `blocking_waiter::block()` or `blocking_waiter::block_for()` returns
   * immediately.
   *
   * Callable from any context: ISR, task, thread, or another core, as far as
   * the provider implementing this waiter supports it.
   */
  void notify() noexcept
  {
    driver_notify();
  }

protected:
  ~waiter() = default;

private:
  virtual void driver_notify() noexcept = 0;
};

/**
 * @brief The context-facing half of a waiter.
 *
 * Implemented by waiter providers: a WFI/sleep based provider on bare metal,
 * an RTOS provider, a fiber provider, or a std-based provider on hosts. Each
 * context of execution (the main loop, a task, a fiber, a thread) owns exactly
 * one blocking waiter, returned by `hal::current_waiter()` while that context
 * is running.
 *
 * # Lifetime
 *
 * A waiter must have static lifetime, or live for as long as the application
 * that uses it. An interrupt that captured a pointer to a waiter may notify it
 * after the operation that armed it has ended, so a waiter must never be
 * destroyed while any notifier could still reach it.
 *
 * # Timing granularity
 *
 * The resolution of every duration passed to this interface is defined by the
 * provider. A provider that can only wait in whole milliseconds waits in whole
 * milliseconds. That is the granularity the system provides.
 *
 * # Interrupt context
 *
 * None of the blocking APIs may be called from an interrupt service routine.
 * Doing so is a contract violation. Providers are encouraged to detect this
 * and terminate.
 */
export class blocking_waiter : public waiter
{
public:
  /**
   * @brief Block until notified, or return at once if a notify is latched.
   *
   * Returning from this function does not prove that the operation being
   * waited on has completed. A latched notification can be stale, left behind
   * by an interrupt that fired after an earlier operation ended. Callers must
   * re-check the completion state of their hardware after this returns and
   * block again if the operation is not done.
   */
  void block()
  {
    driver_block();
  }

  /**
   * @brief Block until notified or until the timeout has elapsed.
   *
   * Same semantics as `block()`, including the requirement to re-check
   * hardware state after returning, but gives up after `p_timeout`.
   *
   * @param p_timeout - the maximum amount of time to block for
   * @return true - a notification was received (or was already latched)
   * @return false - the timeout elapsed without a notification
   */
  [[nodiscard]] bool block_for(time_duration p_timeout)
  {
    return driver_block_for(p_timeout);
  }

  /**
   * @brief Suspend the calling context for a duration.
   *
   * Notifications do not wake the context early. Under a provider with other
   * contexts to run, the time is yielded to them.
   *
   * @param p_duration - the amount of time to sleep for
   */
  void sleep_for(time_duration p_duration)
  {
    driver_sleep_for(p_duration);
  }

  /**
   * @brief Discard any latched notification.
   *
   * Called by `hal::notifier::scope` when an operation ends, so a notification
   * that lands after a timeout cannot wake the next operation early.
   */
  void clear() noexcept
  {
    driver_clear();
  }

protected:
  ~blocking_waiter() = default;

private:
  virtual void driver_block() = 0;
  virtual bool driver_block_for(time_duration p_timeout) = 0;
  virtual void driver_sleep_for(time_duration p_duration) = 0;
  virtual void driver_clear() noexcept = 0;
};

/**
 * @brief Returns the calling context's blocking waiter.
 *
 * Installed by platforms and runtimes. A bare-metal platform typically returns
 * a single static waiter. An RTOS or fiber provider returns the waiter that
 * belongs to the currently running task or fiber.
 */
export using waiter_provider = blocking_waiter & (*)();

/// The installed waiter provider. nullptr until a platform installs one.
waiter_provider active_waiter_provider = nullptr;

/**
 * @brief Install a waiter provider.
 *
 * This is a plain store, not a synchronized one. Only call this before any
 * concurrency starts (scheduler, fibers, threads).
 *
 * @param p_provider - the provider to install
 * @return waiter_provider - the previously installed provider, or nullptr if
 * none was installed.
 */
export waiter_provider set_waiter_provider(waiter_provider p_provider) noexcept
{
  auto const previous = active_waiter_provider;
  active_waiter_provider = p_provider;
  return previous;
}

/**
 * @brief Get the installed waiter provider.
 *
 * @return waiter_provider - the installed provider, or nullptr if none has
 * been installed.
 */
export [[nodiscard]] waiter_provider get_waiter_provider() noexcept
{
  return active_waiter_provider;
}

/**
 * @brief The calling context's waiter, through the installed provider.
 *
 * With no provider installed, this terminates.
 *
 * @return blocking_waiter& - the waiter of the calling context
 */
export [[nodiscard]] blocking_waiter& current_waiter()
{
  if (active_waiter_provider == nullptr) [[unlikely]] {
    std::terminate();
  }
  return active_waiter_provider();
}

/**
 * @brief Yielding sleep for the calling context.
 *
 * Forwards to `current_waiter().sleep_for(p_duration)`. Use this for
 * millisecond-scale waits, such as waiting for a sensor conversion to finish.
 *
 * @param p_duration - the amount of time to sleep for
 */
export void sleep_for(time_duration p_duration)
{
  current_waiter().sleep_for(p_duration);
}

/**
 * @brief Spin the CPU on a steady clock without yielding to any other context.
 *
 * Only for microsecond-scale timing, such as bit-banging a protocol or a tight
 * polling loop. While this runs, no other context makes progress. Prefer
 * `hal::sleep_for` for anything longer.
 *
 * The wait is rounded up to the next tick of the clock, so it is never shorter
 * than requested.
 *
 * @param p_clock - the clock to measure the wait against
 * @param p_duration - the amount of time to spin for. Zero and negative
 * durations return immediately.
 */
export void busy_wait_without_yielding_for(steady_clock& p_clock,
                                           time_duration p_duration)
{
  if (p_duration <= time_duration::zero()) {
    return;
  }

  constexpr u64 nanoseconds_per_second = 1'000'000'000;
  auto const frequency = static_cast<u64>(
    p_clock.frequency().numerical_value_in(mp_units::si::hertz));
  auto const nanoseconds = static_cast<u64>(p_duration.count());
  auto const whole_seconds = nanoseconds / nanoseconds_per_second;
  auto const remainder = nanoseconds % nanoseconds_per_second;

  // Splitting off whole seconds keeps `remainder * frequency` below 2^63 for
  // any 32-bit frequency, so the tick calculation cannot overflow.
  auto const ticks = (whole_seconds * frequency) +
                     (((remainder * frequency) + nanoseconds_per_second - 1) /
                      nanoseconds_per_second);

  auto const start = p_clock.uptime();
  while (p_clock.uptime() - start < ticks) {
    continue;
  }
}

/**
 * @brief The one-pointer handle a peripheral driver owns.
 *
 * A notifier connects a driver's completion source (usually its ISR) to the
 * waiter of the context currently blocked on the driver. While no operation is
 * in progress, the notifier points at nothing and `notify()` does nothing.
 *
 * Usage inside a driver:
 *
 * @code
 * void my_uart::driver_write(mem::scatter_span<hal::byte const> p_data)
 * {
 *   hal::notifier::scope operation(m_notifier, [this]() noexcept {
 *     disable_tx_dma_interrupt();
 *   });
 *   start_tx_dma(p_data);
 *   while (not tx_dma_finished()) {
 *     operation.block();
 *   }
 * }
 *
 * void my_uart::tx_dma_isr()
 * {
 *   clear_tx_dma_flags();
 *   m_notifier.notify();
 * }
 * @endcode
 *
 * # Concurrency
 *
 * The slot is a volatile pointer, not an atomic one, so no atomic support is
 * required from the target. On a single core, an interrupt observes either the
 * old or the new pointer. Because waiters have static lifetime and drivers
 * re-check their hardware after every wakeup, both outcomes are safe: the
 * worst case is one extra trip around the driver's wait loop. Completing an
 * operation from another core or a host thread is not synchronized by the
 * notifier itself. Providers that support that are responsible for routing
 * such notifications safely.
 *
 * # Sharing
 *
 * A notifier serves one operation at a time. A driver shared between contexts
 * must serialize its operations, for example with a mutex. Starting a second
 * operation while one is in progress is a contract violation.
 */
export class notifier
{
public:
  /**
   * @brief RAII capture of the calling context's waiter for one operation.
   *
   * On construction, stores `current_waiter()` into the notifier. On
   * destruction, runs the disarm action, detaches the waiter from the
   * notifier, then clears any latched notification. Taking the disarm action
   * here, rather than relying on the driver's own guard objects, guarantees the
   * hardware is disarmed before the latch is cleared, regardless of the order
   * local variables are declared in.
   *
   * @tparam Disarm - a callable invoked with no arguments that stops the
   * hardware from notifying for this operation, typically by disabling an
   * interrupt. It must be noexcept because it runs in a destructor, including
   * during stack unwinding. Pass an empty lambda if the hardware disarms itself
   * on completion.
   */
  template<class Disarm>
  class scope
  {
  public:
    static_assert(std::is_nothrow_invocable_v<Disarm&>,
                  "The disarm action of a notifier::scope must be invocable "
                  "with no arguments and must be noexcept.");

    /**
     * @brief Capture the calling context's waiter into the notifier.
     *
     * Construct this before arming the hardware, so a completion that fires
     * immediately still reaches the waiter.
     *
     * The notifier must not already be in use by another operation. Violating
     * this is a contract violation.
     *
     * @param p_notifier - the driver's notifier
     * @param p_disarm - the action that disarms the hardware for this
     * operation
     */
    scope(notifier& p_notifier, Disarm p_disarm)
      : m_notifier(&p_notifier)
      , m_waiter(&current_waiter())
      , m_disarm(std::move(p_disarm))
    {
#if defined(__cpp_contracts)
      contract_assert(p_notifier.m_waiter == nullptr);
#else
      if (p_notifier.m_waiter != nullptr) [[unlikely]] {
        std::terminate();
      }
#endif
      m_notifier->m_waiter = m_waiter;
    }

    scope(scope const&) = delete;
    scope& operator=(scope const&) = delete;
    scope(scope&&) = delete;
    scope& operator=(scope&&) = delete;

    /**
     * @brief Disarm the hardware, detach the waiter, and clear the latch.
     *
     * The waiter is detached before the latch is cleared, so an interrupt
     * landing between the two cannot leave a stale notification behind.
     */
    ~scope()
    {
      m_disarm();
      m_notifier->m_waiter = nullptr;
      m_waiter->clear();
    }

    /**
     * @brief Block until notified.
     *
     * See `blocking_waiter::block()`. Re-check the hardware after this
     * returns.
     */
    void block()
    {
      m_waiter->block();
    }

    /**
     * @brief Block until notified or until the timeout elapses.
     *
     * See `blocking_waiter::block_for()`. Re-check the hardware after this
     * returns.
     *
     * @param p_timeout - the maximum amount of time to block for
     * @return true - a notification was received
     * @return false - the timeout elapsed without a notification
     */
    [[nodiscard]] bool block_for(time_duration p_timeout)
    {
      return m_waiter->block_for(p_timeout);
    }

  private:
    notifier* m_notifier;
    blocking_waiter* m_waiter;
    Disarm m_disarm;
  };

  /// Construct an idle notifier. `notify()` does nothing until an operation
  /// begins.
  notifier() noexcept = default;

  notifier(notifier const&) = delete;
  notifier& operator=(notifier const&) = delete;
  notifier(notifier&&) = delete;
  notifier& operator=(notifier&&) = delete;
  ~notifier() = default;

  /**
   * @brief Notify the waiter of the operation in progress, if any.
   *
   * Callable from any context, including ISRs. Does nothing if no operation is
   * in progress.
   */
  void notify() noexcept
  {
    // Read the slot exactly once. The volatile qualifier keeps the compiler
    // from re-reading it between the null check and the call.
    waiter* const target = m_waiter;
    if (target != nullptr) {
      target->notify();
    }
  }

private:
  waiter* volatile m_waiter = nullptr;
};
}  // namespace hal::inline v5

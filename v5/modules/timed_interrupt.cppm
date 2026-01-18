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

export module hal:timed_interrupt;

export import strong_ptr;
export import :units;

export namespace hal::inline v5 {
/**
 * @brief Disambiguation tag object for timer callbacks
 *
 */
struct timed_interrupt_schedule_tag
{};

struct timed_interrupt_callback
{
  virtual void schedule(timed_interrupt_schedule_tag) = 0;
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
  void schedule(mem::optional_ptr<timed_interrupt_callback> const& p_callback,
                time_duration p_delay)
  {
    driver_schedule(p_callback, p_delay);
  }

  virtual ~timed_interrupt() = default;

private:
  virtual bool driver_scheduled() = 0;
  virtual void driver_schedule(
    mem::optional_ptr<timed_interrupt_callback> const& p_callback,
    time_duration p_delay) = 0;
};
}  // namespace hal::inline v5

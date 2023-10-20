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

#include <chrono>

#include "functional.hpp"
#include "units.hpp"

namespace hal {
/**
 * @brief Timer hardware abstraction interface.
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
class timer
{
public:
  /**
   * @brief Determine if the timer is currently running
   *
   * @return true - if a callback has been scheduled and has not been invoked
   * yet, false otherwise.
   */
  [[nodiscard]] bool is_running()
  {
    return driver_is_running();
  }

  /**
   * @brief Stops a scheduled event from happening.
   *
   * Does nothing if the timer is not currently running.
   *
   * Note that there must be sufficient time between the this call finishing and
   * the scheduled event's termination. If this call is too close to when the
   * schedule event expires, this function may not complete before the hardware
   * calls the callback.
   */
  void cancel()
  {
    driver_cancel();
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
   * @param p_callback - callback function to be called when the timer expires
   * @param p_delay - the amount of time until the timer expires
   * @throws hal::argument_out_of_domain - if p_interval is greater than what
   * can be cannot be achieved.
   */
  void schedule(hal::callback<void(void)> p_callback,
                hal::time_duration p_delay)
  {
    driver_schedule(p_callback, p_delay);
  }

  virtual ~timer() = default;

private:
  virtual bool driver_is_running() = 0;
  virtual void driver_cancel() = 0;
  virtual void driver_schedule(hal::callback<void(void)> p_callback,
                               hal::time_duration p_delay) = 0;
};
}  // namespace hal

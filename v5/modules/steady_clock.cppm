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

export module hal:steady_clock;

export import async_context;
export import :units;

export namespace hal {
/**
 * @brief Hardware abstraction interface for a steady clock mechanism
 *
 * Implementations of this interface must follow the same requirements as a
 * std::chrono::steady_clock, in that the clock is monotonic & steady. An
 * additional requirement is added to ensure that the clock is reliable. Meaning
 * calls to the interface functions do not return errors because this clock
 * should be infallible. To ensure this, this clock should be driven by the
 * platform's peripheral drivers or some other mechanism that is unlikely to go
 * offline while the platform is in a normal operating state.
 *
 * This clock is steady meaning that subsequent calls to get the uptime of this
 * clock cannot decrease as physical time moves forward and the time between
 * ticks of this clock are constant and defined by the clock's frequency.
 *
 * This can be used to get the time since the boot up, or to be more accurate,
 * the time when the steady clock object is created. This clock is most suitable
 * for measuring time intervals.
 *
 * After creation of this clock, the operating frequency shall not change.
 */
class steady_clock
{
public:
  /**
   * @brief Get the operating frequency of the steady clock
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<hertz> - operating frequency of the steady clock.
   * Guaranteed to be a positive value by the implementing driver.
   */
  [[nodiscard]] async::future<hertz> frequency(async::context& p_context)
  {
    return driver_frequency(p_context);
  }

  /**
   * @brief Get the current value of the steady clock
   *
   * @param p_context - async context for coroutine suspension and resumption.
   * @return async::future<u64> - Number of counts that the steady clock has
   * counted since it started.
   */
  [[nodiscard]] async::future<u64> uptime(async::context& p_context)
  {
    return driver_uptime(p_context);
  }

  virtual ~steady_clock() = default;

private:
  virtual async::future<hertz> driver_frequency(async::context& p_context) = 0;
  virtual async::future<u64> driver_uptime(async::context& p_context) = 0;
};
}  // namespace hal

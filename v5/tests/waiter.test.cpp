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

#include <chrono>
#include <utility>
#include <vector>

#include <boost/ut.hpp>

import hal;

using namespace std::chrono_literals;
using namespace mp_units::si::unit_symbols;

namespace {
/// A single-threaded waiter that records every call made to it. block() and
/// block_for() consume a latched notification if one exists.
class recording_waiter : public hal::blocking_waiter
{
public:
  bool latched = false;
  int notify_count = 0;
  int block_count = 0;
  int clear_count = 0;
  std::vector<hal::time_duration> block_for_timeouts;
  std::vector<hal::time_duration> sleeps;

  /// Invoked from inside block()/block_for() to simulate an interrupt firing
  /// while the context is blocked.
  void (*on_block)(recording_waiter&) = nullptr;

private:
  void driver_notify() noexcept override
  {
    notify_count++;
    latched = true;
  }

  void driver_block() override
  {
    block_count++;
    if (on_block != nullptr) {
      on_block(*this);
    }
    latched = false;
  }

  bool driver_block_for(hal::time_duration p_timeout) override
  {
    block_for_timeouts.push_back(p_timeout);
    if (on_block != nullptr) {
      on_block(*this);
    }
    return std::exchange(latched, false);
  }

  void driver_sleep_for(hal::time_duration p_duration) override
  {
    sleeps.push_back(p_duration);
  }

  void driver_clear() noexcept override
  {
    clear_count++;
    latched = false;
  }
};

recording_waiter test_waiter;

hal::blocking_waiter& test_provider()
{
  return test_waiter;
}

/// Installs the test provider for the lifetime of a test and restores the
/// previous provider afterwards.
struct provider_guard
{
  provider_guard()
    : previous(hal::set_waiter_provider(&test_provider))
  {
    test_waiter = recording_waiter{};
  }

  ~provider_guard()
  {
    hal::set_waiter_provider(previous);
  }

  hal::waiter_provider previous;
};

/// A steady clock that advances one tick each time uptime() is read.
class stepping_clock : public hal::steady_clock
{
public:
  hal::hertz tick_frequency = 1 * MHz;
  hal::u64 ticks = 0;
  hal::u64 reads = 0;

private:
  hal::hertz driver_frequency() override
  {
    return tick_frequency;
  }

  hal::u64 driver_uptime() override
  {
    reads++;
    return ticks++;
  }
};

void waiter_provider_test() noexcept
{
  using namespace boost::ut;

  "set_waiter_provider() returns the previous provider"_test = []() {
    // Setup
    auto const original = hal::get_waiter_provider();

    // Exercise
    auto const previous = hal::set_waiter_provider(&test_provider);
    auto const installed = hal::get_waiter_provider();
    auto const restored_from = hal::set_waiter_provider(original);

    // Verify
    expect(previous == original);
    expect(installed == &test_provider);
    expect(restored_from == &test_provider);
    expect(hal::get_waiter_provider() == original);
  };

  "current_waiter() returns the provider's waiter"_test = []() {
    // Setup
    provider_guard guard;

    // Exercise
    auto& waiter = hal::current_waiter();

    // Verify
    expect(&waiter == static_cast<hal::blocking_waiter*>(&test_waiter));
  };

  "sleep_for() forwards to the current waiter"_test = []() {
    // Setup
    provider_guard guard;

    // Exercise
    hal::sleep_for(5ms);

    // Verify
    expect(that % 1 == test_waiter.sleeps.size());
    expect(test_waiter.sleeps.at(0) == 5ms);
    expect(that % 0 == test_waiter.block_count);
  };
}

void notifier_test() noexcept
{
  using namespace boost::ut;

  "notify() with no operation in progress does nothing"_test = []() {
    // Setup
    provider_guard guard;
    hal::notifier notifier;

    // Exercise
    notifier.notify();

    // Verify
    expect(that % 0 == test_waiter.notify_count);
  };

  "notify() reaches the waiter while a scope is active"_test = []() {
    // Setup
    provider_guard guard;
    hal::notifier notifier;
    hal::notifier::scope operation(notifier, []() noexcept {});

    // Exercise
    notifier.notify();

    // Verify
    expect(that % 1 == test_waiter.notify_count);
    expect(test_waiter.latched);
  };

  "scope destructor disarms, detaches, and clears the latch"_test = []() {
    // Setup
    provider_guard guard;
    hal::notifier notifier;
    int disarm_count = 0;

    // Exercise
    {
      hal::notifier::scope operation(
        notifier, [&disarm_count]() noexcept { disarm_count++; });
      notifier.notify();
    }
    notifier.notify();

    // Verify
    expect(that % 1 == disarm_count);
    expect(that % 1 == test_waiter.clear_count);
    expect(not test_waiter.latched);
    // The second notify happened after the scope ended and must not reach the
    // waiter.
    expect(that % 1 == test_waiter.notify_count);
  };

  "scope can be reused for back to back operations"_test = []() {
    // Setup
    provider_guard guard;
    hal::notifier notifier;

    // Exercise
    {
      hal::notifier::scope operation(notifier, []() noexcept {});
    }
    {
      hal::notifier::scope operation(notifier, []() noexcept {});
      notifier.notify();
    }

    // Verify
    expect(that % 1 == test_waiter.notify_count);
    expect(that % 2 == test_waiter.clear_count);
  };

  "scope::block() waits on the captured waiter"_test = []() {
    // Setup
    provider_guard guard;
    static hal::notifier notifier;
    static bool operation_done = false;
    operation_done = false;
    test_waiter.on_block = [](recording_waiter&) {
      // Simulate the ISR completing the operation while blocked
      operation_done = true;
      notifier.notify();
    };

    // Exercise
    {
      hal::notifier::scope operation(notifier, []() noexcept {});
      while (not operation_done) {
        operation.block();
      }
    }

    // Verify
    expect(that % 1 == test_waiter.block_count);
    expect(that % 1 == test_waiter.notify_count);
  };

  "scope::block_for() forwards the timeout and reports the result"_test = []() {
    // Setup
    provider_guard guard;
    hal::notifier notifier;
    hal::notifier::scope operation(notifier, []() noexcept {});

    // Exercise
    bool const timed_out = not operation.block_for(10ms);
    notifier.notify();
    bool const notified = operation.block_for(20ms);

    // Verify
    expect(timed_out);
    expect(notified);
    expect(that % 2 == test_waiter.block_for_timeouts.size());
    expect(test_waiter.block_for_timeouts.at(0) == 10ms);
    expect(test_waiter.block_for_timeouts.at(1) == 20ms);
  };
}

void busy_wait_test() noexcept
{
  using namespace boost::ut;

  "busy wait spins for the requested number of ticks"_test = []() {
    // Setup
    stepping_clock clock;  // 1 MHz: 1 tick per microsecond

    // Exercise
    hal::busy_wait_without_yielding_for(clock, 10us);

    // Verify
    // One read to capture the start, then reads until 10 ticks have passed.
    expect(that % 11 == clock.reads);
  };

  "busy wait rounds partial ticks up"_test = []() {
    // Setup
    stepping_clock clock;  // 1 MHz: 1 tick per microsecond

    // Exercise
    hal::busy_wait_without_yielding_for(clock, 1500ns);

    // Verify
    // 1.5 ticks rounds up to 2 ticks: one start read plus two waiting reads.
    expect(that % 3 == clock.reads);
  };

  "busy wait handles durations longer than a second"_test = []() {
    // Setup
    stepping_clock clock;
    clock.tick_frequency = 10 * Hz;

    // Exercise
    hal::busy_wait_without_yielding_for(clock, 2500ms);

    // Verify
    // 2.5 seconds at 10 Hz is 25 ticks.
    expect(that % 26 == clock.reads);
  };

  "busy wait with zero or negative duration returns immediately"_test = []() {
    // Setup
    stepping_clock clock;

    // Exercise
    hal::busy_wait_without_yielding_for(clock, 0ns);
    hal::busy_wait_without_yielding_for(clock, -5ms);

    // Verify
    expect(that % 0 == clock.reads);
  };
}
}  // namespace

int main()
{
  waiter_provider_test();
  notifier_test();
  busy_wait_test();
}

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

#include <optional>
#include <ostream>

#include <boost/ut.hpp>

import hal;

namespace hal {
std::ostream& operator<<(std::ostream& p_ostream, timer_mode p_mode)
{
  switch (p_mode) {
    case timer_mode::one_shot:
      return p_ostream << "timer_mode::one_shot";
    case timer_mode::periodic:
      return p_ostream << "timer_mode::periodic";
  }
  return p_ostream << "timer_mode::<unknown>";
}
}  // namespace hal

namespace {

class test_callback : public hal::timed_callback
{
public:
  int call_count = 0;

  void callback() noexcept override
  {
    call_count++;
  }
};

// Follows the single-subscriber contract: one registered callback at a time.
class test_timed_interrupt final : public hal::unstable_timed_interrupt
{
public:
  hal::timed_callback* stored_callback = nullptr;
  hal::time_duration stored_delay{ 0 };
  std::optional<hal::timer_mode> m_mode;

  // Simulates the hardware timer expiring.
  void fire()
  {
    if (stored_callback) {
      stored_callback->callback();
    }
  }

private:
  bool driver_scheduled() override
  {
    return stored_callback != nullptr;
  }

  void driver_schedule(hal::timed_callback& p_callback,
                       hal::time_duration p_delay,
                       hal::timer_mode p_mode) override
  {
    if (stored_callback) {
      throw hal::subscription_busy{};
    }
    stored_callback = &p_callback;
    stored_delay = p_delay;
    m_mode = p_mode;
  }

  void driver_cancel() noexcept override
  {
    stored_callback = nullptr;
  }
};

void timed_interrupt_test() noexcept
{
  using namespace boost::ut;

  "::scheduled() - false by default"_test = []() {
    // Setup
    test_timed_interrupt test;

    // Exercise
    auto result = test.scheduled();

    // Verify
    expect(!result);
  };

  "::schedule() - stores callback and delay"_test = []() {
    // Setup
    test_timed_interrupt test;
    test_callback callback;
    hal::time_duration delay{ 1000 };

    // Exercise
    auto subscription = test.schedule(callback, delay);
    test.fire();

    // Verify
    expect(&callback == test.stored_callback);
    expect(that % delay == test.stored_delay);
    expect(test.scheduled());
    expect(that % 1 == callback.call_count);
  };

  "::schedule() - destroying the subscription cancels"_test = []() {
    // Setup
    test_timed_interrupt test;
    test_callback callback;

    // Exercise
    {
      auto subscription = test.schedule(callback, hal::time_duration{ 1000 });
    }
    test.fire();

    // Verify
    expect(!test.scheduled());
    expect(that % 0 == callback.call_count);
  };

  "::schedule() - defaults to one_shot mode"_test = []() {
    // Setup
    test_timed_interrupt test;
    test_callback callback;

    // Exercise
    auto subscription = test.schedule(callback, hal::time_duration{ 1000 });

    // Verify
    expect(that % hal::timer_mode::one_shot == test.m_mode.value());
  };

  "::schedule() - with periodic mode"_test = []() {
    // Setup
    test_timed_interrupt test;
    test_callback callback;

    // Exercise
    auto subscription = test.schedule(
      callback, hal::time_duration{ 1000 }, hal::timer_mode::periodic);

    // Verify
    expect(that % hal::timer_mode::periodic == test.m_mode.value());
  };

  "::schedule() - throws while a callback is registered"_test = []() {
    // Setup
    test_timed_interrupt test;
    test_callback callback1;
    test_callback callback2;
    auto subscription = test.schedule(callback1, hal::time_duration{ 1000 });

    // Exercise
    bool threw = false;
    try {
      auto subscription2 = test.schedule(callback2, hal::time_duration{ 2000 });
    } catch (hal::subscription_busy const&) {
      threw = true;
    }

    // Verify
    expect(threw);
    expect(&callback1 == test.stored_callback);
  };

  "::schedule() - reschedule after the subscription ends"_test = []() {
    // Setup
    test_timed_interrupt test;
    test_callback callback1;
    test_callback callback2;
    {
      auto subscription = test.schedule(callback1, hal::time_duration{ 1000 });
    }

    // Exercise
    auto subscription = test.schedule(callback2, hal::time_duration{ 2000 });

    // Verify
    expect(&callback2 == test.stored_callback);
    expect(that % hal::time_duration{ 2000 } == test.stored_delay);
  };
};

}  // namespace

int main()
{
  timed_interrupt_test();
}

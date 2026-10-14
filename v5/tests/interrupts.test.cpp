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

// Follows the contract: one registered callback, armed independently.
class test_timed_interrupt final : public hal::timed_interrupt
{
public:
  hal::timed_callback* stored_callback = nullptr;
  hal::time_duration stored_delay{ 0 };
  std::optional<hal::timer_mode> m_mode;
  bool armed = false;

  // Simulates the hardware timer expiring.
  void fire()
  {
    if (not armed) {
      return;
    }
    if (m_mode == hal::timer_mode::one_shot) {
      armed = false;
    }
    if (stored_callback) {
      stored_callback->callback();
    }
  }

private:
  bool driver_scheduled() override
  {
    return armed;
  }

  void driver_schedule(hal::time_duration p_delay,
                       hal::timer_mode p_mode) override
  {
    stored_delay = p_delay;
    m_mode = p_mode;
    armed = true;
  }

  void driver_cancel() noexcept override
  {
    armed = false;
  }

  void driver_on_expire(hal::timed_callback& p_callback) override
  {
    if (stored_callback) {
      throw hal::subscription_busy(this);
    }
    stored_callback = &p_callback;
  }

  void driver_clear_expire() noexcept override
  {
    armed = false;
    stored_callback = nullptr;
  }
};

// Re-arms its own one-shot timer from inside the callback, a fixed number of
// times.
class rearming_callback : public hal::timed_callback
{
public:
  explicit rearming_callback(hal::timed_interrupt& p_timer)
    : m_timer(&p_timer)
  {
  }

  int call_count = 0;
  int rearm_limit = 2;

private:
  void callback() noexcept override
  {
    call_count++;
    if (call_count <= rearm_limit) {
      m_timer->schedule(hal::time_duration{ 500 });
    }
  }

  hal::timed_interrupt* m_timer;
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

  "::schedule() - arms the timer with the delay"_test = []() {
    // Setup
    test_timed_interrupt test;
    test_callback callback;
    auto subscription = test.on_expire(callback);
    hal::time_duration delay{ 1000 };

    // Exercise
    test.schedule(delay);
    bool const armed = test.scheduled();
    test.fire();

    // Verify
    expect(armed);
    expect(that % delay == test.stored_delay);
    expect(that % 1 == callback.call_count);
    expect(!test.scheduled());  // one-shot stops after expiring
  };

  "::schedule() - defaults to one_shot mode"_test = []() {
    // Setup
    test_timed_interrupt test;

    // Exercise
    test.schedule(hal::time_duration{ 1000 });

    // Verify
    expect(that % hal::timer_mode::one_shot == test.m_mode.value());
  };

  "::schedule() - periodic keeps firing until cancel()"_test = []() {
    // Setup
    test_timed_interrupt test;
    test_callback callback;
    auto subscription = test.on_expire(callback);

    // Exercise
    test.schedule(hal::time_duration{ 1000 }, hal::timer_mode::periodic);
    test.fire();
    test.fire();
    test.cancel();
    test.fire();

    // Verify
    expect(that % hal::timer_mode::periodic == test.m_mode.value());
    expect(that % 2 == callback.call_count);
    expect(!test.scheduled());
  };

  "::schedule() - re-arms a one-shot timer without re-registering"_test = []() {
    // Setup
    test_timed_interrupt test;
    test_callback callback;
    auto subscription = test.on_expire(callback);

    // Exercise
    test.schedule(hal::time_duration{ 1000 });
    test.fire();
    test.schedule(hal::time_duration{ 2000 });
    test.fire();

    // Verify
    expect(that % 2 == callback.call_count);
    expect(that % hal::time_duration{ 2000 } == test.stored_delay);
    expect(&callback == test.stored_callback);
  };

  "::schedule() - while armed restarts with the new delay"_test = []() {
    // Setup
    test_timed_interrupt test;
    test.schedule(hal::time_duration{ 1000 }, hal::timer_mode::periodic);

    // Exercise
    test.schedule(hal::time_duration{ 3000 });

    // Verify
    expect(test.scheduled());
    expect(that % hal::time_duration{ 3000 } == test.stored_delay);
    expect(that % hal::timer_mode::one_shot == test.m_mode.value());
  };

  "::schedule() - callback can re-arm its own timer"_test = []() {
    // Setup
    test_timed_interrupt test;
    rearming_callback callback(test);
    auto subscription = test.on_expire(callback);

    // Exercise
    test.schedule(hal::time_duration{ 500 });
    test.fire();
    test.fire();
    test.fire();
    test.fire();  // the third call did not re-arm

    // Verify
    expect(that % 3 == callback.call_count);
    expect(!test.scheduled());
  };

  "::schedule() - without a callback arms and fires into nothing"_test = []() {
    // Setup
    test_timed_interrupt test;

    // Exercise
    test.schedule(hal::time_duration{ 1000 });
    test.fire();

    // Verify
    expect(!test.scheduled());
  };

  "::on_expire() - throws while a callback is registered"_test = []() {
    // Setup
    test_timed_interrupt test;
    test_callback callback1;
    test_callback callback2;
    auto subscription = test.on_expire(callback1);

    // Exercise
    bool threw = false;
    try {
      auto subscription2 = test.on_expire(callback2);
    } catch (hal::subscription_busy const&) {
      threw = true;
    }

    // Verify
    expect(threw);
    expect(&callback1 == test.stored_callback);
  };

  "::on_expire() - destroying the subscription cancels the timer"_test = []() {
    // Setup
    test_timed_interrupt test;
    test_callback callback;

    // Exercise
    {
      auto subscription = test.on_expire(callback);
      test.schedule(hal::time_duration{ 1000 });
    }
    test.fire();

    // Verify
    expect(!test.scheduled());
    expect(nullptr == test.stored_callback);
    expect(that % 0 == callback.call_count);
  };

  "::on_expire() - register again after the subscription ends"_test = []() {
    // Setup
    test_timed_interrupt test;
    test_callback callback1;
    test_callback callback2;
    {
      auto subscription = test.on_expire(callback1);
    }

    // Exercise
    auto subscription = test.on_expire(callback2);
    test.schedule(hal::time_duration{ 2000 });
    test.fire();

    // Verify
    expect(&callback2 == test.stored_callback);
    expect(that % 0 == callback1.call_count);
    expect(that % 1 == callback2.call_count);
  };
};

}  // namespace

int main()
{
  timed_interrupt_test();
}

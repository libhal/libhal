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

#include <boost/ut.hpp>
#include <memory_resource>

import hal;

namespace {

class test_callback : public hal::timed_interrupt_callback
{
public:
  int call_count = 0;

  void schedule() override
  {
    call_count++;
  }
};

class test_timed_interrupt : public hal::timed_interrupt
{
public:
  bool is_scheduled = false;
  mem::optional_ptr<hal::timed_interrupt_callback> stored_callback;
  hal::time_duration stored_delay{ 0 };

  ~test_timed_interrupt() override = default;

private:
  bool driver_scheduled() override
  {
    return is_scheduled;
  }

  void driver_schedule(
    mem::optional_ptr<hal::timed_interrupt_callback> const& p_callback,
    hal::time_duration p_delay) override
  {
    stored_callback = p_callback;
    stored_delay = p_delay;
    is_scheduled = p_callback.has_value();
  }
};

boost::ut::suite<"hal::timed_interrupt"> timed_interrupt_test = []() {
  using namespace boost::ut;

  "::scheduled() - false by default"_test = []() {
    // Setup
    test_timed_interrupt test;

    // Exercise
    auto result = test.scheduled();

    // Verify
    expect(!result);
  };

  "::schedule() - with callback"_test = []() {
    // Setup
    test_timed_interrupt test;
    auto callback =
      mem::make_strong_ptr<test_callback>(std::pmr::new_delete_resource());
    hal::time_duration delay{ 1000 };

    // Exercise
    test.schedule(callback, delay);

    // Verify
    expect(test.stored_callback.has_value());
    expect(callback == test.stored_callback.value());
    expect(that % delay == test.stored_delay);
    expect(test.is_scheduled);
  };

  "::schedule() - with nullptr"_test = []() {
    // Setup
    test_timed_interrupt test;
    auto callback =
      mem::make_strong_ptr<test_callback>(std::pmr::new_delete_resource());
    test.schedule(callback, hal::time_duration{ 1000 });

    // Exercise
    test.schedule(nullptr, hal::time_duration{ 500 });

    // Verify
    expect(!test.stored_callback.has_value());
    expect(!test.is_scheduled);
  };

  "::scheduled() - true after scheduling"_test = []() {
    // Setup
    test_timed_interrupt test;
    auto callback =
      mem::make_strong_ptr<test_callback>(std::pmr::new_delete_resource());
    test.schedule(callback, hal::time_duration{ 1000 });

    // Exercise
    auto result = test.scheduled();

    // Verify
    expect(result);
  };

  "::schedule() - reschedule replaces previous"_test = []() {
    // Setup
    test_timed_interrupt test;
    auto callback1 =
      mem::make_strong_ptr<test_callback>(std::pmr::new_delete_resource());
    auto callback2 =
      mem::make_strong_ptr<test_callback>(std::pmr::new_delete_resource());
    hal::time_duration delay1{ 1000 };
    hal::time_duration delay2{ 2000 };

    test.schedule(callback1, delay1);

    // Exercise
    test.schedule(callback2, delay2);

    // Verify
    expect(test.stored_callback.has_value());
    expect(callback2 == test.stored_callback.value());
    expect(that % delay2 == test.stored_delay);
    expect(test.is_scheduled);
  };
};

}  // namespace

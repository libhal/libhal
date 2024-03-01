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

#include <libhal/timer.hpp>

#include <functional>

#include <libhal/error.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
class test_timer : public hal::timer
{
public:
  bool m_is_running{ false };
  hal::callback<void(void)> m_callback = []() {};
  hal::time_duration m_delay;

private:
  bool driver_is_running() override
  {
    return m_is_running;
  };

  void driver_cancel() override
  {
    m_is_running = false;
  };

  // NOLINTNEXTLINE
  void driver_schedule(hal::callback<void(void)> p_callback,
                       hal::time_duration p_delay) override
  {
    m_is_running = true;
    m_callback = p_callback;
    m_delay = p_delay;
  };
};
}  // namespace

void timer_test()
{
  using namespace boost::ut;
  "timer interface test"_test = []() {
    // Setup
    test_timer test;
    bool callback_stored_successfully = false;
    const hal::callback<void(void)> expected_callback =
      [&callback_stored_successfully]() {
        callback_stored_successfully = true;
      };
    const hal::time_duration expected_delay = {};

    // Exercise + Verify
    auto is_running = test.is_running();
    expect(that % false == is_running);

    test.schedule(expected_callback, expected_delay);
    is_running = test.is_running();
    expect(that % true == is_running);
    expect(expected_delay == test.m_delay);

    test.m_callback();
    expect(that % true == callback_stored_successfully);

    test.cancel();
    is_running = test.is_running();
    expect(that % false == is_running);
  };
};
}  // namespace hal

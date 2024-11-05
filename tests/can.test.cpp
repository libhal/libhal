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

#include <libhal/can.hpp>

#include <libhal/error.hpp>
#include <libhal/functional.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
constexpr hal::can::settings expected_settings{ .baud_rate = 1.0_MHz };
constexpr hal::can::message_t expected_message{ .id = 22,
                                             .payload = { 0xCC, 0xDD, 0xEE, },
                                             .length = 3 };

class test_can : public hal::can
{
public:
  can::settings m_settings{};
  can::message_t m_message{};
  bool m_bus_on_called{ false };
  hal::callback<handler> m_handler{};
  std::size_t m_cursor = 0;

  ~test_can() override = default;

private:
  void driver_configure(can::settings const& p_settings) override
  {
    m_settings = p_settings;
  }

  void driver_bus_on() override
  {
    m_bus_on_called = true;
  }

  void driver_send(can::message_t const& p_message) override
  {
    m_message = p_message;
  }

  void driver_on_receive(hal::callback<handler> p_handler) override
  {
    m_handler = p_handler;
  }
};
}  // namespace

boost::ut::suite<"can_test"> can_test = []() {
  using namespace boost::ut;

  "::configure()"_test = []() {
    // Setup
    test_can test;

    // Ensure
    expect(expected_settings != test.m_settings);

    // Exercise
    test.configure(expected_settings);

    // Verify
    expect(expected_settings == test.m_settings);
  };

  "::send()"_test = []() {
    // Setup
    test_can test;

    // Ensure
    expect(expected_message != test.m_message);

    // Exercise
    test.send(expected_message);

    // Verify
    expect(expected_message == test.m_message);
  };

  "::bus_on()"_test = []() {
    // Setup
    test_can test;

    // Ensure
    expect(that % not test.m_bus_on_called);

    // Exercise
    test.bus_on();

    // Verify
    expect(that % test.m_bus_on_called);
  };

  "::on_receive()"_test = []() {
    // Setup
    test_can test;
    bool callback_called = false;

    // Ensure
    test.m_handler({});
    expect(that % not callback_called);

    // Exercise
    // Exercise: Simulate incoming message
    test.on_receive([&callback_called](hal::can::message_t const&) {
      callback_called = true;
    });
    test.m_handler({});

    // Verify
    expect(that % callback_called);
  };
};
}  // namespace hal

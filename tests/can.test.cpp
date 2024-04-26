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

#include <boost/ut.hpp>

namespace hal {
namespace {
constexpr hal::can::settings expected_settings{
  .baud_rate = 1.0_Hz,
};
int counter = 0;
constexpr hal::can::message_t expected_message{ .id = 1, .length = 0 };
hal::callback<hal::can::handler> expected_handler =
  [](const hal::can::message_t&) { counter++; };

class test_can : public hal::can
{
public:
  settings m_settings{};
  message_t m_message{};
  hal::callback<handler> m_handler = [](const message_t&) {};
  bool m_bus_on_called{ false };
  ~test_can() override = default;

private:
  void driver_configure(const settings& p_settings) override
  {
    m_settings = p_settings;
  };

  void driver_bus_on() override
  {
    m_bus_on_called = true;
  }

  void driver_send(const message_t& p_message) override
  {
    m_message = p_message;
  };

  void driver_on_receive(hal::callback<handler> p_handler) override
  {
    m_handler = p_handler;
  };
};
}  // namespace

void can_test()
{
  using namespace boost::ut;

  "can interface test"_test = []() {
    // Setup
    test_can test;

    // Exercise
    test.configure(expected_settings);
    test.send(expected_message);
    test.on_receive(expected_handler);
    test.m_handler(expected_message);

    // Verify
    expect(that % expected_settings.baud_rate == test.m_settings.baud_rate);
    expect(that % expected_message.id == test.m_message.id);
    expect(that % 1 == counter);
  };
};
}  // namespace hal

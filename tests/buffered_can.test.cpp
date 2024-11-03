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

#include <libhal/buffered_can.hpp>

#include <libhal/error.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
constexpr hal::can_settings expected_settings{ .baud_rate = 1.0_MHz };
constexpr hal::can_message expected_message{ .id = 10, .length = 0 };
constexpr hal::can_message expected_message2{ .id = 11,
                                              .payload = { 0xAA, 0xBB },
                                              .length = 2 };
constexpr hal::can_message expected_message3{ .id = 22,
                                              .payload = { 0xCC, 0xDD, 0xEE, },
                                              .length = 3 };

class test_can : public hal::buffered_can
{
public:
  can_settings m_settings{};
  can_message m_message{};
  bool m_bus_on_called{ false };
  std::array<can_message, 16> m_message_buffer{};
  std::size_t m_cursor = 0;

  ~test_can() override = default;

  void add_message_to_buffer(can_message const& p_message)
  {
    m_message_buffer[m_cursor] = p_message;
    m_cursor = (m_cursor + 1) % m_message_buffer.size();
  }

private:
  void driver_configure(can_settings const& p_settings) override
  {
    m_settings = p_settings;
  }

  void driver_bus_on() override
  {
    m_bus_on_called = true;
  }

  void driver_send(can_message const& p_message) override
  {
    m_message = p_message;
  }

  std::span<can_message const> driver_receive_buffer() override
  {
    return m_message_buffer;
  }

  std::size_t driver_receive_cursor() override
  {
    return m_cursor;
  }
};
}  // namespace

boost::ut::suite<"buffered_can_test"> buffered_can_test = []() {
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

  "::receive_buffer() & ::receive_cursor()"_test = []() {
    // Setup
    test_can test;
    auto const receive_buffer = test.receive_buffer();
    auto const initial_cursor = test.receive_cursor();

    // Ensure
    expect(that % 0 == initial_cursor);
    expect(that % initial_cursor == test.receive_cursor());

    // Exercise
    // Exercise: Simulate incoming message
    test.add_message_to_buffer(expected_message);
    auto const first_message_cursor = test.receive_cursor();
    test.add_message_to_buffer(expected_message2);
    auto const second_message_cursor = test.receive_cursor();
    test.add_message_to_buffer(expected_message3);
    auto const last_cursor = test.receive_cursor();

    // Verify
    auto const distance = last_cursor - initial_cursor;
    expect(that % 3 == distance);
    expect(expected_message == receive_buffer[initial_cursor]);
    expect(expected_message2 == receive_buffer[first_message_cursor]);
    expect(expected_message3 == receive_buffer[second_message_cursor]);
  };
};
}  // namespace hal

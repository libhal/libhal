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
constexpr hal::can::message expected_message{ .id = 10, .length = 0 };
constexpr hal::can::message expected_message2{ .id = 11,
                                               .payload = { 0xAA, 0xBB },
                                               .length = 2 };
constexpr hal::can::message expected_message3{ .id = 22,
                                              .payload = { 0xCC, 0xDD, 0xEE, },
                                              .length = 3 };

class test_can : public hal::can
{
public:
  can::message m_message{};
  bool m_bus_on_called{ false };
  std::array<can::message, 16> m_message_buffer{};
  std::optional<hal::callback<void(on_bus_off_tag)>> m_on_bus_off_callback =
    std::nullopt;
  std::optional<hal::callback<void(on_receive_tag, can::message const&)>>
    m_on_receive_callback = std::nullopt;
  u32 m_cursor = 0;
  accept m_acceptance = accept::none;
  std::vector<id_t> m_filter_ids{};

  ~test_can() override = default;

  void add_message_to_buffer(can::message const& p_message)
  {
    m_message_buffer[m_cursor] = p_message;
    m_cursor = (m_cursor + 1) % m_message_buffer.size();
  }

private:
  u32 driver_baud_rate() override
  {
    return 100_kHz;
  }

  void driver_send(can::message const& p_message) override
  {
    m_message = p_message;
  }

  std::span<can::message const> driver_receive_buffer() override
  {
    return m_message_buffer;
  }

  u32 driver_receive_cursor() override
  {
    return m_cursor;
  }

  void driver_on_bus_off(
    std::optional<hal::callback<void(on_bus_off_tag)>> p_callback) override
  {
    m_on_bus_off_callback = p_callback;
  }

  void driver_on_receive(
    std::optional<hal::callback<void(on_receive_tag, can::message const&)>>
      p_callback) override
  {
    m_on_receive_callback = p_callback;
  }

  void driver_bus_on() override
  {
    m_bus_on_called = true;
  }

  void driver_filter_mode(accept p_acceptance) override
  {
    m_acceptance = p_acceptance;
    m_filter_ids.clear();
  }

  void driver_allow_message_id(id_t p_message_id) override
  {
    if (m_acceptance == accept::filtered) {
      m_filter_ids.push_back(p_message_id);
    }
  }
};
}  // namespace

boost::ut::suite<"can_test"> can_test = []() {
  using namespace boost::ut;

  "::baud_rate()"_test = []() {
    // Setup
    test_can test;

    // Verify
    expect(100_kHz == test.baud_rate());
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

  "::on_bus_off()"_test = []() {
    // Setup
    test_can test;
    bool callback_called = false;

    // Ensure
    expect(that % not callback_called);

    // Exercise
    test.on_bus_off(
      [&callback_called](can::on_bus_off_tag) { callback_called = true; });
    // Exercise: Use `value()` because it throws an exception if the optional is
    //           disengaged.
    test.m_on_bus_off_callback.value()(can::on_bus_off_tag{});

    // Verify
    expect(that % callback_called);
  };

  "::driver_on_receive()"_test = []() {
    // Setup
    test_can test;
    bool callback_called = false;
    can::message message{
      .id = 0,
      .payload = {},
      .length = 0,
      .is_remote_request = false,
    };

    // Ensure
    expect(that % not callback_called);
    expect(expected_message != message);

    // Exercise
    test.on_receive([&callback_called, &message](
                      can::on_receive_tag, can::message const& p_message) {
      callback_called = true;
      message = p_message;
    });
    // Exercise: Use `value()` because it throws an exception if the optional is
    //           disengaged.
    test.m_on_receive_callback.value()(can::on_receive_tag{}, expected_message);

    // Verify
    expect(that % callback_called);
    expect(expected_message == message);
  };

  "::filter_mode(can::accept::all)"_test = []() {
    // Setup
    test_can test;

    // Ensure
    expect(can::accept::all != test.m_acceptance);

    // Exercise
    test.filter_mode(can::accept::all);

    // Verify
    expect(can::accept::all == test.m_acceptance);
  };

  "::filter_mode(can::accept::none)"_test = []() {
    // Setup
    test_can test;

    // Ensure
    expect(can::accept::none == test.m_acceptance);

    // Exercise
    test.filter_mode(can::accept::none);

    // Verify: (stays the same)
    expect(can::accept::none == test.m_acceptance);
  };

  "::filter_mode(can::accept::filtered)"_test = []() {
    // Setup
    test_can test;

    // Ensure
    expect(can::accept::filtered != test.m_acceptance);

    // Exercise
    test.filter_mode(can::accept::filtered);

    // Verify
    expect(can::accept::filtered == test.m_acceptance);
  };

  "::filter_mode(filtered) && ::allow_message_id()"_test = []() {
    // Setup
    test_can test;

    // Ensure
    expect(can::accept::filtered != test.m_acceptance);
    expect(that % 0 == test.m_filter_ids.size());

    // Exercise
    test.filter_mode(can::accept::filtered);
    test.allow_message_id(0x111);
    test.allow_message_id(0x222);
    test.allow_message_id(0x333);
    test.allow_message_id(0x444);
    test.allow_message_id(0x123);

    // Verify
    expect(can::accept::filtered == test.m_acceptance);
    expect(that % 5 == test.m_filter_ids.size());
  };

  "::filter_mode(filtered) && ::allow_message_id() && ::filter_mode(none)"_test =
    []() {
      // Setup
      test_can test;
      test.filter_mode(can::accept::filtered);
      test.allow_message_id(0x111);
      test.allow_message_id(0x222);
      test.allow_message_id(0x333);
      test.allow_message_id(0x444);
      test.allow_message_id(0x123);

      // Ensure
      expect(can::accept::filtered == test.m_acceptance);
      expect(that % 5 == test.m_filter_ids.size());

      // Exercise
      test.filter_mode(can::accept::none);

      // Verify
      expect(can::accept::none == test.m_acceptance);
      expect(that % 0 == test.m_filter_ids.size());
    };
};
}  // namespace hal

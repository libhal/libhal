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

namespace {

constexpr hal::can_message expected_can_message = {
  .id = 22,
  .extended = false,
  .remote_request = false,
  .length = 3,
  .payload = {
    0xCC,
    0xDD,
    0xEE,
  },
};

struct test_can_transceiver : public hal::can_transceiver
{
public:
  can_message sent_message{};
  std::size_t cursor = 0;
  std::span<can_message> working_receive_buffer;

  test_can_transceiver(std::span<can_message> p_receive_buffer)
  {
    working_receive_buffer = p_receive_buffer;
  }

private:
  u32 driver_baud_rate() override
  {
    return 100_kHz;
  }

  void driver_send(can_message const& p_message) override
  {
    sent_message = p_message;
  }

  std::span<can_message const> driver_receive_buffer() override
  {
    return working_receive_buffer;
  }

  std::size_t driver_receive_cursor() override
  {
    return cursor++;
  }
};
}  // namespace

boost::ut::suite<"can_transceiver"> can_transceiver_test = []() {
  using namespace boost::ut;

  std::array<can_message, 8> receive_buffer{};

  "::baud_rate()"_test = [&]() {
    // Setup
    test_can_transceiver test(receive_buffer);

    // Ensure && Exercise && Verify
    expect(that % 100_kHz == test.baud_rate());
  };

  "::send()"_test = [&]() {
    // Setup
    test_can_transceiver test(receive_buffer);

    // Ensure
    expect(expected_can_message != test.sent_message);

    // Exercise
    test.send(expected_can_message);

    // Verify
    expect(expected_can_message == test.sent_message);
  };

  "::receive_buffer()"_test = [&]() {
    // Setup
    test_can_transceiver test(receive_buffer);

    // Exercise & Verify
    expect(that % std::span(receive_buffer).data() ==
           test.working_receive_buffer.data());
    expect(that % std::span(receive_buffer).size() ==
           test.working_receive_buffer.size());
  };

  "::cursor()"_test = [&]() {
    // Setup
    test_can_transceiver test(receive_buffer);

    // Verify
    expect(that % 0 == test.receive_cursor());
    expect(that % 1 == test.receive_cursor());
    expect(that % 2 == test.receive_cursor());
    expect(that % 3 == test.receive_cursor());
    expect(that % 4 == test.receive_cursor());
  };
};

namespace {
struct test_can_interrupt : public hal::can_interrupt
{
public:
  hal::can_interrupt::optional_receive_handler handler;

private:
  void driver_on_receive(optional_receive_handler p_callback) override
  {
    handler = p_callback;
  }
};
}  // namespace

boost::ut::suite<"can_interrupt"> can_interrupt_test = []() {
  using namespace boost::ut;

  "::on_receive()"_test = [&]() {
    // Setup
    test_can_interrupt test;
    int call_count = 0;

    // Ensure
    expect(that % not test.handler.has_value());

    // Exercise
    test.on_receive([&call_count](hal::can_interrupt::on_receive_tag,
                                  can_message const& p_message) {
      call_count++;
      expect(expected_can_message == p_message);
    });

    // Verify
    expect(that % test.handler.has_value());
    test.handler.value()(hal::can_interrupt::on_receive_tag{},
                         expected_can_message);
    expect(that % 1 == call_count);
  };
};

namespace {
struct test_can_bus_manager : public hal::can_bus_manager
{
public:
  hal::u32 current_baud_rate = 0_Hz;
  accept current_mode = accept::none;
  optional_bus_off_handler handler;
  int bus_on_call_count = 0;

private:
  void driver_baud_rate(hal::u32 p_hertz) override
  {
    current_baud_rate = p_hertz;
  }
  void driver_filter_mode(accept p_accept) override
  {
    current_mode = p_accept;
  }
  void driver_on_bus_off(
    std::optional<hal::callback<bus_off_handler>> p_callback) override
  {
    handler = p_callback;
  }
  void driver_bus_on() override
  {
    bus_on_call_count++;
  }
};
}  // namespace

boost::ut::suite<"can_bus_manager"> can_bus_manager_test = []() {
  using namespace boost::ut;

  "::baud_rate()"_test = [&]() {
    // Setup
    constexpr auto expected_baud = 250_kHz;
    test_can_bus_manager test;

    // Ensure
    expect(that % 0_Hz == test.current_baud_rate);

    // Exercise
    test.baud_rate(expected_baud);

    // Verify
    expect(that % expected_baud == test.current_baud_rate);
  };

  "::filter_mode()"_test = [&]() {
    // Setup
    constexpr auto expected_mode = hal::can_bus_manager::accept::filtered;
    test_can_bus_manager test;

    // Ensure
    expect(hal::can_bus_manager::accept::none == test.current_mode);

    // Exercise
    test.filter_mode(expected_mode);

    // Verify
    expect(expected_mode == test.current_mode);
  };

  "::on_bus_off()"_test = [&]() {
    // Setup
    test_can_bus_manager test;
    int call_count = 0;

    // Ensure
    expect(that % not test.handler.has_value());

    // Exercise
    test.on_bus_off(
      [&call_count](hal::can_bus_manager::bus_off_tag) { call_count++; });

    // Verify
    expect(that % test.handler.has_value());
    test.handler.value()(hal::can_bus_manager::bus_off_tag{});
    expect(that % 1 == call_count);
  };

  "::bus_on()"_test = [&]() {
    // Setup
    test_can_bus_manager test;

    // Ensure
    expect(that % 0 == test.bus_on_call_count);

    // Exercise
    test.bus_on();

    // Verify
    expect(that % 1 == test.bus_on_call_count);
  };
};

namespace {
struct test_can_identifier_filter : public hal::can_identifier_filter
{
public:
  std::optional<u16> id;

private:
  void driver_allow(std::optional<u16> p_id) override
  {
    id = p_id;
  }
};

struct test_can_extended_identifier_filter
  : public hal::can_extended_identifier_filter
{
public:
  std::optional<u32> id;

private:
  void driver_allow(std::optional<u32> p_id) override
  {
    id = p_id;
  }
};

struct test_can_range_filter : public hal::can_range_filter
{
public:
  std::optional<pair> id;

private:
  void driver_allow(std::optional<pair> p_id) override
  {
    id = p_id;
  }
};

struct test_can_extended_range_filter : public hal::can_extended_range_filter
{
public:
  std::optional<pair> id;

private:
  void driver_allow(std::optional<pair> p_id) override
  {
    id = p_id;
  }
};

struct test_can_mask_filter : public hal::can_mask_filter
{
public:
  std::optional<pair> id;

private:
  void driver_allow(std::optional<pair> p_id) override
  {
    id = p_id;
  }
};

struct test_can_extended_mask_filter : public hal::can_extended_mask_filter
{
public:
  std::optional<pair> id;

private:
  void driver_allow(std::optional<pair> p_id) override
  {
    id = p_id;
  }
};
}  // namespace

boost::ut::suite<"can_identifier_filter"> can_identifier_filter_test = []() {
  using namespace boost::ut;

  "hal::can_identifier_filter::allow()"_test = [&]() {
    // Setup
    constexpr auto expected_id = 0x15;
    test_can_identifier_filter test;

    // Ensure
    expect(that % not test.id.has_value());

    // Exercise
    test.allow(expected_id);

    // Verify
    expect(that % expected_id == test.id.value());
  };

  "hal::can_extended_identifier_filter::allow()"_test = [&]() {
    // Setup
    constexpr auto expected_id = 0x45;
    test_can_extended_identifier_filter test;

    // Ensure
    expect(that % not test.id.has_value());

    // Exercise
    test.allow(expected_id);

    // Verify
    expect(that % expected_id == test.id.value());
  };

  "hal::test_can_range_filter::allow()"_test = [&]() {
    // Setup
    constexpr auto expected_id1 = 0x45;
    constexpr auto expected_id2 = 0x90;
    constexpr hal::test_can_range_filter::pair expected_pair{
      .id_1 = expected_id1,
      .id_2 = expected_id2,
    };
    test_can_range_filter test;

    // Ensure
    expect(that % not test.id.has_value());

    // Exercise
    test.allow(expected_pair);

    // Verify
    expect(expected_pair == test.id.value());
  };

  "hal::test_can_extended_range_filter::allow()"_test = [&]() {
    // Setup
    constexpr auto expected_id1 = 0x4500;
    constexpr auto expected_id2 = 0x1A550;
    constexpr hal::test_can_extended_range_filter::pair expected_pair{
      .id_1 = expected_id1,
      .id_2 = expected_id2,
    };
    test_can_extended_range_filter test;

    // Ensure
    expect(that % not test.id.has_value());

    // Exercise
    test.allow(expected_pair);

    // Verify
    expect(expected_pair == test.id.value());
  };

  "hal::test_can_mask_filter::allow()"_test = [&]() {
    // Setup
    constexpr auto expected_id = 0x123;
    constexpr hal::test_can_mask_filter::pair expected_pair{
      .id = expected_id,
      .mask = 0x1FFF,
    };
    test_can_mask_filter test;

    // Ensure
    expect(that % not test.id.has_value());

    // Exercise
    test.allow(expected_pair);

    // Verify
    expect(expected_pair == test.id.value());
  };

  "hal::test_can_extended_mask_filter::allow()"_test = [&]() {
    // Setup
    constexpr auto expected_id = 0x1117;
    constexpr hal::test_can_extended_mask_filter::pair expected_pair{
      .id = expected_id,
      .mask = 0x1FF0,
    };
    test_can_extended_mask_filter test;

    // Ensure
    expect(that % not test.id.has_value());

    // Exercise
    test.allow(expected_pair);

    // Verify
    expect(expected_pair == test.id.value());
  };
};
}  // namespace hal

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

#include <array>
#include <memory_resource>
#include <optional>

#include <boost/ut.hpp>

import hal;

using namespace mp_units;
using namespace mp_units::si::unit_symbols;

namespace {
class test_can_transceiver : public hal::can_transceiver
{
public:
  hal::can_message last_sent_message{};
  std::array<hal::can_message, 8> rx_buffer{};
  hal::usize rx_cursor{ 0 };
  hal::hertz baud_rate_hz{ 500'000 * Hz };

private:
  hal::hertz driver_baud_rate() override
  {
    return baud_rate_hz;
  }

  void driver_send(hal::can_message const& p_message) override
  {
    last_sent_message = p_message;
  }

  hal::circular_span<hal::can_message const> driver_receive_buffer() override
  {
    return rx_buffer;
  }

  hal::usize driver_receive_cursor() override
  {
    return rx_cursor;
  }
};

class test_can_interrupt : public hal::can_interrupt
{
public:
  mem::optional_ptr<hal::can_receive_callback> stored_callback;

  /// Simulate the driver's ISR receiving a message
  void simulate_receive(hal::can_message const& p_message)
  {
    if (stored_callback.has_value()) {
      stored_callback.value()->callback(p_message);
    }
  }

private:
  void driver_on_receive(
    mem::optional_ptr<hal::can_receive_callback> const& p_callback) override
  {
    stored_callback = p_callback;
  }
};

class counting_receive_callback : public hal::can_receive_callback
{
public:
  int call_count = 0;
  hal::can_message last_message{};

  void callback(hal::can_message const& p_message) noexcept override
  {
    call_count++;
    last_message = p_message;
  }
};

class counting_bus_off_callback : public hal::can_bus_off_callback
{
public:
  int call_count = 0;

  void callback() noexcept override
  {
    call_count++;
  }
};

class test_can_bus_manager : public hal::can_bus_manager
{
public:
  hal::u32 last_baud_rate{ 0 };
  hal::can_message_acceptance last_filter_mode{};
  mem::optional_ptr<hal::can_bus_off_callback> bus_off_callback;
  bool bus_on_called{ false };

private:
  void driver_baud_rate(hal::u32 p_hertz) override
  {
    last_baud_rate = p_hertz;
  }

  void driver_filter_mode(hal::can_message_acceptance p_accept) override
  {
    last_filter_mode = p_accept;
  }

  void driver_on_bus_off(
    mem::optional_ptr<hal::can_bus_off_callback> const& p_callback) override
  {
    bus_off_callback = p_callback;
  }

  void driver_bus_on() override
  {
    bus_on_called = true;
  }
};

template<typename Allowed>
class test_can_filter : public hal::can_filter<Allowed>
{
public:
  std::optional<Allowed> last_allowed{ std::nullopt };

private:
  void driver_allow(std::optional<Allowed> p_allowed) override
  {
    last_allowed = p_allowed;
  }
};

void can_message_equality_test() noexcept
{
  using namespace boost::ut;

  "can_message equality with identical messages"_test = [&]() {
    hal::can_message a{
      .id = 0x123,
      .extended = false,
      .remote_request = false,
      .length = 3,
      .payload = { hal::byte{ 0x01 }, hal::byte{ 0x02 }, hal::byte{ 0x03 } },
    };
    hal::can_message b = a;

    expect(a == b);
  };

  "can_message inequality when IDs differ"_test = [&]() {
    hal::can_message a{ .id = 0x100, .length = 1 };
    hal::can_message b{ .id = 0x200, .length = 1 };

    expect(!(a == b));
  };

  "can_message inequality when lengths differ"_test = [&]() {
    hal::can_message a{ .id = 0x100, .length = 1 };
    hal::can_message b{ .id = 0x100, .length = 2 };

    expect(!(a == b));
  };

  "can_message inequality when payload bytes within length differ"_test =
    [&]() {
      hal::can_message a{
        .id = 0x100,
        .length = 2,
        .payload = { hal::byte{ 0xAA }, hal::byte{ 0xBB } },
      };
      hal::can_message b{
        .id = 0x100,
        .length = 2,
        .payload = { hal::byte{ 0xAA }, hal::byte{ 0xCC } },
      };

      expect(!(a == b));
    };

  "can_message equality ignores payload bytes beyond length"_test = [&]() {
    hal::can_message a{
      .id = 0x100,
      .length = 1,
      .payload = { hal::byte{ 0xAA }, hal::byte{ 0xFF } },
    };
    hal::can_message b{
      .id = 0x100,
      .length = 1,
      .payload = { hal::byte{ 0xAA }, hal::byte{ 0x00 } },
    };

    expect(a == b);
  };

  "can_message inequality when extended flag differs"_test = [&]() {
    hal::can_message a{ .id = 0x100, .extended = false, .length = 0 };
    hal::can_message b{ .id = 0x100, .extended = true, .length = 0 };

    expect(!(a == b));
  };

  "can_message inequality when remote_request flag differs"_test = [&]() {
    hal::can_message a{ .id = 0x100, .remote_request = false, .length = 0 };
    hal::can_message b{ .id = 0x100, .remote_request = true, .length = 0 };

    expect(!(a == b));
  };

  "can_message equality with zero-length payload"_test = [&]() {
    hal::can_message a{ .id = 0x42, .length = 0 };
    hal::can_message b{ .id = 0x42, .length = 0 };

    expect(a == b);
  };

  "can_message equality with extended ID"_test = [&]() {
    hal::can_message a{
      .id = 0x1FFFFFFF,
      .extended = true,
      .length = 2,
      .payload = { hal::byte{ 0xDE }, hal::byte{ 0xAD } },
    };
    hal::can_message b = a;

    expect(a == b);
  };
}

void can_transceiver_baud_rate_test() noexcept
{
  using namespace boost::ut;

  "baud_rate() returns configured value"_test = [&]() {
    test_can_transceiver test;
    test.baud_rate_hz = 1'000'000 * Hz;

    auto result = test.baud_rate();

    expect((1'000'000 * Hz) == result);
  };

  "baud_rate() returns default value"_test = [&]() {
    test_can_transceiver test;

    auto result = test.baud_rate();

    expect((500'000 * Hz) == result);
  };
}

void can_transceiver_send_test() noexcept
{
  using namespace boost::ut;

  "send() passes message to driver"_test = [&]() {
    test_can_transceiver test;
    hal::can_message expected{
      .id = 0x123,
      .extended = false,
      .remote_request = false,
      .length = 2,
      .payload = { hal::byte{ 0xAB }, hal::byte{ 0xCD } },
    };

    test.send(expected);

    expect(expected == test.last_sent_message);
  };

  "send() with extended message"_test = [&]() {
    test_can_transceiver test;
    hal::can_message expected{
      .id = 0x1FFFFFFF,
      .extended = true,
      .length = 1,
      .payload = { hal::byte{ 0xFF } },
    };

    test.send(expected);

    expect(expected == test.last_sent_message);
  };

  "send() with zero-length message"_test = [&]() {
    test_can_transceiver test;
    hal::can_message expected{ .id = 0x200, .length = 0 };

    test.send(expected);

    expect(expected == test.last_sent_message);
  };
}

void can_transceiver_receive_buffer_test() noexcept
{
  using namespace boost::ut;

  "receive_buffer() returns driver's buffer"_test = [&]() {
    test_can_transceiver test;
    test.rx_buffer[0].id = 0x111;
    test.rx_buffer[1].id = 0x222;

    auto buf = test.receive_buffer();

    expect(that % test.rx_buffer.size() == buf.size());
    expect(that % 0x111u == buf[0].id);
    expect(that % 0x222u == buf[1].id);
  };

  "receive_buffer() size is at least 1"_test = [&]() {
    test_can_transceiver test;

    auto buf = test.receive_buffer();

    expect(buf.size() >= 1);
  };
}

void can_transceiver_receive_cursor_test() noexcept
{
  using namespace boost::ut;

  "receive_cursor() returns initial cursor of zero"_test = [&]() {
    test_can_transceiver test;

    auto cursor = test.receive_cursor();

    expect(that % 0 == cursor);
  };

  "receive_cursor() returns updated cursor value"_test = [&]() {
    test_can_transceiver test;
    test.rx_cursor = 3;

    auto cursor = test.receive_cursor();

    expect(that % 3 == cursor);
  };

  "receive_cursor() is within bounds of receive_buffer()"_test = [&]() {
    test_can_transceiver test;
    test.rx_cursor = 7;

    auto cursor = test.receive_cursor();
    auto buf = test.receive_buffer();

    expect(cursor < buf.size());
  };
}

void can_bus_manager_test() noexcept
{
  using namespace boost::ut;

  "baud_rate() passes hertz to driver"_test = [&]() {
    test_can_bus_manager test;

    test.baud_rate(250'000);

    expect(250'000 == test.last_baud_rate);
  };

  "baud_rate() passes 1MHz to driver"_test = [&]() {
    test_can_bus_manager test;

    test.baud_rate(1'000'000);

    expect(1'000'000 == test.last_baud_rate);
  };

  "filter_mode() passes 'none' to driver"_test = [&]() {
    test_can_bus_manager test;

    test.filter_mode(hal::can_message_acceptance::none);

    expect(hal::can_message_acceptance::none == test.last_filter_mode);
  };

  "filter_mode() passes 'all' to driver"_test = [&]() {
    test_can_bus_manager test;

    test.filter_mode(hal::can_message_acceptance::all);

    expect(hal::can_message_acceptance::all == test.last_filter_mode);
  };

  "filter_mode() passes 'filtered' to driver"_test = [&]() {
    test_can_bus_manager test;

    test.filter_mode(hal::can_message_acceptance::filtered);

    expect(hal::can_message_acceptance::filtered == test.last_filter_mode);
  };

  "on_bus_off() passes callback to driver"_test = [&]() {
    // Setup
    test_can_bus_manager test;
    auto callback = mem::make_strong_ptr<counting_bus_off_callback>(
      std::pmr::new_delete_resource());

    // Exercise
    test.on_bus_off(callback);
    test.bus_off_callback.value()->callback();

    // Verify
    expect(test.bus_off_callback.has_value());
    expect(that % 1 == callback->call_count);
  };

  "on_bus_off() with empty callback clears it"_test = [&]() {
    // Setup
    test_can_bus_manager test;
    auto callback = mem::make_strong_ptr<counting_bus_off_callback>(
      std::pmr::new_delete_resource());
    test.on_bus_off(callback);

    // Exercise
    test.on_bus_off(mem::optional_ptr<hal::can_bus_off_callback>{});

    // Verify
    expect(not test.bus_off_callback.has_value());
  };

  "bus_on() calls driver"_test = [&]() {
    test_can_bus_manager test;

    test.bus_on();

    expect(test.bus_on_called);
  };
}

void can_interrupt_test() noexcept
{
  using namespace boost::ut;

  "on_receive() delivers received message to callback"_test = [&]() {
    // Setup
    test_can_interrupt test;
    auto callback = mem::make_strong_ptr<counting_receive_callback>(
      std::pmr::new_delete_resource());
    hal::can_message const message{ .id = 0x111, .length = 1 };

    // Exercise
    test.on_receive(callback);
    test.simulate_receive(message);

    // Verify
    expect(that % 1 == callback->call_count);
    expect(message == callback->last_message);
  };

  "on_receive() with empty callback stops delivery"_test = [&]() {
    // Setup
    test_can_interrupt test;
    auto callback = mem::make_strong_ptr<counting_receive_callback>(
      std::pmr::new_delete_resource());
    test.on_receive(callback);

    // Exercise
    test.on_receive(mem::optional_ptr<hal::can_receive_callback>{});
    test.simulate_receive(hal::can_message{});

    // Verify
    expect(that % 0 == callback->call_count);
  };
}

void can_filter_test() noexcept
{
  using namespace boost::ut;

  "can_id_filter allow() passes ID to driver"_test = [&]() {
    test_can_filter<hal::u16> test;

    test.allow(hal::u16{ 0x123 });

    expect(test.last_allowed.has_value());
    expect(that % hal::u16{ 0x123 } == test.last_allowed.value());
  };

  "can_id_filter allow() with nullopt clears filter"_test = [&]() {
    test_can_filter<hal::u16> test;
    test.allow(hal::u16{ 0x123 });

    test.allow(std::nullopt);

    expect(!test.last_allowed.has_value());
  };

  "can_mask_filter allow() passes mask criteria to driver"_test = [&]() {
    test_can_filter<hal::can_mask> test;
    hal::can_mask expected{ .id = 0x100, .mask = 0x7F0 };

    test.allow(expected);

    expect(test.last_allowed.has_value());
    expect((expected == test.last_allowed.value()) >> fatal);
  };

  "can_mask_filter allow() with nullopt clears filter"_test = [&]() {
    test_can_filter<hal::can_mask> test;
    test.allow(hal::can_mask{ .id = 0x100, .mask = 0x7FF });

    test.allow(std::nullopt);

    expect(!test.last_allowed.has_value());
  };

  "can_range_filter allow() passes range to driver"_test = [&]() {
    test_can_filter<hal::can_range> test;
    hal::can_range expected{ .id_1 = 0x100, .id_2 = 0x1FF };

    test.allow(expected);

    expect(test.last_allowed.has_value());
    expect((expected == test.last_allowed.value()) >> fatal);
  };

  "can_range_filter allow() with nullopt clears filter"_test = [&]() {
    test_can_filter<hal::can_range> test;
    test.allow(hal::can_range{ .id_1 = 0x100, .id_2 = 0x1FF });

    test.allow(std::nullopt);

    expect(!test.last_allowed.has_value());
  };

  "can_mask_ext_filter allow() passes extended mask criteria to driver"_test =
    [&]() {
      test_can_filter<hal::can_mask_ext> test;
      hal::can_mask_ext expected{ .id = 0x1FFFF00, .mask = 0x1FFFFFF0 };

      test.allow(expected);

      expect(test.last_allowed.has_value());
      expect((expected == test.last_allowed.value()) >> fatal);
    };

  "can_range_ext_filter allow() passes extended range to driver"_test = [&]() {
    test_can_filter<hal::can_range_ext> test;
    hal::can_range_ext expected{ .id_1 = 0x1000000, .id_2 = 0x1FFFFFFF };

    test.allow(expected);

    expect(test.last_allowed.has_value());
    expect((expected == test.last_allowed.value()) >> fatal);
  };
}

}  // namespace

int main()
{
  can_message_equality_test();
  can_transceiver_baud_rate_test();
  can_transceiver_send_test();
  can_transceiver_receive_buffer_test();
  can_transceiver_receive_cursor_test();
  can_bus_manager_test();
  can_interrupt_test();
  can_filter_test();
}

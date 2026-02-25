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
#include <chrono>
#include <coroutine>
#include <memory_resource>

#include <boost/ut.hpp>

import hal;
import async_context;

using namespace mp_units::si::unit_symbols;

namespace {
class test_serial : public hal::serial
{
public:
  hal::serial::settings configured_settings{};
  std::array<hal::byte, 256> last_data_out{};
  hal::usize last_data_out_size{};
  std::array<hal::byte, 256> rx_buffer{};
  hal::usize rx_cursor{ 0 };

  ~test_serial() override = default;

private:
  async::future<void> driver_configure(
    async::context&,
    hal::serial::settings const& p_settings) override
  {
    configured_settings = p_settings;
    return {};
  }

  async::future<void> driver_write(
    async::context&,
    hal::scatter_span<hal::byte const> p_data) override
  {
    last_data_out_size = 0;
    for (auto const& span : p_data) {
      for (auto byte : span) {
        if (last_data_out_size >= last_data_out.size()) {
          break;
        }
        last_data_out[last_data_out_size] = byte;
        last_data_out_size++;
      }
    }
    return {};
  }

  hal::circular_span<hal::byte const> driver_receive_buffer() override
  {
    return rx_buffer;
  }

  hal::usize driver_receive_cursor() override
  {
    return rx_cursor;
  }
};

class test_awaitable_serial : public hal::awaitable_serial
{
public:
  hal::serial::settings configured_settings{};
  std::array<hal::byte, 256> last_data_out{};
  hal::usize last_data_out_size{};
  std::array<hal::byte, 256> rx_buffer{};
  hal::usize rx_cursor{ 0 };
  hal::serial_event last_event{};
  bool wait_for_called{ false };

  ~test_awaitable_serial() override = default;

private:
  async::future<void> driver_configure(
    async::context&,
    hal::serial::settings const& p_settings) override
  {
    configured_settings = p_settings;
    return {};
  }

  async::future<void> driver_write(
    async::context&,
    hal::scatter_span<hal::byte const> p_data) override
  {
    last_data_out_size = 0;
    for (auto const& span : p_data) {
      for (auto byte : span) {
        if (last_data_out_size >= last_data_out.size()) {
          break;
        }
        last_data_out[last_data_out_size] = byte;
        last_data_out_size++;
      }
    }
    return {};
  }

  hal::circular_span<hal::byte const> driver_receive_buffer() override
  {
    return rx_buffer;
  }

  hal::usize driver_receive_cursor() override
  {
    return rx_cursor;
  }

  async::future<void> driver_wait_for(async::context&,
                                      hal::serial_event p_event) override
  {
    last_event = p_event;
    wait_for_called = true;
    return {};
  }
};

void serial_configure_test() noexcept
{
  using namespace boost::ut;

  "configure() passes settings to driver"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_serial test;
    hal::serial::settings expected_settings{
      .baud_rate = 9600 * Hz,
      .stop = hal::serial::settings::stop_bits::two,
      .parity = hal::serial::settings::parity::even,
    };

    // Exercise
    test.configure(ctx, expected_settings);

    // Verify
    expect(expected_settings == test.configured_settings);
  };

  "configure() with default settings"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_serial test;
    hal::serial::settings default_settings{};

    // Exercise
    test.configure(ctx, default_settings);

    // Verify
    expect((115200 * Hz) == test.configured_settings.baud_rate);
    expect(hal::serial::settings::stop_bits::one ==
           test.configured_settings.stop);
    expect(hal::serial::settings::parity::none ==
           test.configured_settings.parity);
  };

  "configure() with odd parity and two stop bits"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_serial test;
    hal::serial::settings settings{
      .baud_rate = 38400 * Hz,
      .stop = hal::serial::settings::stop_bits::two,
      .parity = hal::serial::settings::parity::odd,
    };

    // Exercise
    test.configure(ctx, settings);

    // Verify
    expect(settings == test.configured_settings);
  };
}

void serial_write_test() noexcept
{
  using namespace boost::ut;

  "write() sends data to driver"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_serial test;
    std::array<hal::byte, 4> write_buffer = { 0x01, 0x02, 0x03, 0x04 };
    auto write_data = hal::make_scatter_bytes(write_buffer);

    // Exercise
    test.write(ctx, write_data);

    // Verify
    expect(that % 4 == test.last_data_out_size);
  };

  "write() with empty data"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_serial test;
    auto write_data = hal::make_scatter_bytes();

    // Exercise
    test.write(ctx, write_data);

    // Verify
    expect(that % 0 == test.last_data_out_size);
  };

  "write() data content is correct"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_serial test;
    std::array<hal::byte, 3> write_buffer = { hal::byte{ 0xAA },
                                              hal::byte{ 0xBB },
                                              hal::byte{ 0xCC } };
    auto write_data = hal::make_scatter_bytes(write_buffer);

    // Exercise
    test.write(ctx, write_data);

    // Verify
    expect(that % 0xAA == test.last_data_out[0]);
    expect(that % 0xBB == test.last_data_out[1]);
    expect(that % 0xCC == test.last_data_out[2]);
  };

  "write() single byte"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_serial test;
    std::array<hal::byte, 1> write_buffer = { hal::byte{ 0xFF } };
    auto write_data = hal::make_scatter_bytes(write_buffer);

    // Exercise
    test.write(ctx, write_data);

    // Verify
    expect(that % 1 == test.last_data_out_size);
    expect(that % 0xFF == test.last_data_out[0]);
  };
}

void serial_receive_buffer_test() noexcept
{
  using namespace boost::ut;

  "receive_buffer() returns driver's buffer"_test = [&]() {
    // Setup
    test_serial test;
    test.rx_buffer[0] = hal::byte{ 0x42 };
    test.rx_buffer[1] = hal::byte{ 0x43 };

    // Exercise
    auto buf = test.receive_buffer();

    // Verify
    expect(that % test.rx_buffer.size() == buf.size());
    expect(that % hal::byte{ 0x42 } == buf[0]);
    expect(that % hal::byte{ 0x43 } == buf[1]);
    expect(that % static_cast<void*>(test.rx_buffer.data()) ==
           static_cast<void const*>(buf.span().data()))
      << "test.rx_buffer.data() = " << static_cast<void*>(test.rx_buffer.data())
      << " and buf.span().data() = "
      << static_cast<void const*>(buf.span().data());
  };

  "receive_buffer() size is at least 1"_test = [&]() {
    // Setup
    test_serial test;

    // Exercise
    auto buf = test.receive_buffer();

    // Verify
    expect(buf.size() >= 1);
  };
}

void serial_receive_cursor_test() noexcept
{
  using namespace boost::ut;

  "receive_cursor() returns initial cursor of zero"_test = [&]() {
    // Setup
    test_serial test;

    // Exercise
    auto cursor = test.receive_cursor();

    // Verify
    expect(that % 0 == cursor);
  };

  "receive_cursor() returns updated cursor value"_test = [&]() {
    // Setup
    test_serial test;
    test.rx_cursor = 5;

    // Exercise
    auto cursor = test.receive_cursor();

    // Verify
    expect(that % 5 == cursor);
  };

  "receive_cursor() is within bounds of receive_buffer()"_test = [&]() {
    // Setup
    test_serial test;
    test.rx_cursor = 100;

    // Exercise
    auto cursor = test.receive_cursor();
    auto buf = test.receive_buffer();

    // Verify
    expect(cursor < buf.size());
  };
}

void awaitable_serial_wait_for_test() noexcept
{
  using namespace boost::ut;

  "wait_for() with rx event calls driver"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_awaitable_serial test;

    // Exercise
    test.wait_for(ctx, hal::serial_event::rx);

    // Verify
    expect(test.wait_for_called);
    expect(hal::serial_event::rx == test.last_event);
  };

  "wait_for() with idle event calls driver"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_awaitable_serial test;

    // Exercise
    test.wait_for(ctx, hal::serial_event::idle);

    // Verify
    expect(test.wait_for_called);
    expect(hal::serial_event::idle == test.last_event);
  };

  "awaitable_serial inherits serial configure()"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_awaitable_serial test;
    hal::serial::settings expected_settings{
      .baud_rate = 57600 * Hz,
    };

    // Exercise
    test.configure(ctx, expected_settings);

    // Verify
    expect(expected_settings == test.configured_settings);
  };

  "awaitable_serial inherits serial write()"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_awaitable_serial test;
    std::array<hal::byte, 2> write_buffer = { hal::byte{ 0x12 },
                                              hal::byte{ 0x34 } };
    auto write_data = hal::make_scatter_bytes(write_buffer);

    // Exercise
    test.write(ctx, write_data);

    // Verify
    expect(that % 2 == test.last_data_out_size);
  };
}

}  // namespace

int main()
{
  serial_configure_test();
  serial_write_test();
  serial_receive_buffer_test();
  serial_receive_cursor_test();
  awaitable_serial_wait_for_test();
}

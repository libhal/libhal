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
#include <memory_resource>

#include <boost/ut.hpp>

import hal;

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

private:
  void driver_configure(hal::serial::settings const& p_settings) override
  {
    configured_settings = p_settings;
  }

  void driver_write(mem::scatter_span<hal::byte const> p_data) override
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

class test_serial_interrupt : public hal::serial_interrupt
{
public:
  mem::optional_ptr<hal::serial_receive_callback> stored_callback;

  /// Simulate the driver's ISR reporting a receive event
  void simulate_event(hal::serial_event p_event)
  {
    if (stored_callback.has_value()) {
      stored_callback.value()->callback(p_event);
    }
  }

private:
  void driver_on_receive(
    mem::optional_ptr<hal::serial_receive_callback> const& p_callback) override
  {
    stored_callback = p_callback;
  }
};

class recording_receive_callback : public hal::serial_receive_callback
{
public:
  int call_count = 0;
  hal::serial_event last_event{};

  void callback(hal::serial_event p_event) noexcept override
  {
    call_count++;
    last_event = p_event;
  }
};

void serial_configure_test() noexcept
{
  using namespace boost::ut;

  "configure() passes settings to driver"_test = [&]() {
    // Setup
    test_serial test;
    hal::serial::settings expected_settings{
      .baud_rate = 9600 * Hz,
      .stop = hal::serial::settings::stop_bits::two,
      .parity = hal::serial::settings::parity::even,
    };

    // Exercise
    test.configure(expected_settings);

    // Verify
    expect(expected_settings == test.configured_settings);
  };

  "configure() with default settings"_test = [&]() {
    // Setup
    test_serial test;
    hal::serial::settings default_settings{};

    // Exercise
    test.configure(default_settings);

    // Verify
    expect((115200 * Hz) == test.configured_settings.baud_rate);
    expect(hal::serial::settings::stop_bits::one ==
           test.configured_settings.stop);
    expect(hal::serial::settings::parity::none ==
           test.configured_settings.parity);
  };

  "configure() with odd parity and two stop bits"_test = [&]() {
    // Setup
    test_serial test;
    hal::serial::settings settings{
      .baud_rate = 38400 * Hz,
      .stop = hal::serial::settings::stop_bits::two,
      .parity = hal::serial::settings::parity::odd,
    };

    // Exercise
    test.configure(settings);

    // Verify
    expect(settings == test.configured_settings);
  };
}

void serial_write_test() noexcept
{
  using namespace boost::ut;

  "write() sends data to driver"_test = [&]() {
    // Setup
    test_serial test;
    std::array<hal::byte, 4> write_buffer = { 0x01, 0x02, 0x03, 0x04 };

    // Exercise
    test.write({ write_buffer });

    // Verify
    expect(that % 4 == test.last_data_out_size);
  };

  "write() with empty data"_test = [&]() {
    // Setup
    test_serial test;

    // Exercise
    test.write({});

    // Verify
    expect(that % 0 == test.last_data_out_size);
  };

  "write() data content is correct"_test = [&]() {
    // Setup
    test_serial test;
    std::array<hal::byte, 3> write_buffer = { hal::byte{ 0xAA },
                                              hal::byte{ 0xBB },
                                              hal::byte{ 0xCC } };

    // Exercise
    test.write({ write_buffer });

    // Verify
    expect(that % 0xAA == test.last_data_out[0]);
    expect(that % 0xBB == test.last_data_out[1]);
    expect(that % 0xCC == test.last_data_out[2]);
  };

  "write() single byte"_test = [&]() {
    // Setup
    test_serial test;
    std::array<hal::byte, 1> write_buffer = { hal::byte{ 0xFF } };

    // Exercise
    test.write({ write_buffer });

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

void serial_interrupt_test() noexcept
{
  using namespace boost::ut;

  "on_receive() delivers rx event to callback"_test = [&]() {
    // Setup
    test_serial_interrupt test;
    auto callback = mem::make_strong_ptr<recording_receive_callback>(
      std::pmr::new_delete_resource());

    // Exercise
    test.on_receive(callback);
    test.simulate_event(hal::serial_event::rx);

    // Verify
    expect(that % 1 == callback->call_count);
    expect(hal::serial_event::rx == callback->last_event);
  };

  "on_receive() delivers idle event to callback"_test = [&]() {
    // Setup
    test_serial_interrupt test;
    auto callback = mem::make_strong_ptr<recording_receive_callback>(
      std::pmr::new_delete_resource());

    // Exercise
    test.on_receive(callback);
    test.simulate_event(hal::serial_event::idle);

    // Verify
    expect(that % 1 == callback->call_count);
    expect(hal::serial_event::idle == callback->last_event);
  };

  "on_receive() with empty callback stops delivery"_test = [&]() {
    // Setup
    test_serial_interrupt test;
    auto callback = mem::make_strong_ptr<recording_receive_callback>(
      std::pmr::new_delete_resource());
    test.on_receive(callback);

    // Exercise
    test.on_receive(mem::optional_ptr<hal::serial_receive_callback>{});
    test.simulate_event(hal::serial_event::rx);

    // Verify
    expect(that % 0 == callback->call_count);
  };
}

}  // namespace

int main()
{
  serial_configure_test();
  serial_write_test();
  serial_receive_buffer_test();
  serial_receive_cursor_test();
  serial_interrupt_test();
}

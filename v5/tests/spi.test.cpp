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

using namespace mp_units;
using namespace mp_units::si::unit_symbols;

namespace {
class test_spi : public hal::spi_channel
{
public:
  hal::spi_channel::settings configured_settings{};
  hal::hertz reported_clock_rate = 100'000 * si::hertz;
  bool chip_selected{ false };
  std::array<hal::byte, 256> last_data_out{};
  std::array<hal::byte, 256> last_data_in{};
  hal::usize last_data_out_size{};
  hal::usize last_data_in_size{};
  hal::byte last_filler{ hal::spi_channel::default_filler };

  ~test_spi() override = default;

private:
  async::future<void> driver_configure(
    async::context&,
    hal::spi_channel::settings const& p_settings) override
  {
    configured_settings = p_settings;
    return {};
  }

  async::future<hal::hertz> driver_clock_rate(async::context&) override
  {
    return reported_clock_rate;
  }

  async::future<void> driver_chip_select(async::context&,
                                         bool p_select) override
  {
    chip_selected = p_select;
    return {};
  }

  async::future<void> driver_transfer(
    async::context&,
    hal::scatter_span<hal::byte const> p_data_out,
    hal::scatter_span<hal::byte> p_data_in,
    hal::byte p_filler) override
  {
    last_filler = p_filler;

    last_data_out_size = 0;
    for (auto const& span : p_data_out) {
      for (auto byte : span) {
        if (last_data_out_size >= last_data_out.size()) {
          break;
        }
        last_data_out[last_data_out_size] = byte;
        last_data_out_size++;
      }
    }

    last_data_in_size = 0;
    for (auto const& span : p_data_in) {
      last_data_in_size += span.size();
    }

    return {};
  }
};

void spi_configure_test() noexcept
{
  using namespace boost::ut;

  "configure() passes settings to driver"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_spi test;
    hal::spi_channel::settings expected_settings{
      .clock_rate = 1 * MHz,
      .bus_mode = hal::spi_channel::mode::m1,
    };

    // Exercise
    std::ignore = test.configure(ctx, expected_settings);

    // Verify
    expect(expected_settings == test.configured_settings);
  };

  "configure() with default settings"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_spi test;
    hal::spi_channel::settings default_settings{};

    // Exercise
    std::ignore = test.configure(ctx, default_settings);

    // Verify
    expect((100 * kHz) == test.configured_settings.clock_rate);
    expect(hal::spi_channel::mode::m0 == test.configured_settings.bus_mode);
  };
}

void spi_clock_rate_test() noexcept
{
  using namespace boost::ut;

  "clock_rate() returns value from driver"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_spi test;
    auto const expected_freq = 4'000'000 * si::hertz;
    test.reported_clock_rate = expected_freq;

    // Exercise
    auto rate = test.clock_rate(ctx).value();

    // Verify
    expect(expected_freq == rate);
  };
}

void spi_chip_select_test() noexcept
{
  using namespace boost::ut;

  "chip_select(true) asserts chip select"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_spi test;

    // Exercise
    std::ignore = test.chip_select(ctx, true);

    // Verify
    expect(test.chip_selected);
  };

  "chip_select(false) de-asserts chip select"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_spi test;
    std::ignore = test.chip_select(ctx, true);

    // Exercise
    std::ignore = test.chip_select(ctx, false);

    // Verify
    expect(not test.chip_selected);
  };
}

void spi_lock_unlock_test() noexcept
{
  using namespace boost::ut;

  "lock() asserts chip select"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_spi test;

    // Exercise
    std::ignore = test.lock(ctx);

    // Verify
    expect(test.chip_selected);
  };

  "unlock() de-asserts chip select"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_spi test;
    std::ignore = test.lock(ctx);

    // Exercise
    std::ignore = test.unlock(ctx);

    // Verify
    expect(not test.chip_selected);
  };
}

void spi_transfer_test() noexcept
{
  using namespace boost::ut;

  "transfer() with write-only data"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_spi test;
    std::array<hal::byte, 4> write_buffer = { 0x01, 0x02, 0x03, 0x04 };
    auto write_data = hal::make_scatter_bytes(write_buffer);
    auto read_spans = hal::make_writable_scatter_bytes();

    // Exercise
    std::ignore = test.transfer(ctx, write_data, read_spans);

    // Verify
    expect(that % 4 == test.last_data_out_size);
    expect(that % 0 == test.last_data_in_size);
  };

  "transfer() with read-only data"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_spi test;
    auto write_data = hal::make_scatter_bytes();
    std::array<hal::byte, 8> read_buffer{};
    auto read_spans = hal::make_writable_scatter_bytes(read_buffer);

    // Exercise
    std::ignore = test.transfer(ctx, write_data, read_spans);

    // Verify
    expect(that % 0 == test.last_data_out_size);
    expect(that % 8 == test.last_data_in_size);
  };

  "transfer() with write-and-read data"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_spi test;
    std::array<hal::byte, 3> write_buffer = { 0xAA, 0xBB, 0xCC };
    auto write_data = hal::make_scatter_bytes(write_buffer);
    std::array<hal::byte, 5> read_buffer{};
    auto read_spans = hal::make_writable_scatter_bytes(read_buffer);

    // Exercise
    std::ignore = test.transfer(ctx, write_data, read_spans);

    // Verify
    expect(that % 3 == test.last_data_out_size);
    expect(that % 5 == test.last_data_in_size);
  };

  "transfer() with empty data"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_spi test;
    auto write_data = hal::make_scatter_bytes();
    auto read_spans = hal::make_writable_scatter_bytes();

    // Exercise
    std::ignore = test.transfer(ctx, write_data, read_spans);

    // Verify
    expect(that % 0 == test.last_data_out_size);
    expect(that % 0 == test.last_data_in_size);
  };

  "transfer() uses default filler when not specified"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_spi test;
    auto write_data = hal::make_scatter_bytes();
    auto read_spans = hal::make_writable_scatter_bytes();

    // Exercise
    std::ignore = test.transfer(ctx, write_data, read_spans);

    // Verify
    expect(hal::spi_channel::default_filler == test.last_filler);
  };

  "transfer() uses custom filler when specified"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_spi test;
    auto write_data = hal::make_scatter_bytes();
    auto read_spans = hal::make_writable_scatter_bytes();
    hal::byte custom_filler{ 0x00 };

    // Exercise
    std::ignore = test.transfer(ctx, write_data, read_spans, custom_filler);

    // Verify
    expect(custom_filler == test.last_filler);
  };

  "transfer() write data content is correct"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_spi test;
    std::array<hal::byte, 3> write_buffer = { hal::byte{ 0x11 },
                                              hal::byte{ 0x22 },
                                              hal::byte{ 0x33 } };
    auto write_data = hal::make_scatter_bytes(write_buffer);
    auto read_spans = hal::make_writable_scatter_bytes();

    // Exercise
    std::ignore = test.transfer(ctx, write_data, read_spans);

    // Verify
    expect(hal::byte{ 0x11 } == test.last_data_out[0]);
    expect(hal::byte{ 0x22 } == test.last_data_out[1]);
    expect(hal::byte{ 0x33 } == test.last_data_out[2]);
  };
}

}  // namespace

int main()
{
  spi_configure_test();
  spi_clock_rate_test();
  spi_chip_select_test();
  spi_lock_unlock_test();
  spi_transfer_test();
}

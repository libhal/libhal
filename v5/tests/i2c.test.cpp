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

#include <array>
#include <chrono>
#include <coroutine>
#include <memory_resource>

#include <boost/ut.hpp>

import hal;
import async_context;

using namespace mp_units::si::unit_symbols;

namespace {
class test_i2c : public hal::i2c
{
public:
  hal::i2c::settings configured_settings{};
  hal::byte last_address{};
  std::array<hal::byte, 256> last_data_out{};
  std::array<hal::byte, 256> last_data_in{};
  hal::usize last_data_out_size{};
  hal::usize last_data_in_size{};

  ~test_i2c() override = default;

private:
  async::future<void> driver_configure(
    async::context&,
    hal::i2c::settings const& p_settings) override
  {
    configured_settings = p_settings;
    return {};
  }

  async::future<void> driver_transaction(
    async::context&,
    hal::byte p_address,
    hal::scatter_span<hal::byte const> p_data_out,
    hal::scatter_span<hal::byte> p_data_in) override
  {
    last_address = p_address;

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

void i2c_configure_test() noexcept
{
  using namespace boost::ut;

  "configure() passes settings to driver"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_i2c test;
    hal::i2c::settings expected_settings{
      .clock_rate = 400 * kHz,
    };

    // Exercise
    auto result = test.configure(ctx, expected_settings);

    // Verify
    expect(expected_settings == test.configured_settings);
  };

  "configure() with default settings"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_i2c test;
    hal::i2c::settings default_settings{};

    // Exercise
    test.configure(ctx, default_settings);

    // Verify
    expect((100 * kHz) == test.configured_settings.clock_rate);
  };
};

void i2c_transaction_test() noexcept
{
  using namespace boost::ut;

  "transaction() with write-only data"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_i2c test;
    hal::byte address = 0x42;
    std::array<hal::byte, 4> write_buffer = { 0x01, 0x02, 0x03, 0x04 };
    auto write_data = hal::make_scatter_bytes(write_buffer);
    auto read_spans = hal::make_writable_scatter_bytes();

    // Exercise
    test.transaction(ctx, address, write_data, read_spans);

    // Verify
    expect(that % address == test.last_address);
    expect(that % 4 == test.last_data_out_size);
    expect(that % 0 == test.last_data_in_size);
  };

  "transaction() with read-only data"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_i2c test;
    hal::byte address = 0x43;
    auto write_spans = hal::make_scatter_bytes();
    std::array<hal::byte, 8> read_data{};
    auto read_spans = hal::make_writable_scatter_bytes(read_data);

    // Exercise
    test.transaction(ctx, address, write_spans, read_spans);

    // Verify
    expect(that % address == test.last_address);
    expect(that % 0 == test.last_data_out_size);
    expect(that % 8 == test.last_data_in_size);
  };

  "transaction() with write-then-read data"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_i2c test;
    hal::byte address = 0x44;
    std::array<hal::byte, 2> write_buffer = { 0xA0, 0xB0 };
    auto write_data = hal::make_scatter_bytes(write_buffer);
    std::array<hal::byte, 4> read_data{};
    auto read_spans = hal::make_writable_scatter_bytes(read_data);

    // Exercise
    test.transaction(ctx, address, write_data, read_spans);

    // Verify
    expect(that % address == test.last_address);
    expect(that % 2 == test.last_data_out_size);
    expect(that % 4 == test.last_data_in_size);
  };

  "transaction() with empty data"_test = [&]() {
    // Setup
    async::inplace_context<1024> ctx;
    test_i2c test;
    hal::byte address = 0x45;
    auto write_spans = hal::make_scatter_bytes();
    auto read_spans = hal::make_writable_scatter_bytes();

    // Exercise
    test.transaction(ctx, address, write_spans, read_spans);

    // Verify
    expect(that % address == test.last_address);
    expect(that % 0 == test.last_data_out_size);
    expect(that % 0 == test.last_data_in_size);
  };
};

}  // namespace

int main()
{
  i2c_configure_test();
  i2c_transaction_test();
}

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

#include <libhal/i2c.hpp>

#include <functional>

#include <libhal/error.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
constexpr hal::i2c::settings expected_settings{ .clock_rate = 1.0_Hz };
constexpr hal::byte expected_address{ 100 };
constexpr std::array<hal::byte, 4> expected_data_out{ 'a', 'b' };
std::array<hal::byte, 4> expected_data_in{ '1', '2' };
hal::function_ref<hal::timeout_function> const expected_timeout = []() {};

class test_i2c : public hal::i2c
{
public:
  settings m_settings{};
  hal::byte m_address{};
  std::span<hal::byte const> m_data_out{};
  std::span<hal::byte> m_data_in{};
  std::function<hal::timeout_function> m_timeout = []() {};

  ~test_i2c() override = default;

private:
  void driver_configure(settings const& p_settings) override
  {
    m_settings = p_settings;
    return;
  };
  void driver_transaction(
    hal::byte p_address,
    std::span<hal::byte const> p_data_out,
    std::span<hal::byte> p_data_in,
    hal::function_ref<hal::timeout_function> p_timeout) override
  {
    p_timeout();
    m_address = p_address;
    m_data_out = p_data_out;
    m_data_in = p_data_in;
    m_timeout = p_timeout;
  };
};
}  // namespace

void i2c_test()
{
  using namespace boost::ut;
  "i2c interface test"_test = []() {
    // Setup
    test_i2c test;

    // Exercise
    test.configure(expected_settings);
    test.transaction(
      expected_address, expected_data_out, expected_data_in, expected_timeout);

    // Verify
    expect(that % expected_settings.clock_rate == test.m_settings.clock_rate);
    expect(that % expected_address == test.m_address);
    expect(that % expected_data_out.data() == test.m_data_out.data());
    expect(that % expected_data_in.data() == test.m_data_in.data());
  };
};
}  // namespace hal

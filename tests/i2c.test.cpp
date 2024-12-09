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

#include <libhal/error.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
constexpr hal::byte expected_address{ 100 };
constexpr auto expected_data_out = std::to_array<hal::byte const>({ 'a', 'b' });
std::array<hal::byte, 2> expected_data_in{};

class test_i2c : public hal::i2c
{
public:
  hal::byte m_address{};
  std::span<hal::byte const> m_data_out{};
  std::span<hal::byte> m_data_in{};

  ~test_i2c() override = default;

private:
  u32 driver_clock_rate() override
  {
    return 100_kHz;
  };
  void driver_transaction(hal::byte p_address,
                          std::span<hal::byte const> p_data_out,
                          std::span<hal::byte> p_data_in) override
  {
    m_address = p_address;
    m_data_out = p_data_out;
    m_data_in = p_data_in;
  };
};
}  // namespace

boost::ut::suite<"i2c_test"> i2c_test = []() {
  using namespace boost::ut;
  "::clock_rate()"_test = []() {
    // Setup
    test_i2c test;

    // Verify
    expect(that % 100_kHz == test.clock_rate());
  };

  "::transaction(...)"_test = []() {
    // Setup
    test_i2c test;

    // Ensure
    expect(that % expected_address != test.m_address);
    expect(that % expected_data_out.data() != test.m_data_out.data());
    expect(that % expected_data_out.size() != test.m_data_out.size());
    expect(that % expected_data_in.data() != test.m_data_in.data());
    expect(that % expected_data_in.size() != test.m_data_in.size());

    // Exercise
    test.transaction(expected_address, expected_data_out, expected_data_in);

    // Verify
    expect(that % expected_address == test.m_address);
    expect(that % expected_data_out.data() == test.m_data_out.data());
    expect(that % expected_data_out.size() == test.m_data_out.size());
    expect(that % expected_data_in.data() == test.m_data_in.data());
    expect(that % expected_data_in.size() == test.m_data_in.size());
  };
};
}  // namespace hal

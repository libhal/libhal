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

#include <cstddef>
#include <libhal/zero_copy_serial.hpp>

#include <libhal/error.hpp>

#include <boost/ut.hpp>
#include <stdexcept>

namespace hal {
namespace {
constexpr hal::serial::settings expected_settings{
  .baud_rate = 115200.0f,
  .stop = hal::serial::settings::stop_bits::two,
  .parity = hal::serial::settings::parity::none
};

class test_serial : public hal::zero_copy_serial
{
public:
  using settings = hal::serial::settings;
  settings m_settings{};
  std::span<hal::byte const> m_data_write{};
  std::array<hal::byte, 4> m_working_buffer{};
  std::size_t m_cursor{};
  ~test_serial() override = default;

  void append_data_to_receive_buffer(std::span<hal::byte const> p_data)
  {
    if (p_data.size() > m_working_buffer.size()) {
      throw std::runtime_error("p_data is too big for this test API! Must be "
                               "smaller than m_working_buffer");
    }
    std::ranges::copy(p_data, m_working_buffer.begin());
    m_cursor = p_data.size();
  }

private:
  void driver_configure(settings const& p_settings) override
  {
    m_settings = p_settings;
  }

  void driver_write(std::span<hal::byte const> p_data) override
  {
    m_data_write = p_data;
  }

  std::span<hal::byte const> driver_receive_buffer() override
  {
    return m_working_buffer;
  }

  std::size_t driver_cursor() override
  {
    return m_cursor;
  }
};
}  // namespace

boost::ut::suite<"zero_copy_serial_test"> zero_copy_serial_test = []() {
  using namespace boost::ut;

  "::configure()"_test = []() {
    // Setup
    test_serial test;

    // Ensure
    expect(expected_settings != test.m_settings);

    // Exercise
    test.configure(expected_settings);

    // Verify
    expect(expected_settings == test.m_settings);
  };

  "::write"_test = []() {
    // Setup
    constexpr auto expected_payload =
      std::to_array<hal::byte const>({ 'a', 'b' });
    test_serial test;

    // Ensure
    expect(expected_payload.data() != test.m_data_write.data());
    expect(expected_payload.size() != test.m_data_write.size());

    // Exercise
    test.write(expected_payload);

    // Verify
    expect(expected_payload.data() == test.m_data_write.data());
    expect(expected_payload.size() == test.m_data_write.size());
  };

  "::receive_buffer() & ::receive_cursor()"_test = []() {
    // Setup
    constexpr auto expected_buffer = std::to_array<hal::byte>({ '1', '2' });
    test_serial test;

    // Exercise
    auto const receive_buffer = test.receive_buffer();
    auto const cursor1 = static_cast<std::ptrdiff_t>(test.receive_cursor());

    test.append_data_to_receive_buffer(expected_buffer);

    auto const cursor2 = static_cast<std::ptrdiff_t>(test.receive_cursor());
    auto const cursor_distance = static_cast<std::ptrdiff_t>(cursor2 - cursor1);

    bool const the_are_buffers_equal =
      std::equal(receive_buffer.begin() + cursor1,
                 receive_buffer.begin() + cursor2,
                 expected_buffer.begin());

    // Verify
    expect(that % expected_buffer.size() == cursor_distance);
    expect(that % the_are_buffers_equal)
      << "Expected buffer and cursors + buffers are not equal!";
  };
};
}  // namespace hal

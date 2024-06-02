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

#include <libhal/stream_dac.hpp>

#include <libhal/error.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
class test_stream_dac : public hal::stream_dac_u8
{
public:
  virtual ~test_stream_dac() = default;

  samples actual_samples;

private:
  void driver_write(const samples& p_samples) override
  {
    actual_samples = p_samples;
  }
};
}  // namespace

void stream_dac_test()
{
  using namespace boost::ut;
  "stream_dac interface test"_test = []() {
    // Setup
    test_stream_dac test;
    const std::array<std::uint8_t, 7> expected_out{ 0, 1, 2, 3, 4, 5 };
    constexpr hal::hertz expected_sample_rate = 16.0_kHz;

    // Exercise
    test.write({ .sample_rate = expected_sample_rate, .data = expected_out });

    // Verify
    expect(that % expected_sample_rate == test.actual_samples.sample_rate);
    expect(that % expected_out.data() == test.actual_samples.data.data());
    expect(that % expected_out.size() == test.actual_samples.data.size());
  };
};
}  // namespace hal

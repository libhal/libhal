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

#include <libhal/adc.hpp>

#include <libhal/error.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
constexpr auto expected_value = float(0.5);

class test_adc : public hal::adc
{
public:
  constexpr static float m_returned_position{ 0.5f };
  ~test_adc() override = default;

private:
  float driver_read() override
  {
    return m_returned_position;
  }
};
}  // namespace

boost::ut::suite<"hal::adc"> adc_test = []() {
  using namespace boost::ut;
  "::read()"_test = []() {
    // Setup
    test_adc test;

    // Exercise
    auto sample = test.read();

    // Verify
    expect(that % expected_value == sample);
  };
};
}  // namespace hal

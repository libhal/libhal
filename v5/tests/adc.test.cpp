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

#include <boost/ut.hpp>

import hal;

namespace {

class test_adc16 : public hal::adc16
{
public:
  constexpr static hal::u16 returned_position = ((1U << 16U) - 1U) / 2U;
  ~test_adc16() override = default;

private:
  hal::u16 driver_read() override
  {
    return returned_position;
  }
};

boost::ut::suite<"hal::adc16"> adc16_test = []() {
  using namespace boost::ut;
  "::read()"_test = []() {
    // Setup
    test_adc16 test;

    // Exercise
    auto sample = test.read();

    // Verify
    expect(that % test_adc16::returned_position == sample);
  };
};

class test_adc24 : public hal::adc24
{
public:
  constexpr static hal::u32 returned_position = ((1U << 24U) - 1U) / 2U;
  ~test_adc24() override = default;

private:
  hal::u32 driver_read() override
  {
    return returned_position;
  }
};

boost::ut::suite<"hal::adc24"> adc24_test = []() {
  using namespace boost::ut;
  "::read()"_test = []() {
    // Setup
    test_adc24 test;

    // Exercise
    auto sample = test.read();

    // Verify
    expect(that % test_adc24::returned_position == sample);
  };
};

}  // namespace

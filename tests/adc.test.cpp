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

class test_adc16 : public hal::adc16
{
public:
  static constexpr auto expected_value = (1 << 15) - 1;
  ~test_adc16() override = default;

private:
  u16 driver_read() override
  {
    return expected_value;
  }
};

class test_adc24 : public hal::adc24
{
public:
  static constexpr auto expected_value = (1 << 23) - 1;
  ~test_adc24() override = default;

private:
  u32 driver_read() override
  {
    return expected_value;
  }
};

class test_adc32 : public hal::adc32
{
public:
  static constexpr auto expected_value = (1 << 30) - 1;
  ~test_adc32() override = default;

private:
  u32 driver_read() override
  {
    return expected_value;
  }
};
}  // namespace

boost::ut::suite<"hal::adc16"> adc16_test = []() {
  using namespace boost::ut;
  "::read()"_test = []() {
    // Setup
    test_adc16 test;

    // Exercise
    auto sample = test.read();

    // Verify
    expect(that % test_adc16::expected_value == sample);
  };
};

boost::ut::suite<"hal::adc24"> adc24_test = []() {
  using namespace boost::ut;
  "::read()"_test = []() {
    // Setup
    test_adc24 test;

    // Exercise
    auto sample = test.read();

    // Verify
    expect(that % test_adc24::expected_value == sample);
  };
};

boost::ut::suite<"hal::adc32"> adc32_test = []() {
  using namespace boost::ut;
  "::read()"_test = []() {
    // Setup
    test_adc32 test;

    // Exercise
    auto sample = test.read();

    // Verify
    expect(that % test_adc32::expected_value == sample);
  };
};
}  // namespace hal

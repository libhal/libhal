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

namespace {
class test_adc16 : public hal::adc16
{
public:
  constexpr static hal::u16 returned_position = ((1U << 16U) - 1U) / 2U;
  ~test_adc16() override = default;

private:
  async::future<hal::u16> driver_read(async::context&) override
  {
    // WARNING co_return seems to loop forever!!
    return returned_position;
  }
};

void adc16_test() noexcept
{
  using namespace boost::ut;

  "::read()"_test = [&]() {
    // Setup
    async::basic_context<1024> ctx;
    test_adc16 test;

    // Exercise
    auto sample = test.read(ctx);

    // Verify
    expect(that % sample.has_value());
    expect(that % test_adc16::returned_position == sample.value());
  };
};

class test_adc24 : public hal::adc24
{
public:
  constexpr static hal::u32 returned_position = ((1U << 24U) - 1U) / 2U;
  ~test_adc24() override = default;

private:
  async::future<hal::u32> driver_read(async::context&) override
  {
    return returned_position;
  }
};

void adc24_test() noexcept
{
  using namespace boost::ut;

  "::read()"_test = [&]() {
    // Setup
    async::basic_context<1024> context;
    test_adc24 test;

    // Exercise
    auto sample = test.read(context);

    // Verify
    expect(that % sample.has_value());
    expect(that % test_adc24::returned_position == sample.value());
  };
};

}  // namespace

int main()
{
  adc16_test();
  adc24_test();
}

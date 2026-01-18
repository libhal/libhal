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

struct test_context : public async::basic_context
{
  std::array<async::uptr, 1024> m_stack_memory{};

  test_context()
  {
    initialize_stack_memory(m_stack_memory);
  }

  ~test_context() override
  {
    // cancel();
  }

  void do_schedule(async::blocked_by, async::block_info) noexcept override
  {
  }
};

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

boost::ut::suite<"hal::adc16"> adc16_test = []() {
  using namespace boost::ut;

  "::read()"_test = [&]() {
    // Setup
    test_context context;
    test_adc16 test;

    // Exercise
    auto sample = test.read(context);

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

boost::ut::suite<"hal::adc24"> adc24_test = []() {
  using namespace boost::ut;

  "::read()"_test = [&]() {
    // Setup
    test_context context;
    test_adc24 test;

    // Exercise
    auto sample = test.read(context);

    // Verify
    expect(that % sample.has_value());
    expect(that % test_adc24::returned_position == sample.value());
  };
};

}  // namespace

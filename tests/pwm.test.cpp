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

#include <libhal/pwm.hpp>

#include <libhal/error.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
static constexpr u32 expected_frequency = 15_kHz;
constexpr auto expected_duty_cycle = 1337;

class test_pwm16 : public hal::pwm16
{
public:
  u32 m_frequency{};
  u16 m_duty_cycle{};
  ~test_pwm16() override = default;

private:
  void driver_frequency(u32 p_frequency) override
  {
    m_frequency = p_frequency;
  }
  void driver_duty_cycle(u16 p_duty_cycle) override
  {
    m_duty_cycle = p_duty_cycle;
  }
};

class test_pwm_duty_cycle16 : public hal::pwm_duty_cycle16
{
public:
  u16 m_duty_cycle{};
  ~test_pwm_duty_cycle16() override = default;

private:
  u32 driver_frequency() override
  {
    return expected_frequency;
  }
  void driver_duty_cycle(u16 p_duty_cycle) override
  {
    m_duty_cycle = p_duty_cycle;
  }
};
}  // namespace

boost::ut::suite<"pwm16"> pwm16_test = []() {
  using namespace boost::ut;
  // Setup
  test_pwm16 test;

  // Exercise
  test.frequency(expected_frequency);
  test.duty_cycle(expected_duty_cycle);

  // Verify
  expect(that % expected_frequency == test.m_frequency);
  expect(that % expected_duty_cycle == test.m_duty_cycle);
};

boost::ut::suite<"test_pwm_duty_cycle16"> pwm_duty_cycle16_test = []() {
  using namespace boost::ut;
  // Setup
  test_pwm_duty_cycle16 test;

  // Exercise
  test.duty_cycle(expected_duty_cycle);

  // Verify
  expect(that % expected_frequency == test.frequency());
  expect(that % expected_duty_cycle == test.m_duty_cycle);
};
}  // namespace hal

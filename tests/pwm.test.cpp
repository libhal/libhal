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
class test_pwm16_channel : public hal::pwm16_channel
{
public:
  static constexpr auto expected_frequency = 1'000;
  static constexpr auto expected_duty_cycle = 1U << 15U;

  u16 m_duty_cycle{};
  ~test_pwm16_channel() override = default;

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

boost::ut::suite<"pwm16_channel_test"> pwm16_channel_test = []() {
  using namespace boost::ut;
  "pwm interface test"_test = []() {
    // Setup
    test_pwm16_channel test;

    // Exercise
    auto const actual_frequency = test.frequency();
    test.duty_cycle(test_pwm16_channel::expected_duty_cycle);

    // Verify
    expect(that % test_pwm16_channel::expected_duty_cycle == test.m_duty_cycle);
    expect(that % test_pwm16_channel::expected_frequency == actual_frequency);
  };
};

namespace {
class test_pwm_group_manager : public hal::pwm_group_manager
{
public:
  static constexpr auto expected_frequency = 15'250;
  u32 m_frequency{};
  ~test_pwm_group_manager() override = default;

private:
  void driver_frequency(u32 p_frequency) override
  {
    m_frequency = p_frequency;
  }
};
}  // namespace

boost::ut::suite<"pwm_group_manager_test"> pwm_group_manager_test = []() {
  using namespace boost::ut;
  "pwm interface test"_test = []() {
    // Setup
    test_pwm_group_manager test;

    // Exercise
    test.frequency(test_pwm_group_manager::expected_frequency);

    // Verify
    expect(that % test_pwm_group_manager::expected_frequency ==
           test.m_frequency);
  };
};
}  // namespace hal

namespace hal {
namespace {

class test_pwm : public hal::pwm
{
public:
  static constexpr auto expected_frequency = hertz(1'000);
  static constexpr auto expected_duty_cycle = float(0.5);
  hertz m_frequency{};
  float m_duty_cycle{};
  ~test_pwm() override = default;

private:
  void driver_frequency(hertz p_frequency) override
  {
    m_frequency = p_frequency;
  }
  void driver_duty_cycle(float p_duty_cycle) override
  {
    m_duty_cycle = p_duty_cycle;
  }
};
}  // namespace

boost::ut::suite<"pwm_test"> pwm_test = []() {
  using namespace boost::ut;
  "pwm interface test"_test = []() {
    // Setup
    test_pwm test;

    // Exercise
    test.frequency(test_pwm::expected_frequency);
    test.duty_cycle(test_pwm::expected_duty_cycle);

    // Verify
    expect(that % test_pwm::expected_frequency == test.m_frequency);
    expect(that % test_pwm::expected_duty_cycle == test.m_duty_cycle);
  };
};
}  // namespace hal

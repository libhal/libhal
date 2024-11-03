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

#include <libhal/motor.hpp>

#include <libhal/error.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
constexpr auto expected_value = float(0.5);

class test_motor : public hal::motor
{
public:
  float m_power{};
  ~test_motor() override = default;

private:
  void driver_power(float power) override
  {
    m_power = power;
  };
};
}  // namespace

boost::ut::suite<"motor_test"> motor_test = []() {
  using namespace boost::ut;
  "motor interface test"_test = []() {
    // Setup
    test_motor test;

    // Exercise
    test.power(expected_value);

    // Verify
    expect(that % expected_value == test.m_power);
  };
};
}  // namespace hal

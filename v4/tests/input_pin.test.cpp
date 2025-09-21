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

#include <libhal/input_pin.hpp>

#include <libhal/error.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
constexpr hal::input_pin::settings expected_settings{
  .resistor = pin_resistor::pull_down,
};
class test_input_pin : public hal::input_pin
{
public:
  settings m_settings{};
  ~test_input_pin() override = default;

private:
  void driver_configure(settings const& p_settings) override
  {
    m_settings = p_settings;
  }
  bool driver_level() override
  {
    return true;
  }
};
}  // namespace

boost::ut::suite<"input_pin_test"> input_pin_test = []() {
  using namespace boost::ut;
  "input_pin interface test"_test = []() {
    // Setup
    test_input_pin test;

    // Exercise
    test.configure(expected_settings);
    auto level = test.level();

    // Verify
    expect(expected_settings.resistor == test.m_settings.resistor);
    expect(that % true == level);
  };
};
}  // namespace hal

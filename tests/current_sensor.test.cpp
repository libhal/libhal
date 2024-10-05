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

#include <libhal/current_sensor.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {

constexpr auto expected_value = hal::ampere(0.5);
class test_current_sensor : public hal::current_sensor
{
public:
  ~test_current_sensor() override = default;

private:
  hal::ampere driver_read() override
  {
    return expected_value;
  }
};
}  // namespace

void current_sensor_test()
{
  using namespace boost::ut;
  "current sensor interface test"_test = []() {
    test_current_sensor test;

    auto sample = test.read();

    expect(expected_value == sample);
  };
}

}  // namespace hal

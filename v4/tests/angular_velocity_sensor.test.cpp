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

#include <libhal/angular_velocity_sensor.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
class test_angular_velocity_sensor : public hal::angular_velocity_sensor
{
public:
  ~test_angular_velocity_sensor() override = default;

private:
  hal::rpm driver_read() override
  {
    return hal::rpm(5.0);
  }
};
}  // namespace

boost::ut::suite<"angular_velocity_sensor_test"> angular_velocity_sensor_test =
  []() {
    using namespace boost::ut;
    "angular velocity sensor interface test"_test = []() {
      test_angular_velocity_sensor test;

      auto result = test.read();

      expect(hal::rpm(5.0) == result);
    };
  };
}  // namespace hal

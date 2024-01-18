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
class test_current_sensor : public hal::current_sensor
{
public:
  bool m_return_error_status{ false };
  ~test_current_sensor() override = default;

private:
  result<read_t> driver_read() override
  {
    if (m_return_error_status) {
      return hal::new_error();
    }
    return read_t{};
  }
};
}  // namespace

void current_sensor_test()
{
  using namespace boost::ut;
  "current sensor interface test"_test = []() {
    test_current_sensor test;

    auto result = test.read();

    expect(bool{ result });
  };
  "current sensor errors test"_test = []() {
    test_current_sensor test;
    test.m_return_error_status = true;

    auto result = test.read();

    expect(!bool{ result });
  };
}

}  // namespace hal
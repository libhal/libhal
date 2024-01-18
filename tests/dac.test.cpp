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

#include <libhal/dac.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
constexpr auto expected_value = float(0.5);
class test_dac : public hal::dac
{
public:
  float m_passed_value{};
  bool m_return_error_status{ false };

  ~test_dac() override = default;

private:
  result<write_t> driver_write(float p_value) override
  {
    m_passed_value = p_value;
    if (m_return_error_status) {
      return hal::new_error();
    }
    return write_t{};
  }
};
}  // namespace

void dac_test()
{
  using namespace boost::ut;

  "dac interface test"_test = []() {
    using namespace boost::ut;
    // Setup
    test_dac test;

    // Exercise
    auto result = test.write(expected_value);

    // Verify
    expect(bool{ result });
    expect(that % expected_value == test.m_passed_value);
  };

  "dac errors test"_test = []() {
    // Setup
    test_dac test;
    test.m_return_error_status = true;

    // Exercise
    auto result = test.write(expected_value);

    // Verify
    expect(!bool{ result });
  };
};
}  // namespace hal

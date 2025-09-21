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

#include <libhal/steady_clock.hpp>

#include <libhal/error.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
class test_steady_clock : public hal::steady_clock
{
public:
  constexpr static hertz m_frequency{ 1.0_Hz };
  constexpr static std::uint64_t m_uptime{ 100 };

  ~test_steady_clock() override = default;

private:
  hertz driver_frequency() override
  {
    return m_frequency;
  };

  std::uint64_t driver_uptime() override
  {
    return m_uptime;
  };
};
}  // namespace

boost::ut::suite<"steady_clock_test"> steady_clock_test = []() {
  using namespace boost::ut;
  // Setup
  test_steady_clock test;

  // Exercise
  auto frequency = test.frequency();
  auto uptime = test.uptime();

  // Verify
  expect(that % test.m_frequency == frequency);
  expect(that % test.m_uptime == uptime);
};
}  // namespace hal

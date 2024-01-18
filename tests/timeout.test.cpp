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

#include <libhal/timeout.hpp>

#include <boost/ut.hpp>

namespace hal {
void timeout_test()
{
  using namespace boost::ut;

  "hal::delay(timeout)"_test = []() {
    // Setup
    constexpr int timeout_call_limit = 10;
    int counts = 0;
    auto test_timeout_function = [&counts]() mutable -> status {
      counts++;
      if (counts >= timeout_call_limit) {
        return hal::new_error(std::errc::timed_out);
      }
      return {};
    };
    using timeout_type = decltype(test_timeout_function);

    // Exercise
    // Exercise: direct
    auto result = hal::delay<timeout_type>(test_timeout_function);
    // Exercise: Reset
    counts = 0;
    // Exercise: polymorphic
    result = hal::delay(test_timeout_function);

    // Verify
    expect(that % timeout_call_limit == counts);
    expect(bool{ result });
  };

  "hal::delay(timeout) returns error"_test = []() {
    // Setup
    auto test_timeout_function = []() mutable -> status {
      return hal::new_error();
    };

    // Exercise
    auto result = hal::delay(test_timeout_function);

    // Verify
    expect(!bool{ result });
  };
};
}  // namespace hal

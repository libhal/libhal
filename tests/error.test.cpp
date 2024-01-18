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

#include <libhal/error.hpp>

#include <utility>

#include <boost/ut.hpp>

namespace hal {
namespace {
int callback_call_count = 0;
}
void error_test()
{
  using namespace boost::ut;

  on_error_callback = []() { callback_call_count++; };

  "[success] hal::on_error calls callback"_test = []() {
    // Setup
    auto current_call_count = callback_call_count;
    expect(that % current_call_count == callback_call_count);

    // Exercise
    // Should call the `on_error_callback` defined in the tweaks file
    (void)new_error();

    // Verify
    expect(that % current_call_count < callback_call_count);
  };

  "[success] hal::attempt calls handler"_test = []() {
    // Setup
    static const int expected = 123456789;
    int value_to_be_change = 0;

    // Exercise
    // Should call the `on_error_callback` defined in the tweaks file
    auto result = attempt([]() -> status { return new_error(expected); },
                          [&value_to_be_change](int p_handler_value) -> status {
                            value_to_be_change = p_handler_value;
                            return {};
                          });

    // Verify
    expect(that % value_to_be_change == expected);
    expect(that % true == bool{ result });
  };

  "[success] hal::attempt calls handler"_test = []() {
    // Setup
    constexpr int expected = 123456789;
    int value_to_be_change = 0;

    // Exercise
    // Should call the `on_error_callback` defined in the tweaks file

    attempt_all([]() -> status { return new_error(); },
                [&value_to_be_change]() { value_to_be_change = expected; });

    // Verify
    expect(that % value_to_be_change == expected);
  };
};
}  // namespace hal

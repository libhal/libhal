// Copyright 2026 Khalil Estell and the libhal contributors
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

#include <exception>
#include <string_view>
#include <system_error>
#include <type_traits>

#include <boost/ut.hpp>

import hal;

namespace {

static_assert(std::is_base_of_v<std::exception, hal::exception>);
static_assert(std::is_base_of_v<std::exception, hal::io_error>);
static_assert(std::is_base_of_v<hal::exception, hal::subscription_busy>);
static_assert(std::is_base_of_v<hal::exception, hal::reusable_busy>);

void error_test()
{
  using namespace boost::ut;

  "hal exceptions can be caught as std::exception"_test = []() {
    // Setup
    int const driver = 0;
    std::string_view message;

    // Exercise
    try {
      throw hal::io_error(&driver);
    } catch (std::exception const& p_error) {
      message = p_error.what();
    }

    // Verify
    expect(message == "hal::io_error");
  };

  "hal::exception keeps its error code and instance"_test = []() {
    // Setup
    int const driver = 0;
    std::errc code{};
    void const* instance = nullptr;

    // Exercise
    try {
      throw hal::timed_out(&driver);
    } catch (hal::exception const& p_error) {
      code = p_error.error_code();
      instance = p_error.instance();
    }

    // Verify
    expect(std::errc::timed_out == code);
    expect(&driver == instance);
  };

  "busy exceptions are hal::exceptions with their own names"_test = []() {
    // Setup
    int const source = 0;
    hal::subscription_busy const subscription(&source);
    hal::reusable_busy const reusable(&source);

    // Exercise + Verify
    expect(std::errc::device_or_resource_busy == subscription.error_code());
    expect(std::errc::device_or_resource_busy == reusable.error_code());
    expect(&source == subscription.instance());
    expect(std::string_view(subscription.what()) == "hal::subscription_busy");
    expect(std::string_view(reusable.what()) == "hal::reusable_busy");
  };

  "what() names each exception"_test = []() {
    // Setup
    int const driver = 0;

    // Exercise + Verify
    expect(std::string_view(hal::no_such_device(0x42, &driver).what()) ==
           "hal::no_such_device");
    expect(std::string_view(hal::unknown(&driver).what()) == "hal::unknown");
    expect(std::string_view(hal::operation_not_supported(&driver).what()) ==
           "hal::operation_not_supported");
  };
}

}  // namespace

int main()
{
  error_test();
}

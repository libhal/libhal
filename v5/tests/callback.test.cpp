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

#include <boost/ut.hpp>

import hal;

boost::ut::suite<"hal::callback"> callback_test = []() {
  using namespace boost::ut;

  "void() function"_test = []() {
    // Setup
    bool called = false;
    hal::callback<void()> callback = [&called]() { called = true; };

    // Exercise
    callback();

    // Verify
    expect(that % called == true);
  };

  "int(int) function"_test = []() {
    // Setup
    constexpr int expected_value = 42;
    hal::callback<int(int)> callback = [](int value) { return value * 2; };

    // Exercise
    auto result = callback(expected_value / 2);

    // Verify
    expect(that % expected_value == result);
  };

  "void(int) function with capture"_test = []() {
    // Setup
    int captured_value = 0;
    constexpr int test_value = 100;
    hal::callback<void(int)> callback = [&captured_value](int value) {
      captured_value = value;
    };

    // Exercise
    callback(test_value);

    // Verify
    expect(that % test_value == captured_value);
  };

  "copy assignment"_test = []() {
    // Setup
    bool first_called = false;
    bool second_called = false;
    hal::callback<void()> callback1 = [&first_called]() {
      first_called = true;
    };
    hal::callback<void()> callback2 = [&second_called]() {
      second_called = true;
    };

    // Exercise
    callback1 = callback2;
    callback1();

    // Verify
    expect(that % false == first_called);
    expect(that % true == second_called);
  };

  "bool operator check"_test = []() {
    // Setup
    hal::callback<void()> empty_callback;
    hal::callback<void()> valid_callback = []() {};

    // Verify
    expect(that % false == static_cast<bool>(empty_callback));
    expect(that % true == static_cast<bool>(valid_callback));
  };
};

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

#include <boost/ut.hpp>

import std;
import hal;

namespace {
using returns_bool = decltype([] { return true; });
using returns_int = decltype([] { return 1; });
using returns_u32 = decltype([] { return hal::u32{ 1 }; });
using takes_u32 = decltype([](hal::u32) {});
using takes_nothing = decltype([] {});
using never_throws = decltype([]() noexcept {});
struct only_callable_as_rvalue
{
  void operator()() &&;
};
struct only_callable_when_mutable
{
  bool operator()();
};

// The return type must match exactly
static_assert(hal::callable<returns_bool, bool()>);
static_assert(not hal::callable<returns_int, bool()>);
static_assert(not hal::callable<returns_u32, bool()>);
static_assert(not hal::callable<returns_bool, hal::u32()>);
static_assert(not hal::callable<takes_nothing, bool()>);
static_assert(hal::callable<returns_u32, hal::u32()>);

// The arguments must be passable, with the usual conversions
static_assert(hal::callable<takes_u32, void(hal::u32)>);
static_assert(hal::callable<takes_u32, void(hal::u8)>);
static_assert(not hal::callable<takes_u32, void()>);
static_assert(not hal::callable<takes_nothing, void(hal::u32)>);

// noexcept in the signature requires a call that can't throw
static_assert(hal::callable<never_throws, void() noexcept>);
static_assert(hal::callable<never_throws, void()>);
static_assert(not hal::callable<takes_nothing, void() noexcept>);

// Checked as a call through an lvalue, the way a named parameter is called
static_assert(not hal::callable<only_callable_as_rvalue, void()>);
static_assert(hal::callable<only_callable_when_mutable, bool()>);
static_assert(not hal::callable<only_callable_when_mutable const, bool()>);

// A Signature that isn't a function type is never satisfied
static_assert(not hal::callable<returns_bool, bool>);

// A function using it, as an API would
int call_twice(hal::callable<int()> auto&& p_function)
{
  return p_function() + p_function();
}

void functional_test()
{
  using namespace boost::ut;

  "hal::callable constrains an auto parameter"_test = []() {
    // Setup
    int calls = 0;
    auto const count = [&calls] {
      calls++;
      return calls;
    };

    // Exercise
    auto const result = call_twice(count);

    // Verify
    expect(that % 3 == result);
    expect(that % 2 == calls);
  };
}
}  // namespace

int main()
{
  functional_test();
}

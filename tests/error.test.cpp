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

#include <boost/ut.hpp>

namespace hal {
void error_test()
{
  using namespace boost::ut;

  "[success] hal::safe_throw"_test = []() {
    // Setup
    bool exception_thrown = false;

    try {
      hal::safe_throw(hal::timed_out(nullptr));
    } catch (hal::timed_out const&) {
      exception_thrown = true;
    }

    expect(exception_thrown);
  };

#define TEST_COMPILE_TIME_FAILURE 0
#if TEST_COMPILE_TIME_FAILURE
  "[failure] hal::safe_throw(non-trivial-dtor)"_test = []() {
    // Setup
    bool exception_thrown = false;

    struct dtor_t : hal::exception
    {
      dtor_t(float p_value)
        : hal::exception(std::errc{}, nullptr)
        , value(p_value)
      {
      }

      ~dtor_t()
      {
        value = 0;
      }

      float value;
    };

    try {
      hal::safe_throw(dtor_t(5.0f));
    } catch (dtor_t const&) {
      exception_thrown = true;
    }

    expect(exception_thrown);
  };

  "[failure] hal::safe_throw(non_hal_exception_type)"_test = []() {
    // Setup
    bool exception_thrown = false;

    try {
      hal::safe_throw(5);
    } catch (int const&) {
      exception_thrown = true;
    }

    expect(exception_thrown);
  };
#endif
};
}  // namespace hal

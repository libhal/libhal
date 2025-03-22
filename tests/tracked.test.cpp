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

#include "libhal/spi.hpp"
#include <libhal/output_pin.hpp>
#include <libhal/tracked.hpp>

#include <libhal/error.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {

class custom_output_pin : public hal::output_pin
{
public:
  custom_output_pin() = default;
  ~custom_output_pin() override = default;

private:
  void driver_configure(settings const&) override
  {
    puts("`configure` called!");
  }
  void driver_level(bool) override
  {
    puts("`level(bool)` called!");
  }
  bool driver_level() override
  {
    puts("`bool level()` called!");
    return false;
  }
};

class blinker
{
public:
  explicit blinker(tracked_ptr<hal::output_pin>&& p_ptr)
    : m_ptr(std::move(p_ptr))
  {
  }

  void blink()
  {
    m_ptr->level(true);
    m_ptr->level(false);
  }

  tracked_ptr<hal::output_pin> m_ptr;
};

}  // namespace
boost::ut::suite<"tracked_ptr"> tracked_ptr_test = [] {
  using namespace boost::ut;
  "lifetime_observer"_test = []() {
    // Setup

    [[maybe_unused]] auto test_scope = []() {
      unique_tracked<hal::custom_output_pin> test_pin;
      blinker my_blinker(test_pin.get_tracked_pointer());
      my_blinker.blink();
      return my_blinker;
    }();

    // Exercise
    try {
      test_scope.blink();
    } catch (hal::lifetime_violation const&) {
      puts("Lifetime was violated!!!");
    } catch (...) {
      puts("Unknown exception!!!");
    }

    // Verify
    // expect(that % nullptr == test_scope.get());
    // expect(that % nullptr == ptrb1.get());
  };
};
}  // namespace hal

// Examples of what should be possible:

#include <libhal/i2c.hpp>

namespace hal {
template<typename T>
struct mutex
{};

class pca8265
{
public:
  explicit pca8265(hal::tracked_ptr<mutex<hal::i2c>>&& p_ptr)
    : m_ptr(std::move(p_ptr))
  {
  }

  tracked_ptr<mutex<hal::i2c>> m_ptr;
};

class display
{
public:
  explicit pca8265(hal::tracked_ptr<mutex<hal::spi_channel>>&& p_ptr)
    : m_ptr(std::move(p_ptr))
  {
  }

  tracked_ptr<mutex<hal::i2c>> m_ptr;
};
}  // namespace hal

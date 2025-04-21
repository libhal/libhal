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

#include <libhal/initializers.hpp>
#include <libhal/output_pin.hpp>
#include <libhal/smart_ref.hpp>
#include <libhal/units.hpp>

#include <libhal/error.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
class test_output_pin : public hal::output_pin
{
public:
  settings m_settings{};
  bool m_driver_level{};

  ~test_output_pin() override = default;

private:
  void driver_configure(settings const& p_settings) override
  {
    m_settings = p_settings;
  }
  void driver_level(bool p_high) override
  {
    m_driver_level = p_high;
  }
  bool driver_level() override
  {
    return m_driver_level;
  }
};

template<usize buffer_size>
struct arena
{
  arena() = default;
  arena(arena&) = delete;
  arena& operator=(arena&) = delete;
  arena(arena&&) = default;
  arena& operator=(arena&&) = default;
  ~arena() = default;

  auto& allocator()
  {
    return m_allocator;
  }

  void release(hal::unsafe)
  {
    m_resource.release();
    m_driver_memory.fill(0);
  }

private:
  // Create a polymorphic allocator using the monotonic buffer resource
  std::array<hal::byte, buffer_size> m_driver_memory{};
  std::pmr::monotonic_buffer_resource m_resource{
    m_driver_memory.data(),
    m_driver_memory.size(),
    std::pmr::null_memory_resource()
  };
  std::pmr::polymorphic_allocator<hal::byte> m_allocator{ &m_resource };
};

template<class = decltype([]() {})>
auto& static_arena(hal::buffer_param auto p_buffer_size)
{
  static arena<p_buffer_size()> allocator_object{};
  return allocator_object;
}

}  // namespace

boost::ut::suite<"output_pin_test"> output_pin_test = []() {
  using namespace boost::ut;
  "test"_test = []() {
    // Setup
    auto mem = static_arena(hal::buffer<1024>);
    // auto test = ;

    // Exercise
    test.configure(expected_settings);
    test.level(true);
    auto level = test.level();

    // Verify
    expect(expected_settings.open_drain == test.m_settings.open_drain);
    expect(expected_settings.resistor == test.m_settings.resistor);
    expect(that % true == test.m_driver_level);
    expect(that % true == level);
  };
};
}  // namespace hal

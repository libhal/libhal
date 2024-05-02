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

#include <span>

#include <libhal/error.hpp>
#include <libhal/serial.hpp>
#include <libhal/units.hpp>

#include <boost/ut.hpp>

namespace hal {
namespace {
struct test_class_allocator
{
  test_class_allocator(port_param auto p_port, buffer_param auto p_buffer)
    : m_buffer(create_unique_static_buffer(p_buffer))
    , m_port(p_port())
  {
    static_assert(0 <= p_port() and p_port() <= 2,
                  "Supported ports are 0 to 2");
  }

  // Made public for inspection
  std::span<hal::byte> m_buffer{};
  std::uint8_t m_port;
};
}  // namespace

void initializers_test()
{
  using namespace boost::ut;
  "create_unique_static_buffer()"_test = []() {
    // Setup
    auto span1 = hal::create_unique_static_buffer(buffer<128>);
    auto span2 = hal::create_unique_static_buffer(buffer<128>);
    auto span3 = hal::create_unique_static_buffer(buffer<256>);
    auto span4 = hal::create_unique_static_buffer(buffer<64>);
    auto span5 = hal::create_unique_static_buffer(buffer<72>);

    test_class_allocator test1(port<2>, buffer<128>);
    test_class_allocator test2(port<0>, buffer<512>);
    test_class_allocator test3(port<1>, buffer<128>);

    // Exercise
    // Verify
    // Verify: Sizes of each span
    expect(that % 128 == span1.size());
    expect(that % 128 == span2.size());
    expect(that % 256 == span3.size());
    expect(that % 64 == span4.size());
    expect(that % 72 == span5.size());
    expect(that % 128 == test1.m_buffer.size());
    expect(that % 512 == test2.m_buffer.size());
    expect(that % 128 == test3.m_buffer.size());

    expect(that % span1.data() == span1.data());
    expect(that % span1.data() != span2.data());
    expect(that % span1.data() != span3.data());
    expect(that % span1.data() != span4.data());
    expect(that % span1.data() != span5.data());
    expect(that % span1.data() != test1.m_buffer.data());
    expect(that % span1.data() != test2.m_buffer.data());
    expect(that % span1.data() != test3.m_buffer.data());

    expect(that % span2.data() != span1.data());
    expect(that % span2.data() == span2.data());
    expect(that % span2.data() != span3.data());
    expect(that % span2.data() != span4.data());
    expect(that % span2.data() != span5.data());
    expect(that % span2.data() != test1.m_buffer.data());
    expect(that % span2.data() != test2.m_buffer.data());
    expect(that % span2.data() != test3.m_buffer.data());

    expect(that % span3.data() != span1.data());
    expect(that % span3.data() != span2.data());
    expect(that % span3.data() == span3.data());
    expect(that % span3.data() != span4.data());
    expect(that % span3.data() != span5.data());
    expect(that % span3.data() != test1.m_buffer.data());
    expect(that % span3.data() != test2.m_buffer.data());
    expect(that % span3.data() != test3.m_buffer.data());

    expect(that % span4.data() != span1.data());
    expect(that % span4.data() != span2.data());
    expect(that % span4.data() != span3.data());
    expect(that % span4.data() == span4.data());
    expect(that % span4.data() != span5.data());
    expect(that % span4.data() != test1.m_buffer.data());
    expect(that % span4.data() != test2.m_buffer.data());
    expect(that % span4.data() != test3.m_buffer.data());

    expect(that % span5.data() != span1.data());
    expect(that % span5.data() != span2.data());
    expect(that % span5.data() != span3.data());
    expect(that % span5.data() != span4.data());
    expect(that % span5.data() == span5.data());
    expect(that % span5.data() != test1.m_buffer.data());
    expect(that % span5.data() != test2.m_buffer.data());
    expect(that % span5.data() != test3.m_buffer.data());

    expect(that % test1.m_buffer.data() != test2.m_buffer.data());
    expect(that % test1.m_buffer.data() != test3.m_buffer.data());
    expect(that % test2.m_buffer.data() != test3.m_buffer.data());
  };
};
}  // namespace hal

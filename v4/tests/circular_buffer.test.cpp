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

#include <memory_resource>

#include <libhal/circular_buffer.hpp>
#include <libhal/units.hpp>

#include "helpers.hpp"

#include <boost/ut.hpp>

namespace hal::v5 {
namespace {
// Default test allocator
std::pmr::monotonic_buffer_resource test_buffer{ 4096 };
std::pmr::polymorphic_allocator<byte> test_allocator{ &test_buffer };
}  // namespace

// Circular buffer test suite
boost::ut::suite<"circular_buffer_test"> circular_buffer_test = []() {
  using namespace boost::ut;

  "construction"_test = [&] {
    // Test default constructor with capacity
    auto buffer = make_circular_buffer<test_class>(test_allocator, 5);

    expect(that % 5 == buffer.capacity());
    expect(that % 0 == buffer.write_index());
    expect(that % 5 == test_class::s_instance_count)
      << "Should have created five instances";

    // Test construction with initial value
    auto buffer2 =
      make_circular_buffer<test_class>(test_allocator, 3, test_class(42));

    expect(that % 3 == buffer2.capacity());
    expect(that % 42 == buffer2[0].value());
    expect(that % 42 == buffer2[1].value());
    expect(that % 42 == buffer2[2].value());

    // Test initializer list construction
    auto buffer3 = circular_buffer<int>(test_allocator, { 1, 2, 3, 4 });

    expect(that % 4 == buffer3.capacity());
    expect(that % 1 == buffer3[0]);
    expect(that % 2 == buffer3[1]);
    expect(that % 3 == buffer3[2]);
    expect(that % 4 == buffer3[3]);
    expect(that % 4 == buffer3.write_index());

    // Test minimum capacity of 1
    auto buffer4 = make_circular_buffer<test_class>(test_allocator, 0);
    expect(that % 1 == buffer4.capacity())
      << "Should enforce minimum capacity of 1";
  };

  "push"_test = [&] {
    auto buffer = make_circular_buffer<test_class>(test_allocator, 3);

    // Initial state
    expect(that % 0 == buffer.write_index());

    // Push first element
    buffer.push(test_class(10));
    expect(that % 1 == buffer.write_index());
    expect(that % 10 == buffer[0].value());

    // Push second element
    buffer.push(test_class(20));
    expect(that % 2 == buffer.write_index());
    expect(that % 10 == buffer[0].value());
    expect(that % 20 == buffer[1].value());

    // Push third element
    buffer.push(test_class(30));
    expect(that % 0 == buffer.write_index());
    expect(that % 10 == buffer[0].value());
    expect(that % 20 == buffer[1].value());
    expect(that % 30 == buffer[2].value());

    // Push fourth element (should overwrite first)
    buffer.push(test_class(40));
    expect(that % 1 == buffer.write_index());
    expect(that % 40 == buffer[0].value());
    expect(that % 20 == buffer[1].value());
    expect(that % 30 == buffer[2].value());

    // Push fifth element (should overwrite second)
    buffer.push(test_class(50));
    expect(that % 2 == buffer.write_index());
    expect(that % 40 == buffer[0].value());
    expect(that % 50 == buffer[1].value());
    expect(that % 30 == buffer[2].value());
  };

  "push_move"_test = [&] {
    // Test that the move version of push works correctly
    auto buffer = make_circular_buffer<std::string>(test_allocator, 2);

    std::string s1 = "Hello";
    std::string s2 = "World";

    buffer.push(std::move(s1));
    buffer.push(std::move(s2));

    expect(that % std::string_view("Hello") == buffer[0]);
    expect(that % std::string_view("World") == buffer[1]);
  };

  "emplace"_test = [&] {
    auto buffer = make_circular_buffer<test_class>(test_allocator, 2);

    // Initial state
    expect(that % 0 == buffer.write_index());

    // Emplace first element
    auto& ref1 = buffer.emplace(10);
    expect(that % 1 == buffer.write_index());
    expect(that % 10 == ref1.value());
    expect(that % 10 == buffer[0].value());

    // Emplace second element
    auto& ref2 = buffer.emplace(20);
    expect(that % 0 == buffer.write_index());
    expect(that % 20 == ref2.value());
    expect(that % 10 == buffer[0].value());
    expect(that % 20 == buffer[1].value());

    // Emplace third element (should overwrite first)
    auto& ref3 = buffer.emplace(30);
    expect(that % 1 == buffer.write_index());
    expect(that % 30 == ref3.value());
    expect(that % 30 == buffer[0].value());
    expect(that % 20 == buffer[1].value());
  };

  "indexing"_test = [&] {
    auto buffer = circular_buffer<int>(test_allocator, { 1, 2, 3, 4, 5 });

    // Regular indexing
    expect(that % 1 == buffer[0]);
    expect(that % 2 == buffer[1]);
    expect(that % 3 == buffer[2]);
    expect(that % 4 == buffer[3]);
    expect(that % 5 == buffer[4]);

    // Wrap-around indexing with modulo
    expect(that % 1 == buffer[5]);   // 5 % 5 = 0
    expect(that % 2 == buffer[6]);   // 6 % 5 = 1
    expect(that % 3 == buffer[7]);   // 7 % 5 = 2
    expect(that % 4 == buffer[8]);   // 8 % 5 = 3
    expect(that % 5 == buffer[9]);   // 9 % 5 = 4
    expect(that % 1 == buffer[10]);  // 10 % 5 = 0

    // Large indices
    // Any multiple of 5 gives index 0
    expect(that % 1 == buffer[10000UL * 5UL]);
    expect(that % 2 == buffer[10000UL * 5UL + 1UL]);

    // Const access
    circular_buffer<int> const const_buffer(test_allocator, { 10, 20, 30 });
    expect(that % 10 == const_buffer[0]);
    expect(that % 20 == const_buffer[1]);
    expect(that % 30 == const_buffer[2]);
    expect(that % 10 == const_buffer[3]);  // 3 % 3 = 0
  };

  "data_access"_test = [&] {
    auto buffer = circular_buffer<int>(test_allocator, { 1, 2, 3, 4 });

    // Get raw pointer to data
    int* data_ptr = buffer.data();

    // Verify data is correct
    expect(that % 1 == data_ptr[0]);
    expect(that % 2 == data_ptr[1]);
    expect(that % 3 == data_ptr[2]);
    expect(that % 4 == data_ptr[3]);

    // Modify through raw pointer
    data_ptr[2] = 30;
    expect(that % 30 == buffer[2]);

    // Const access
    circular_buffer<int> const const_buffer(test_allocator, { 10, 20, 30 });
    int const* const_data_ptr = const_buffer.data();
    expect(that % 10 == const_data_ptr[0]);
    expect(that % 20 == const_data_ptr[1]);
    expect(that % 30 == const_data_ptr[2]);
  };

  "capacity_and_size_bytes"_test = [] {
    auto buffer1 = make_circular_buffer<int>(test_allocator, 10);
    expect(that % 10 == buffer1.capacity());
    expect(that % (10 * sizeof(int)) == buffer1.size_bytes());

    auto buffer2 = make_circular_buffer<double>(test_allocator, 5);
    expect(that % 5 == buffer2.capacity());
    expect(that % (5 * sizeof(double)) == buffer2.size_bytes());

    // Minimum size of 1
    auto buffer3 = make_circular_buffer<int>(test_allocator, 0);
    expect(that % 1 == buffer3.capacity());
    expect(that % sizeof(int) == buffer3.size_bytes());
  };

  "write_index"_test = [&] {
    auto buffer = make_circular_buffer<int>(test_allocator, 3);

    // Initial state
    expect(that % 0 == buffer.write_index());

    // After one push
    buffer.push(10);
    expect(that % 1 == buffer.write_index());

    // After second push
    buffer.push(20);
    expect(that % 2 == buffer.write_index());

    // After third push (wraps around)
    buffer.push(30);
    expect(that % 0 == buffer.write_index());

    // After fourth push
    buffer.push(40);
    expect(that % 1 == buffer.write_index());
  };

  "destruction"_test = [&] {
    expect(that % 0 == test_class::s_instance_count)
      << "Should start with no instances";

    {
      auto buffer = make_circular_buffer<test_class>(test_allocator, 5);
      expect(that % 5 == test_class::s_instance_count)
        << "Should have created five instances";

      // Push some elements
      buffer.push(test_class(10));
      buffer.push(test_class(20));

      expect(that % 5 == test_class::s_instance_count)
        << "Pushing shouldn't change instance count, just replace elements";
    }

    expect(that % 0 == test_class::s_instance_count)
      << "All instances should be destroyed when buffer goes out of scope";
  };
};
}  // namespace hal::v5

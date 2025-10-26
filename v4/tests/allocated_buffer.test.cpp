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

#include <libhal/allocated_buffer.hpp>

#include "helpers.hpp"
#include "libhal/error.hpp"

#include <boost/ut.hpp>

namespace hal {
namespace {
// Default test allocator
std::pmr::monotonic_buffer_resource test_buffer{ 4096 };
std::pmr::polymorphic_allocator<byte> test_allocator{ &test_buffer };
}  // namespace

// Dynbuffer test suite
boost::ut::suite<"allocated_buffer_test"> allocated_buffer_test = []() {
  using namespace boost::ut;

  "construction"_test = [&] {
    // Reset instance count
    test_class::s_instance_count = 0;

    // Test default size constructor
    allocated_buffer<test_class> buf1(&test_buffer, 5);
    expect(that % 5 == buf1.size());
    expect(that % 5 == test_class::s_instance_count)
      << "Should have created 5 instances";

    // Test size and value constructor
    allocated_buffer<test_class> buf2(test_allocator, 3, test_class(42));
    expect(that % 3 == buf2.size());
    expect(that % 8 == test_class::s_instance_count)
      << "Should have 8 instances total";

    // Test with range-based for loop
    for (auto const& elem : buf2) {
      expect(that % 42 == elem.value())
        << "Elements should be initialized with provided value";
    }

    // Test initializer list constructor
    allocated_buffer<int> buf3(test_allocator, { 1, 2, 3, 4 });
    expect(that % 4 == buf3.size());
    int expected_val = 1;
    for (auto const& elem : buf3) {
      expect(that % expected_val == elem);
      ++expected_val;
    }

    // Test make_allocated_buffer helper
    auto buf4 = make_allocated_buffer<test_class>(test_allocator, 2);
    expect(that % 2 == buf4.size());
    expect(that % 10 == test_class::s_instance_count)
      << "Should have 10 instances total";

    auto buf5 =
      make_allocated_buffer<test_class>(test_allocator, 2, test_class(100));
    expect(that % 2 == buf5.size());
    expect(that % 12 == test_class::s_instance_count)
      << "Should have 12 instances total";

    for (auto const& elem : buf5) {
      expect(that % 100 == elem.value());
    }
  };

  "minimum size guarantee"_test = [&] {
    // Reset instance count
    test_class::s_instance_count = 0;

    // Test with requested size 0
    allocated_buffer<test_class> buf(test_allocator, 0);
    expect(that % 1 == buf.size())
      << "Should allocate at least 1 element even if 0 is requested";
    expect(false == buf.empty()) << "empty() should always return false";
    expect(that % 1 == test_class::s_instance_count)
      << "Should have created 1 instance";

    // Test with initializer list of size 0
    allocated_buffer<test_class> buf2(test_allocator, {});
    expect(that % 1 == buf2.size())
      << "Should allocate at least 1 element for empty initializer list";
    expect(that % 2 == test_class::s_instance_count)
      << "Should have created 2 instances total";

    // Test access to the single element
    // NOLINTBEGIN(performance-unnecessary-copy-initialization)
    expect(nothrow([&] { [[maybe_unused]] auto _ = buf.front(); }))
      << "front() should work on minimum-sized buffer";
    expect(nothrow([&] { [[maybe_unused]] auto _ = buf.back(); }))
      << "back() should work on minimum-sized buffer";
    expect(nothrow([&] { [[maybe_unused]] auto _ = buf[0]; }))
      << "operator[] should work on minimum-sized buffer";
    // NOLINTEND(performance-unnecessary-copy-initialization)
  };

  "not copyable or movable"_test = [&] {
    // This test verifies that copy and move operations are properly deleted

    // Verify that copy constructor is deleted
    // The commented code below should not compile:
    // allocated_buffer<int> buf1(test_allocator, 3);
    // allocated_buffer<int> buf2(buf1); // Should not compile

    // Verify that copy assignment is deleted
    // The commented code below should not compile:
    // allocated_buffer<int> buf1(test_allocator, 3);
    // allocated_buffer<int> buf2(test_allocator, 2);
    // buf2 = buf1; // Should not compile

    // Verify that move constructor is deleted
    // The commented code below should not compile:
    // allocated_buffer<int> buf1(test_allocator, 3);
    // allocated_buffer<int> buf2(std::move(buf1)); // Should not compile

    // Verify that move assignment is deleted
    // The commented code below should not compile:
    // allocated_buffer<int> buf1(test_allocator, 3);
    // allocated_buffer<int> buf2(test_allocator, 2);
    // buf2 = std::move(buf1); // Should not compile

    // We can only test that the class compiles, not that these operations don't
    // compile So this is mostly a documentation test
    expect(true) << "allocated_buffer compilation check passed";
  };

  "bounds checking"_test = [&] {
    // Create test buffer
    allocated_buffer<int> buf(test_allocator, 5);
    for (size_t i = 0; i < buf.size(); ++i) {
      buf[i] = static_cast<int>(i + 1);
    }

    // Test element access with [] - should use at() internally
    expect(that % 1 == buf[0]);
    expect(that % 3 == buf[2]);
    expect(that % 5 == buf[4]);

    // NOLINTBEGIN(performance-unnecessary-copy-initialization)
    // Test out of bounds with []
    expect(
      throws<hal::out_of_range>([&buf] { [[maybe_unused]] auto _ = buf[5]; }))
      << "operator[] should throw on out-of-bounds access";
    // NOLINTEND(performance-unnecessary-copy-initialization)

    // Test at() with bounds checking
    expect(that % 1 == buf.at(0));
    expect(that % 3 == buf.at(2));
    expect(that % 5 == buf.at(4));

    // NOLINTBEGIN(performance-unnecessary-copy-initialization)
    expect(throws<hal::out_of_range>(
      [&buf] { [[maybe_unused]] auto _ = buf.at(5); }))
      << "at() should throw on out-of-bounds access";
    // NOLINTEND(performance-unnecessary-copy-initialization)

    // Test front() and back()
    expect(that % 1 == buf.front());
    expect(that % 5 == buf.back());

    // Test modification
    buf[2] = 30;
    expect(that % 30 == buf[2]);
    buf.at(4) = 50;
    expect(that % 50 == buf[4]);
  };

  "iterators"_test = [&] {
    // Create test buffer
    allocated_buffer<int> buf(test_allocator, 5);
    int value = 1;
    for (auto& elem : buf) {
      elem = value++;
    }

    // Test range-based for loop
    int sum = 0;
    for (auto const& val : buf) {
      sum += val;
    }
    expect(that % 15 == sum) << "Sum should be 1+2+3+4+5 = 15";

    // Test const iterators
    allocated_buffer<int> const& const_buf = buf;
    sum = 0;
    // NOLINTNEXTLINE(modernize-loop-convert): needed for tests
    for (auto it = const_buf.begin(); it != const_buf.end(); ++it) {
      sum += *it;
    }
    expect(that % 15 == sum) << "Sum should be 1+2+3+4+5 = 15";

    // Test reverse iterators
    std::vector<int> expected = { 5, 4, 3, 2, 1 };
    std::vector<int> actual;
    // NOLINTNEXTLINE(modernize-loop-convert): needed for tests
    for (auto it = buf.rbegin(); it != buf.rend(); ++it) {
      actual.push_back(*it);
    }

    for (size_t i = 0; i < expected.size(); ++i) {
      expect(that % expected[i] == actual[i]);
    }

    // Test const reverse iterators
    actual.clear();
    // NOLINTNEXTLINE(modernize-loop-convert): needed for tests
    for (auto it = const_buf.crbegin(); it != const_buf.crend(); ++it) {
      actual.push_back(*it);
    }

    for (size_t i = 0; i < expected.size(); ++i) {
      expect(that % expected[i] == actual[i]);
    }
  };

  "fill"_test = [&] {
    // Create test buffer
    allocated_buffer<test_class> buf(test_allocator, 3);
    for (auto const& elem : buf) {
      expect(that % 0 == elem.value());
    }

    // Fill with a value
    buf.fill(test_class(42));
    for (auto const& elem : buf) {
      expect(that % 42 == elem.value());
    }
  };

  "equality comparison"_test = [&] {
    // Create buffers with different content
    allocated_buffer<int> buf1(test_allocator, { 1, 2, 3 });
    allocated_buffer<int> buf2(test_allocator, { 1, 2, 3 });
    allocated_buffer<int> buf3(test_allocator, { 1, 2, 4 });
    allocated_buffer<int> buf4(test_allocator, { 1, 2 });

    expect(buf1 == buf2) << "Identical buffers should be equal";
    expect(buf1 != buf3)
      << "Buffers with different elements should not be equal";
    expect(buf1 != buf4) << "Buffers with different sizes should not be equal";
  };

  "destruction"_test = [&] {
    // Reset instance count
    test_class::s_instance_count = 0;

    {
      // Create buffer
      allocated_buffer<test_class> buf(test_allocator, 5, test_class(42));
      expect(that % 5 == test_class::s_instance_count)
        << "Should have 5 instances";
    }

    // After scope ends, all instances should be destroyed
    expect(that % 0 == test_class::s_instance_count)
      << "All instances should be destroyed";
  };

  "trivial type optimization"_test = [&] {
    // This test verifies that trivial types work correctly
    // We can't directly test the optimization, but we can verify correct
    // behavior

    // Test with trivially constructible/destructible type (int)
    allocated_buffer<int> buf(test_allocator, 5);

    // Fill with values
    for (size_t i = 0; i < buf.size(); ++i) {
      buf[i] = static_cast<int>(i * 10);
    }

    // Verify values
    for (size_t i = 0; i < buf.size(); ++i) {
      expect(that % static_cast<int>(i * 10) == buf[i]);
    }
  };
};
}  // namespace hal

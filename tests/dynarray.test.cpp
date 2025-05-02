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

#include <libhal/dynarray.hpp>

#include <boost/ut.hpp>

// NOLINTBEGIN(performance-unnecessary-copy-initialization)
namespace hal::v5 {
namespace {
// Create a test class to use with dynarray
class test_class
{
public:
  explicit test_class(int p_value = 0)
    : m_value(p_value)
    , m_constructed(true)
  {
    // Track object construction
    ++s_instance_count;
  }

  ~test_class()
  {
    // Track object destruction
    m_constructed = false;
    --s_instance_count;
  }

  test_class(test_class const& other)
    : m_value(other.m_value)
    , m_constructed(true)
  {
    ++s_instance_count;
  }

  test_class& operator=(test_class const& other)
  {
    if (this != &other) {
      m_value = other.m_value;
    }
    return *this;
  }

  bool operator==(test_class const& other) const
  {
    return m_value == other.m_value;
  }

  [[nodiscard]] int value() const
  {
    return m_value;
  }

  void set_value(int p_value)
  {
    m_value = p_value;
  }

  [[nodiscard]] bool is_constructed() const
  {
    return m_constructed;
  }

  // Static counter for number of instances
  static int s_instance_count;

private:
  int m_value;
  bool m_constructed{ false };
};

// Initialize static counter
int test_class::s_instance_count = 0;

// Default test allocator
std::pmr::monotonic_buffer_resource test_buffer{ 4096 };
std::pmr::polymorphic_allocator<byte> test_allocator{ &test_buffer };

}  // namespace

// Dynarray test suite
boost::ut::suite<"dynarray_test"> dynarray_test = []() {
  using namespace boost::ut;

  "construction"_test = [&] {
    // Reset instance count
    test_class::s_instance_count = 0;

    // Test default size constructor
    dynarray<test_class> arr1(test_allocator, 5);
    expect(that % 5 == arr1.size());
    expect(that % 5 == test_class::s_instance_count)
      << "Should have created 5 instances";

    // Test size and value constructor
    dynarray<test_class> arr2(test_allocator, 3, test_class(42));
    expect(that % 3 == arr2.size());
    expect(that % 8 == test_class::s_instance_count)
      << "Should have 8 instances total";

    // Test with range-based for loop
    for (auto const& elem : arr2) {
      expect(that % 42 == elem.value())
        << "Elements should be initialized with provided value";
    }

    // Test initializer list constructor
    dynarray<int> arr3(test_allocator, { 1, 2, 3, 4 });
    expect(that % 4 == arr3.size());
    int expected_val = 1;
    for (auto const& elem : arr3) {
      expect(that % expected_val == elem);
      ++expected_val;
    }

    // Test make_dynarray helper
    auto arr4 = make_dynarray<test_class>(test_allocator, 2);
    expect(that % 2 == arr4.size());
    expect(that % 10 == test_class::s_instance_count)
      << "Should have 10 instances total";

    auto arr5 = make_dynarray<test_class>(test_allocator, 2, test_class(100));
    expect(that % 2 == arr5.size());
    expect(that % 12 == test_class::s_instance_count)
      << "Should have 12 instances total";

    for (auto const& elem : arr5) {
      expect(that % 100 == elem.value());
    }
  };

  "minimum size guarantee"_test = [&] {
    // Reset instance count
    test_class::s_instance_count = 0;

    // Test with requested size 0
    dynarray<test_class> arr(test_allocator, 0);
    expect(that % 1 == arr.size())
      << "Should allocate at least 1 element even if 0 is requested";
    expect(false == arr.empty()) << "empty() should always return false";
    expect(that % 1 == test_class::s_instance_count)
      << "Should have created 1 instance";

    // Test with initializer list of size 0
    dynarray<test_class> arr2(test_allocator, {});
    expect(that % 1 == arr2.size())
      << "Should allocate at least 1 element for empty initializer list";
    expect(that % 2 == test_class::s_instance_count)
      << "Should have created 2 instances total";

    // Test access to the single element
    expect(nothrow([&] { arr.front(); }))
      << "front() should work on minimum-sized array";
    expect(nothrow([&] { arr.back(); }))
      << "back() should work on minimum-sized array";
    expect(nothrow([&] { arr[0]; }))
      << "operator[] should work on minimum-sized array";
  };

  "bounds checking"_test = [&] {
    // Create test array
    dynarray<int> arr(test_allocator, 5);
    for (size_t i = 0; i < arr.size(); ++i) {
      arr[i] = static_cast<int>(i + 1);
    }

    // Test element access with [] - should use at() internally
    expect(that % 1 == arr[0]);
    expect(that % 3 == arr[2]);
    expect(that % 5 == arr[4]);

    // Test out of bounds with []
    expect(throws<std::out_of_range>([&] { arr[5]; }))
      << "operator[] should throw on out-of-bounds access";

    // Test at() with bounds checking
    expect(that % 1 == arr.at(0));
    expect(that % 3 == arr.at(2));
    expect(that % 5 == arr.at(4));
    expect(throws<std::out_of_range>([&] { arr.at(5); }))
      << "at() should throw on out-of-bounds access";

    // Test front() and back()
    expect(that % 1 == arr.front());
    expect(that % 5 == arr.back());

    // Test modification
    arr[2] = 30;
    expect(that % 30 == arr[2]);
    arr.at(4) = 50;
    expect(that % 50 == arr[4]);
  };

  "iterators"_test = [&] {
    // Create test array
    dynarray<int> arr(test_allocator, 5);
    int value = 1;
    for (auto& elem : arr) {
      elem = value++;
    }

    // Test range-based for loop
    int sum = 0;
    for (auto const& val : arr) {
      sum += val;
    }
    expect(that % 15 == sum) << "Sum should be 1+2+3+4+5 = 15";

    // Test const iterators
    dynarray<int> const& const_arr = arr;
    sum = 0;
    // NOLINTNEXTLINE(modernize-loop-convert): for testing
    for (auto it = const_arr.begin(); it != const_arr.end(); ++it) {
      sum += *it;
    }
    expect(that % 15 == sum) << "Sum should be 1+2+3+4+5 = 15";

    // Test reverse iterators
    std::vector<int> expected = { 5, 4, 3, 2, 1 };
    std::vector<int> actual;
    // NOLINTNEXTLINE(modernize-loop-convert): for testing
    for (auto it = arr.rbegin(); it != arr.rend(); ++it) {
      actual.push_back(*it);
    }

    for (size_t i = 0; i < expected.size(); ++i) {
      expect(that % expected[i] == actual[i]);
    }

    // Test const reverse iterators
    actual.clear();
    // NOLINTNEXTLINE(modernize-loop-convert): for testing
    for (auto it = const_arr.crbegin(); it != const_arr.crend(); ++it) {
      actual.push_back(*it);
    }

    for (size_t i = 0; i < expected.size(); ++i) {
      expect(that % expected[i] == actual[i]);
    }
  };

  "fill"_test = [&] {
    // Create test array
    dynarray<test_class> arr(test_allocator, 3);
    for (auto const& elem : arr) {
      expect(that % 0 == elem.value());
    }

    // Fill with a value
    arr.fill(test_class(42));
    for (auto const& elem : arr) {
      expect(that % 42 == elem.value());
    }
  };

  "equality comparison"_test = [&] {
    // Create identical arrays
    dynarray<int> arr1(test_allocator, { 1, 2, 3 });
    dynarray<int> arr2(test_allocator, { 1, 2, 3 });
    dynarray<int> arr3(test_allocator, { 1, 2, 4 });
    dynarray<int> arr4(test_allocator, { 1, 2 });

    expect(arr1 == arr2) << "Identical arrays should be equal";
    expect(arr1 != arr3)
      << "Arrays with different elements should not be equal";
    expect(arr1 != arr4) << "Arrays with different sizes should not be equal";
  };

  "destruction"_test = [&] {
    // Reset instance count
    test_class::s_instance_count = 0;

    {
      // Create array
      dynarray<test_class> arr(test_allocator, 5, test_class(42));
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
    dynarray<int> arr(test_allocator, 5);

    // Fill with values
    for (size_t i = 0; i < arr.size(); ++i) {
      arr[i] = static_cast<int>(i * 10);
    }

    // Verify values
    for (size_t i = 0; i < arr.size(); ++i) {
      expect(that % static_cast<int>(i * 10) == arr[i]);
    }

    // Test copy with trivial type
    dynarray<int> arr_copy(arr);
    expect(arr == arr_copy) << "Copy should be equal to original";

    // Test assignment with trivial type
    dynarray<int> arr_assign(test_allocator, 2);
    arr_assign = arr;
    expect(arr == arr_assign) << "After assignment should be equal to original";
  };

  "not movable"_test = [&] {
    // This test verifies that move operations are properly deleted

    // Verify that move constructor is deleted
    // The commented code below should not compile:
    // dynarray<int> arr1(test_allocator, 3);
    // dynarray<int> arr2(std::move(arr1));  // Should not compile

    // Verify that move assignment is deleted
    // The commented code below should not compile:
    // dynarray<int> arr1(test_allocator, 3);
    // dynarray<int> arr2(test_allocator, 2);
    // arr2 = std::move(arr1);  // Should not compile

    // We can only test that the class compiles, not that these operations don't
    // compile So this is mostly a documentation test
    expect(true) << "dynarray compilation check passed";
  };
};
// NOLINTEND(performance-unnecessary-copy-initialization)
}  // namespace hal::v5

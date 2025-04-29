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

#include <libhal/experimental/ptr.hpp>

#include <boost/ut.hpp>

// NOLINTBEGIN(performance-unnecessary-copy-initialization)
namespace hal {
namespace {
// Create a test class to use with smart pointers
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

  test_class(test_class const&) = delete;
  test_class& operator=(test_class const&) = delete;
  test_class(test_class&&) = delete;
  test_class& operator=(test_class&&) = delete;

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

// Base class for testing polymorphism
class base_class
{
public:
  virtual ~base_class() = default;
  [[nodiscard]] virtual int value() const = 0;
};

// Derived class for testing polymorphism
class derived_class : public base_class
{
public:
  explicit derived_class(int p_value = 0)
    : m_value(p_value)
  {
  }

  [[nodiscard]] int value() const override
  {
    return m_value;
  }

private:
  int m_value;
};

// Default test allocator
std::pmr::monotonic_buffer_resource test_buffer{ 4096 };
std::pmr::polymorphic_allocator<byte> test_allocator{ &test_buffer };

}  // namespace

// Strong pointer test suite
boost::ut::suite<"strong_ptr_test"> strong_ptr_test = []() {
  using namespace boost::ut;

  "construction"_test = [&] {
    // Test make_strong_ptr
    auto ptr = make_strong_ptr<test_class>(test_allocator, 42);

    expect(that % 42 == ptr->value());
    expect(that % 1 == test_class::s_instance_count)
      << "Should have created one instance";
    expect(that % 1 == ptr.use_count())
      << "Should have exactly one reference\n";

    // Test copy constructor
    auto ptr2 = ptr;
    expect(that % 2 == ptr.use_count())
      << "Should have two references after copy\n";
    expect(that % 2 == ptr2.use_count())
      << "Copy should share reference count\n";

    // Test copy constructor and reference counting
    {
      auto ptr3 = ptr;
      expect(that % 3 == ptr.use_count()) << "Should have three references\n";
    }
    expect(that % 2 == ptr.use_count())
      << "Should have two references after ptr3 goes out of scope\n";

    auto ptr4 = ptr2;
    expect(that % 3 == ptr.use_count());
    expect(that % 3 == ptr4.use_count());
    expect(nothrow([&] { auto _ = ptr4->value(); }));
  };

  "operator overloads"_test = [&] {
    auto ptr = make_strong_ptr<test_class>(test_allocator, 42);

    // Test dereference operator
    expect(that % 42 == (*ptr).value());

    // Test arrow operator
    expect(that % 42 == ptr->value());

    // Test modifying the underlying object
    ptr->set_value(100);
    expect(that % 100 == ptr->value());
  };

  "polymorphism"_test = [&] {
    // Test polymorphic behavior
    auto derived = make_strong_ptr<derived_class>(test_allocator, 42);
    strong_ptr<base_class> base = derived;

    expect(that % 42 == base->value());
    expect(that % 2 == derived.use_count())
      << "Base and derived should share ownership\n";

    // Modify through derived
    auto derived2 = make_strong_ptr<derived_class>(test_allocator, 100);
    base = derived2;

    expect(that % 100 == base->value());
    expect(that % 1 == derived.use_count())
      << "Original derived should now have only one reference\n";
    expect(that % 2 == derived2.use_count())
      << "New derived should share with base\n";
  };

  "aliasing"_test = [&] {
    // Create a class with internal structure
    struct outer_class
    {
      int member_offset{};
      test_class inner;
      std::array<test_class, 2> array_inner;
      explicit outer_class(int p_value)
        : inner(p_value)
        , array_inner{ test_class{ p_value }, test_class{ p_value } }
      {
      }
    };

    auto outer = make_strong_ptr<outer_class>(test_allocator, 42);
    // Create an alias to the inner object
    strong_ptr<test_class> inner(outer, &outer_class::inner);
    // Create an alias to the array_inner object's index 1
    strong_ptr<test_class> array_inner(outer, &outer_class::array_inner, 1);

    expect(that % 42 == inner->value());
    expect(that % 42 == array_inner->value());

    expect(that % 3 == outer.use_count())
      << "Outer and inner alias should share ownership\n";
    expect(that % 3 == outer.use_count())
      << "Outer and inner alias should share ownership\n";

    // Modify through the alias
    inner->set_value(100);
    // Modify through the alias
    array_inner->set_value(120);

    expect(that % 100 == outer->inner.value());
    expect(that % 120 == outer->array_inner[1].value());
  };

  "alias outliving original"_test = [&] {
    // Create a class with internal structure
    struct outer_class
    {
      int member_offset{};
      test_class inner;
      std::array<test_class, 2> array_inner;
      explicit outer_class(int p_value)
        : inner(p_value)
        , array_inner{ test_class{ p_value }, test_class{ p_value } }
      {
      }
    };

    auto outlived = [] -> auto {
      auto outer = make_strong_ptr<outer_class>(test_allocator, 42);
      // Create an alias to the inner object
      strong_ptr<test_class> inner(outer, &outer_class::inner);
      // Create an alias to the array_inner object's index 1
      strong_ptr<test_class> array_inner(outer, &outer_class::array_inner, 1);

      expect(that % 42 == inner->value());
      expect(that % 42 == array_inner->value());

      expect(that % 3 == outer.use_count())
        << "Outer and inner alias should share ownership\n";
      expect(that % 3 == outer.use_count())
        << "Outer and inner alias should share ownership\n";

      // Modify through the alias
      inner->set_value(100);
      // Modify through the alias
      array_inner->set_value(120);

      expect(that % 100 == outer->inner.value());
      expect(that % 120 == outer->array_inner[1].value());

      // NOLINTNEXTLINE(performance-move-const-arg)
      auto move_becomes_copy = std::move(inner);

      // Test that explicit move becomes a copy
      expect(that % 4 == move_becomes_copy.use_count())
        << "Outer and inner alias should share ownership\n";

      return array_inner;
    }();

    expect(that % 120 == outlived->value());

    expect(that % 1 == outlived.use_count())
      << "outlived alias should have sole ownership\n";
  };

  "equality"_test = [&] {
    auto ptr1 = make_strong_ptr<test_class>(test_allocator, 42);
    auto ptr2 = ptr1;
    auto ptr3 = make_strong_ptr<test_class>(test_allocator, 43);

    expect(ptr1 == ptr2) << "Copies should be equal\n";
    expect(ptr1 != ptr3) << "Different objects should not be equal\n";
  };

  "destruction"_test = [&] {
    expect(that % 0 == test_class::s_instance_count)
      << "Should start with no instances\n";

    {
      auto ptr = make_strong_ptr<test_class>(test_allocator, 42);
      expect(that % 1 == test_class::s_instance_count)
        << "Should have one instance\n";
    }

    expect(that % 0 == test_class::s_instance_count)
      << "Instance should be destroyed\n";
  };
};

// Weak pointer test suite
boost::ut::suite<"weak_ptr_test"> weak_ptr_test = []() {
  using namespace boost::ut;

  "construction"_test = [&] {
    // Test creating a weak_ptr from a strong_ptr
    auto strong = make_strong_ptr<test_class>(test_allocator, 42);
    weak_ptr<test_class> weak = strong;

    expect(not weak.expired()) << "Weak pointer should not be expired\n";
    expect(that % 1 == strong.use_count())
      << "Weak reference shouldn't affect strong count\n";

    // Test default constructor
    weak_ptr<test_class> empty;
    expect(empty.expired())
      << "Default constructed weak_ptr should be expired\n";
    expect(that % 0 == empty.use_count());

    // Test copy constructor
    weak_ptr<test_class> weak2 = weak;
    expect(not weak2.expired()) << "Copied weak_ptr should not be expired\n";

    // Test from expired strong_ptr
    {
      auto temp_strong = make_strong_ptr<test_class>(test_allocator, 100);
      weak_ptr<test_class> temp_weak = temp_strong;

      expect(not temp_weak.expired()) << "Weak pointer should not be expired\n";

      // Temp_strong will go out of scope here
    }

    // Previous strong_ptr should be destroyed, but the weak_ptr should still
    // exist
    expect(that % 1 == test_class::s_instance_count)
      << "Only one test_class should exist\n";
  };

  "lock"_test = [&] {
    auto strong = make_strong_ptr<test_class>(test_allocator, 42);
    weak_ptr<test_class> weak = strong;

    // Test locking to get a strong_ptr
    {
      auto locked = weak.lock();
      expect(that % true == static_cast<bool>(locked))
        << "Lock should succeed on valid weak_ptr\n";
      expect(that % 42 == locked->value())
        << "Locked value should match original\n";
      expect(that % 2 == strong.use_count())
        << "Should now have two strong references\n";
    }

    expect(that % 1 == strong.use_count())
      << "Back to one reference after locked ptr is destroyed\n";

    // Test locking an expired weak_ptr
    weak_ptr<test_class> temp_weak;

    {
      auto temp_strong = make_strong_ptr<test_class>(test_allocator, 100);
      temp_weak = temp_strong;

      auto locked = temp_weak.lock();
      expect(that % true == bool(locked))
        << "Lock should succeed on existing weak_ptr\n";
    }

    auto locked = temp_weak.lock();
    expect(that % false == bool(locked))
      << "Lock should fail on expired weak_ptr\n";
  };

  "polymorphism"_test = [&] {
    auto derived = make_strong_ptr<derived_class>(test_allocator, 42);
    weak_ptr<base_class> base_weak = derived;

    expect(not base_weak.expired())
      << "Polymorphic weak_ptr should not be expired\n";

    auto locked = base_weak.lock();
    expect(that % true == bool(locked))
      << "Should be able to lock polymorphic weak_ptr\n";
    expect(that % 42 == locked->value())
      << "Locked value should match original\n";
  };

  "expired"_test =
    [&] {
      // Create a scope for the strong_ptr
      weak_ptr<test_class> weak;

      {
        auto strong = make_strong_ptr<test_class>(test_allocator, 42);
        weak = strong;
        expect(not weak.expired()) << "Weak pointer should not be expired\n";
      }

      expect(weak.expired())
        << "Weak pointer should be expired after strong_ptr is destroyed\n";
      expect(that % 0 == test_class::s_instance_count)
        << "Object should be destroyed\n";

      auto locked = weak.lock();
      expect(that % false == bool(locked))
        << "Locking expired weak_ptr should return null optional\n";
    };
};

// Optional pointer test suite
boost::ut::suite<"optional_ptr_test"> optional_ptr_test = []() {
  using namespace boost::ut;

  "construction"_test = [&] {
    // Test default constructor
    hal::optional_ptr<test_class> empty;
    expect(that % false == bool(empty))
      << "Default constructed optional_ptr should be empty\n";

    // Test nullptr constructor
    hal::optional_ptr<test_class> null_ptr = nullptr;
    expect(that % false == bool(null_ptr))
      << "Nullptr constructed optional_ptr should be empty\n";

    // Test from strong_ptr
    auto strong = make_strong_ptr<test_class>(test_allocator, 42);
    optional_ptr<test_class> opt = strong;

    expect(that % true == bool(opt))
      << "Optional from strong_ptr should be valid\n";
    expect(that % 2 == strong.use_count()) << "Should share ownership\n";

    // Test make_optional_ptr factory function
    hal::optional_ptr<test_class> direct_opt =
      make_strong_ptr<test_class>(test_allocator, 100);
    expect(that % true == bool(direct_opt))
      << "Factory-created optional should be valid\n";
    expect(that % 100 == direct_opt->value())
      << "Value should match construction parameter\n";

    // Test copy constructor
    optional_ptr<test_class> opt2 = opt;
    expect(that % true == bool(opt2)) << "Copy should be valid\n";
    expect(that % 3 == strong.use_count())
      << "Should now have three shared owners\n";
  };

  "access"_test = [&] {
    auto strong = make_strong_ptr<test_class>(test_allocator, 42);
    optional_ptr<test_class> opt = strong;

    expect(nothrow([&] { auto _ = opt->value(); }))
      << "Arrow operator should work on valid optional\n";
    expect(nothrow([&] { auto _ = (*opt).value(); }))
      << "Dereference operator should work on valid optional\n";

    // Test value method
    expect(that % 42 == opt->value());

    // Test modifying through optional
    opt->set_value(100);
    expect(that % 100 == strong->value())
      << "Changes through optional should affect underlying object\n";

    // Test exception on accessing null optional
    optional_ptr<test_class> empty;
    expect(throws<std::bad_optional_access>([&] { auto _ = empty->value(); }))
      << "Accessing null optional with arrow operator should throw\n";
    expect(throws<std::bad_optional_access>([&] { auto _ = (*empty).value(); }))
      << "Accessing null optional with dereference operator should throw\n";
  };

  "reset"_test = [&] {
    auto strong = make_strong_ptr<test_class>(test_allocator, 42);
    optional_ptr<test_class> opt = strong;

    expect(that % true == bool(opt)) << "Optional should be valid\n";
    expect(that % 2 == strong.use_count()) << "Should share ownership\n";

    // Reset to null
    opt = nullptr;
    expect(that % false == bool(opt))
      << "Optional should be empty after reset\n";
    expect(that % 1 == strong.use_count()) << "Should release ownership\n";

    // Re-assign
    opt = strong;
    expect(that % true == bool(opt)) << "Optional should be valid again\n";
    expect(that % 2 == strong.use_count()) << "Should share ownership again\n";
  };

  "polymorphism"_test = [&] {
    auto derived = make_strong_ptr<derived_class>(test_allocator, 42);
    optional_ptr<base_class> base_opt = derived;

    expect(that % true == bool(base_opt))
      << "Polymorphic optional should be valid\n";
    expect(that % 42 == base_opt->value())
      << "Value should be accessible through base interface\n";
    expect(that % 2 == derived.use_count()) << "Should share ownership\n";

    // Test assigning different derived
    auto derived2 = make_strong_ptr<derived_class>(test_allocator, 100);
    base_opt = derived2;

    expect(that % 100 == base_opt->value())
      << "Value should update after assignment\n";
    expect(that % 1 == derived.use_count())
      << "First derived should lose shared ownership\n";
    expect(that % 2 == derived2.use_count())
      << "Second derived should gain shared ownership\n";
  };

  "weak_ptr_lock"_test = [&] {
    weak_ptr<test_class> weak;
    {
      // Test creating an optional_ptr through weak_ptr::lock()
      auto strong = make_strong_ptr<test_class>(test_allocator, 42);
      weak = strong;

      auto locked = weak.lock();
      expect(that % true == bool(locked))
        << "Locked weak_ptr should be valid\n";
      expect(that % 42 == locked->value()) << "Value should match original\n";
      expect(that % 2 == strong.use_count())
        << "Should share ownership with original\n";
    }

    // Lock should now fail
    auto locked2 = weak.lock();
    expect(that % false == bool(locked2))
      << "Locked expired weak_ptr should be empty\n";
  };

  "equality"_test = [&] {
    auto strong1 = make_strong_ptr<test_class>(test_allocator, 42);
    auto strong2 = make_strong_ptr<test_class>(test_allocator, 43);

    optional_ptr<test_class> opt1 = strong1;
    optional_ptr<test_class> opt2 = strong1;
    optional_ptr<test_class> opt3 = strong2;
    optional_ptr<test_class> empty1;
    optional_ptr<test_class> empty2;
#if 0
    expect(opt1 == opt2) << "Optionals pointing to same object should be equal\n";
    expect(opt1 != opt3)
      << "Optionals pointing to different objects should not be equal\n";
    expect(empty1 == empty2) << "Empty optionals should be equal\n";
    expect(opt1 != empty1) << "Valid and empty optionals should not be equal\n";

    expect(empty1 == nullptr) << "Empty optional should equal nullptr\n";
    expect(nullptr == empty1) << "nullptr should equal empty optional\n";
    expect(opt1 != nullptr) << "Valid optional should not equal nullptr\n";
    expect(nullptr != opt1) << "nullptr should not equal valid optional\n";
#endif
  };
};
// NOLINTEND(performance-unnecessary-copy-initialization)
}  // namespace hal

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

// Runtime behavior of hal:lifetime. Misuse that must NOT compile, and the
// dangling references Clang's lifetime safety analysis must catch, are in
// tests/compile_checks/lifetime_cases.cpp.

#include <array>
#include <cstddef>
#include <exception>
#include <memory>
#include <memory_resource>
#include <optional>
#include <type_traits>
#include <utility>

#include <boost/ut.hpp>

import hal;

namespace {

// Thrown by test types whose constructor fails on purpose.
struct construct_error : std::exception
{};

int g_ctor = 0;
int g_dtor = 0;  // stands in for "peripheral reset"

struct i2c : hal::no_bare_new_mixin
{
  virtual int read() = 0;

protected:
  ~i2c() = default;  // no virtual dtor needed; owner ends the full type
};

struct fake_i2c final : i2c
{
  explicit fake_i2c(int p_port) noexcept
    : port(p_port)
  {
    ++g_ctor;
  }
  ~fake_i2c()
  {
    ++g_dtor;
  }
  fake_i2c(fake_i2c const&) = delete;
  fake_i2c& operator=(fake_i2c const&) = delete;

  int read() override
  {
    return port;
  }
  int port;
};

class sensor
{
public:
  explicit sensor(hal::ref<i2c> p_bus [[clang::lifetimebound]])
    : m_bus(p_bus)
  {
  }
  [[nodiscard]] int read() const
  {
    return m_bus->read();
  }

private:
  hal::ref<i2c> m_bus;
};

fake_i2c g_bus{ 7 };

hal::static_ref<fake_i2c> get_static_bus()
{
  static fake_i2c bus{ 2 };
  return hal::static_ref<fake_i2c>{ bus };
}

hal::owner<fake_i2c> acquire_i2c1()
{
  static constinit hal::reusable<fake_i2c> storage;
  return emplace(storage, 1);
}

// Typical application: owns its owners, composites borrow them. Members are
// destroyed in reverse order, so the sensor goes before the bus.
struct app
{
  hal::owner<i2c> bus = acquire_i2c1();
  sensor s{ bus };
};

struct throws_once : hal::no_bare_new_mixin
{
  static inline bool armed = true;
  throws_once()
  {
    if (std::exchange(armed, false)) {
      throw construct_error{};
    }
  }
};

// Counts allocations so tests can check that every owner frees its storage.
class counting_resource final : public std::pmr::memory_resource
{
public:
  int allocations = 0;
  int deallocations = 0;

private:
  void* do_allocate(std::size_t p_bytes, std::size_t p_align) override
  {
    ++allocations;
    return std::pmr::new_delete_resource()->allocate(p_bytes, p_align);
  }
  void do_deallocate(void* p_ptr,
                     std::size_t p_bytes,
                     std::size_t p_align) override
  {
    ++deallocations;
    std::pmr::new_delete_resource()->deallocate(p_ptr, p_bytes, p_align);
  }
  [[nodiscard]] bool do_is_equal(
    std::pmr::memory_resource const& p_other) const noexcept override
  {
    return this == &p_other;
  }
};

// The interface is the second base, so a pointer to it differs from a pointer
// to the complete object. Destroying through the wrong one would be caught by
// ASan.
struct tag_base
{
  int tag = 0x5a5a;
};
struct offset_i2c final
  : tag_base
  , i2c
{
  explicit offset_i2c(int p_port)
    : port(p_port)
  {
    ++g_ctor;
  }
  offset_i2c(offset_i2c const&) = delete;
  offset_i2c& operator=(offset_i2c const&) = delete;
  ~offset_i2c()
  {
    ++g_dtor;
  }
  int read() override
  {
    return port;
  }
  int port;
};

// A manager hands out its resource through an owner, keeping the concrete
// type (fake_i2c here) out of its public interface.
class i2c_manager
{
public:
  [[nodiscard]] hal::owner<i2c> acquire(
    std::pmr::polymorphic_allocator<> p_allocator) [[clang::lifetimebound]]
  {
    return hal::make_owner<fake_i2c>(p_allocator, m_port);
  }

private:
  int m_port = 8;
};

struct throws_on_construct : hal::no_bare_new_mixin
{
  throws_on_construct()
  {
    throw construct_error{};
  }
};

static_assert(std::is_constructible_v<hal::owner<i2c>, hal::owner<fake_i2c>&&>);

// A static reusable<T> only needs an exit-time destructor when T does.
static_assert(std::is_trivially_destructible_v<hal::reusable<int>>);
static_assert(not std::is_trivially_destructible_v<hal::reusable<fake_i2c>>);

static_assert(sizeof(hal::ref<i2c>) == sizeof(void*));
static_assert(sizeof(hal::static_ref<i2c>) == sizeof(void*));
static_assert(std::is_trivially_copyable_v<hal::ref<i2c>>);
static_assert(std::is_trivially_copyable_v<hal::static_ref<i2c>>);

void lifetime_test()
{
  using namespace boost::ut;

  "ref from static_ref factory"_test = []() {
    // Setup
    sensor s(get_static_bus());

    // Exercise + Verify
    expect(that % 2 == s.read());
  };

  "ref from static global"_test = []() {
    // Setup
    sensor s(g_bus);

    // Exercise + Verify
    expect(that % 7 == s.read());
  };

  "ref from local in the same scope"_test = []() {
    // Setup
    fake_i2c bus{ 3 };
    sensor s(bus);

    // Exercise + Verify
    expect(that % 3 == s.read());
  };

  "ref to an earlier member of a composite"_test = []() {
    // Setup
    struct board
    {
      fake_i2c bus{ 4 };
      sensor s{ bus };
    };
    board b;

    // Exercise + Verify
    expect(that % 4 == b.s.read());
  };

  "composite bound to a static can be returned"_test = []() {
    // Setup
    auto make = []() { return sensor(get_static_bus()); };

    // Exercise + Verify
    expect(that % 2 == make().read());
  };

  "unsafe_assume_static() over a startup arena"_test = []() {
    // Setup
    alignas(std::max_align_t) static std::array<std::byte, 256> arena_buffer{};
    static std::pmr::monotonic_buffer_resource resource(arena_buffer.data(),
                                                        arena_buffer.size());
    std::pmr::polymorphic_allocator<> allocator(&resource);
    auto* bus = allocator.new_object<fake_i2c>(5);  // never freed

    // Exercise
    sensor s(hal::static_ref<fake_i2c>::unsafe_assume_static(*bus));

    // Verify
    expect(that % 5 == s.read());
  };

  "no_bare_new_mixin still allows placement and allocator paths"_test = []() {
    // Setup
    alignas(fake_i2c) static std::array<std::byte, sizeof(fake_i2c)> buffer{};

    // Exercise
    auto* placed = new (buffer.data()) fake_i2c{ 1 };
    std::destroy_at(placed);
    auto* constructed =
      std::construct_at(reinterpret_cast<fake_i2c*>(buffer.data()), 2);
    std::destroy_at(constructed);
    std::optional<fake_i2c> optional_bus;
    optional_bus.emplace(3);

    // Verify
    expect(that % 3 == optional_bus->read());
  };

  "reusable is reused after each owner ends"_test = []() {
    // Setup
    int const ctor_before = g_ctor;
    int const dtor_before = g_dtor;

    // Exercise
    for (int i = 0; i < 3; i++) {
      app a;
      expect(that % 1 == a.s.read());
    }  // ~app: ~sensor, then ~owner -> ~fake_i2c (peripheral reset)

    // Verify
    expect(that % 3 == g_ctor - ctor_before);
    expect(that % 3 == g_dtor - dtor_before);
  };

  "reusable throws while held, empties at end of scope"_test = []() {
    // Setup
    bool threw = false;

    // Exercise
    {
      auto first = acquire_i2c1();
      try {
        auto second = acquire_i2c1();
      } catch (hal::reusable_busy const&) {
        threw = true;
      }
    }  // first ends here, emptying the reusable
    auto again = acquire_i2c1();

    // Verify
    expect(threw);
    expect(that % 1 == again->read());
  };

  "emplace() takes runtime arguments"_test = []() {
    // Setup
    static constinit hal::reusable<fake_i2c> storage;
    int volatile runtime_port = 9;  // not a constant expression

    // Exercise
    auto bus = emplace(storage, static_cast<int>(runtime_port));

    // Verify
    expect(that % 9 == bus->read());
    expect(storage.has_value());
  };

  "owner from reusable moves and upcasts"_test = []() {
    // Setup
    int const dtor_before = g_dtor;

    // Exercise
    {
      auto derived = acquire_i2c1();
      hal::owner<i2c> base = std::move(derived);  // upcast, ownership moves
      expect(not derived);  // NOLINT(bugprone-use-after-move): checking it
      expect(static_cast<bool>(base));
      expect(that % dtor_before == g_dtor);
    }  // only base destroys, and it destroys the full fake_i2c

    // Verify
    expect(that % 1 == g_dtor - dtor_before);
  };

  "failed construction leaves the reusable empty"_test = []() {
    // Setup
    static constinit hal::reusable<throws_once> storage;
    bool threw = false;

    // Exercise
    try {
      auto held = emplace(storage);
    } catch (construct_error const&) {
      threw = true;
    }
    bool const has_value_after_throw = storage.has_value();
    auto held = emplace(storage);

    // Verify
    expect(threw);
    expect(not has_value_after_throw);
    expect(storage.has_value());
  };

  "owner allocates from the resource and frees on scope end"_test = []() {
    // Setup
    counting_resource resource;
    int const dtor_before = g_dtor;
    int value = 0;

    // Exercise
    {
      auto bus = hal::make_owner<fake_i2c>(&resource, 3);
      sensor s(bus);
      value = s.read();
    }

    // Verify
    expect(that % 3 == value);
    expect(that % 1 == resource.allocations);
    expect(that % 1 == resource.deallocations);
    expect(that % 1 == g_dtor - dtor_before);
  };

  "owner upcast destroys the complete object"_test = []() {
    // Setup
    counting_resource resource;
    int const dtor_before = g_dtor;

    // Exercise
    {
      hal::owner<i2c> bus = hal::make_owner<offset_i2c>(&resource, 4);
      expect(that % 4 == bus->read());
    }

    // Verify
    expect(that % 1 == g_dtor - dtor_before);
    expect(that % 1 == resource.deallocations);
  };

  "owner move leaves the source empty and frees once"_test = []() {
    // Setup
    counting_resource resource;

    // Exercise
    {
      auto first = hal::make_owner<fake_i2c>(&resource, 5);
      auto second = std::move(first);
      expect(not first);  // NOLINT(bugprone-use-after-move): checking it
      expect(static_cast<bool>(second));
    }

    // Verify
    expect(that % 1 == resource.allocations);
    expect(that % 1 == resource.deallocations);
  };

  "owner from a manager keeps the concrete type private"_test = []() {
    // Setup
    counting_resource resource;
    i2c_manager manager;

    // Exercise
    auto bus = manager.acquire(&resource);
    sensor s(bus);

    // Verify
    expect(that % 8 == s.read());
  };

  "make_owner frees the storage when construction throws"_test = []() {
    // Setup
    counting_resource resource;
    bool threw = false;

    // Exercise
    try {
      auto bad = hal::make_owner<throws_on_construct>(&resource);
    } catch (construct_error const&) {
      threw = true;
    }

    // Verify
    expect(threw);
    expect(that % 1 == resource.allocations);
    expect(that % 1 == resource.deallocations);
  };

  "std::optional<owner> ends an owner early"_test = []() {
    // Setup
    counting_resource resource;
    std::optional<hal::owner<i2c>> bus;

    // Exercise
    bus.emplace(hal::make_owner<fake_i2c>(&resource, 6));
    bool const held = bus.has_value();
    bus.reset();

    // Verify
    expect(held);
    expect(that % 1 == resource.deallocations);
  };
}

}  // namespace

int main()
{
  lifetime_test();
}

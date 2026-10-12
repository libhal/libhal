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

#include <cstddef>
#include <memory>
#include <memory_resource>
#include <optional>
#include <type_traits>
#include <utility>

#include <libhal/lifetime_macros.hpp>

#include <boost/ut.hpp>

import hal;

namespace {

int g_ctor = 0;
int g_dtor = 0;  // stands in for "peripheral reset"

struct i2c : hal::no_bare_new_mixin
{
  virtual int read() = 0;

protected:
  ~i2c() = default;  // no virtual dtor needed; slots destroy the full type
};

struct fake_i2c final : i2c
{
  explicit fake_i2c(int p_port)
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
  explicit sensor(hal::ref<i2c> p_bus HAL_LIFETIMEBOUND)
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

hal::lease<fake_i2c> acquire_i2c1()
{
  static constinit hal::static_slot<fake_i2c> slot;
  return slot.emplace(1);
}

// Typical application: owns its leases, composites borrow them. Members are
// destroyed in reverse order, so the sensor goes before the bus.
struct app
{
  hal::lease<i2c> bus = acquire_i2c1();
  sensor s{ bus };
};

struct throws_once : hal::no_bare_new_mixin
{
  static inline bool armed = true;
  throws_once()
  {
    if (std::exchange(armed, false)) {
      throw 42;
    }
  }
};

static_assert(sizeof(hal::ref<i2c>) == sizeof(void*));
static_assert(sizeof(hal::static_ref<i2c>) == sizeof(void*));
static_assert(sizeof(hal::lease<i2c>) == 2 * sizeof(void*));
static_assert(std::is_trivially_copyable_v<hal::ref<i2c>>);
static_assert(std::is_trivially_copyable_v<hal::static_ref<i2c>>);
static_assert(not std::is_copy_constructible_v<hal::lease<i2c>>);
static_assert(std::is_nothrow_move_constructible_v<hal::lease<i2c>>);

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

  "assume_static() over a startup arena"_test = []() {
    // Setup
    alignas(std::max_align_t) static std::byte arena_buffer[256];
    static std::pmr::monotonic_buffer_resource resource(arena_buffer,
                                                        sizeof arena_buffer);
    std::pmr::polymorphic_allocator<> allocator(&resource);
    auto* bus = allocator.new_object<fake_i2c>(5);  // never freed

    // Exercise
    sensor s(hal::static_ref<fake_i2c>::assume_static(*bus));

    // Verify
    expect(that % 5 == s.read());
  };

  "no_bare_new_mixin still allows placement and allocator paths"_test = []() {
    // Setup
    alignas(fake_i2c) static std::byte buffer[sizeof(fake_i2c)];

    // Exercise
    auto* placed = new (buffer) fake_i2c{ 1 };
    std::destroy_at(placed);
    auto* constructed = std::construct_at(reinterpret_cast<fake_i2c*>(buffer), 2);
    std::destroy_at(constructed);
    std::optional<fake_i2c> optional_bus;
    optional_bus.emplace(3);

    // Verify
    expect(that % 3 == optional_bus->read());
  };

  "static_slot is reused after each lease ends"_test = []() {
    // Setup
    int const ctor_before = g_ctor;
    int const dtor_before = g_dtor;

    // Exercise
    for (int i = 0; i < 3; i++) {
      app a;
      expect(that % 1 == a.s.read());
    }  // ~app: ~sensor, then ~lease -> ~fake_i2c (peripheral reset)

    // Verify
    expect(that % 3 == g_ctor - ctor_before);
    expect(that % 3 == g_dtor - dtor_before);
  };

  "static_slot throws while leased, frees at end of scope"_test = []() {
    // Setup
    bool threw = false;

    // Exercise
    {
      auto first = acquire_i2c1();
      try {
        auto second = acquire_i2c1();
      } catch (hal::slot_busy const&) {
        threw = true;
      }
    }  // first ends here, freeing the slot
    auto again = acquire_i2c1();

    // Verify
    expect(threw);
    expect(that % 1 == again->read());
  };

  "lease moves and upcasts"_test = []() {
    // Setup
    int const dtor_before = g_dtor;

    // Exercise
    {
      auto derived = acquire_i2c1();
      hal::lease<i2c> base = std::move(derived);  // upcast, ownership moves
      expect(not derived);
      expect(static_cast<bool>(base));
      expect(that % dtor_before == g_dtor);
    }  // only base destroys, and it destroys the full fake_i2c

    // Verify
    expect(that % 1 == g_dtor - dtor_before);
  };

  "failed construction leaves the slot free"_test = []() {
    // Setup
    static constinit hal::static_slot<throws_once> slot;
    bool threw = false;

    // Exercise
    try {
      auto lease = slot.emplace();
    } catch (int) {
      threw = true;
    }
    bool const in_use_after_throw = slot.in_use();
    auto lease = slot.emplace();

    // Verify
    expect(threw);
    expect(not in_use_after_throw);
    expect(slot.in_use());
  };
}

}  // namespace

int main()
{
  lifetime_test();
}

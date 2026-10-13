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

// Runtime lifetime-check cases (HAL_LIFETIME_CHECKS=1). Each case is built as
// its own executable with -DCASE_<name> and hosted_hooks.cpp (see
// lifetime_checks.cmake).
//   CATCH_*  must exit with code 3 (violation detected at the bad binding)
//   CLEAN_*  must exit with code 0 (no false positive)
//   MISS_*   known misses: exit 0
#include <array>
#include <cstddef>
#include <memory_resource>
#include <thread>

import hal;

struct i2c : hal::no_bare_new_mixin
{
  virtual int read() = 0;

protected:
  ~i2c() = default;
};

struct fake_i2c final : i2c
{
  explicit fake_i2c(int p_port)
    : port(p_port)
  {
  }
  int read() override
  {
    return port;
  }
  int port;
};

class sensor
{
public:
  // By value is the documented pattern for refs. With HAL_LIFETIME_CHECKS the
  // copy constructor runs the check, which is why it is not trivial here.
  // NOLINTNEXTLINE(performance-unnecessary-value-param)
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

[[maybe_unused]] static hal::static_ref<fake_i2c> get_static_bus() noexcept
{
  static fake_i2c bus{ 2 };
  return hal::static_ref<fake_i2c>{ bus };
}
[[maybe_unused]] static hal::owner<fake_i2c> acquire_i2c1()
{
  static constinit hal::reusable<fake_i2c> storage;
  return emplace(storage, 1);
}

// ---------------------------------------------------------------------------
#if defined(CASE_CATCH_STUDENT_FORGOT_STATIC)
// The classroom bug: `static` on the sensor, forgotten on the bus.
sensor& get_sensor()
{
  fake_i2c bus{ 1 };  // should have been static
  static sensor s(bus);
  return s;
}
int main()
{
  return get_sensor().read() == 1 ? 0 : 1;
}

#elif defined(CASE_CATCH_STATIC_REF_ESCAPE)
hal::static_ref<sensor> make()
{
  fake_i2c bus{ 1 };
  static sensor s(bus);
  return hal::static_ref<sensor>{ s };
}
int main()
{
  return make()->read() == 1 ? 0 : 1;
}

#elif defined(CASE_CATCH_GLOBAL_SENSOR_ASSIGNED)
struct holder
{
  hal::ref<i2c> bus;
};
static holder g_holder{ hal::ref<i2c>(get_static_bus()) };
int main()
{
  fake_i2c local{ 1 };
  g_holder.bus = hal::ref<i2c>(local);  // reassign a global's ref to a local
  return 0;
}

#elif defined(CASE_CATCH_ARENA_COMPOSITE_TO_LOCAL)
// assume_static arena objects live in .bss and are treated as static.
alignas(std::max_align_t) static std::array<std::byte, 256> arena_buf{};
int main()
{
  static std::pmr::monotonic_buffer_resource mr(arena_buf.data(),
                                                arena_buf.size());
  std::pmr::polymorphic_allocator<> a(&mr);
  fake_i2c local{ 1 };
  auto* s = a.new_object<sensor>(local);
  return s->read() == 1 ? 0 : 1;
}

#elif defined(CASE_CATCH_FORGOT_STATIC_IN_STD_THREAD)
// Same bug as above, but on a std::thread's stack instead of main's.
sensor& get_sensor()
{
  fake_i2c bus{ 1 };  // should have been static
  static sensor s(bus);
  return s;
}
int main()
{
  int result = 1;
  std::thread worker([&result] { result = get_sensor().read() == 1 ? 0 : 1; });
  worker.join();
  return result;
}

#elif defined(CASE_CLEAN_LOCAL_TO_LOCAL_IN_STD_THREAD)
int main()
{
  int result = 1;
  std::thread worker([&result] {
    fake_i2c bus{ 3 };
    sensor s(bus);
    result = s.read() == 3 ? 0 : 1;
  });
  worker.join();
  return result;
}

#elif defined(CASE_CLEAN_STATIC_TO_STATIC)
sensor& get_sensor()
{
  static sensor s(get_static_bus());
  return s;
}
int main()
{
  return get_sensor().read() == 2 ? 0 : 1;
}

#elif defined(CASE_CLEAN_LOCAL_TO_LOCAL)
int main()
{
  fake_i2c bus{ 3 };
  sensor s(bus);
  return s.read() == 3 ? 0 : 1;
}

#elif defined(CASE_CLEAN_LOCAL_TO_STATIC)
int main()
{
  sensor s(get_static_bus());
  return s.read() == 2 ? 0 : 1;
}

#elif defined(CASE_CLEAN_APP_WITH_OWNER)
struct app
{
  hal::owner<i2c> bus = acquire_i2c1();
  sensor s{ bus };
};
int main()
{
  for (int i = 0; i < 3; i++) {
    app a;
    if (a.s.read() != 1) {
      return 1;
    }
  }
  return 0;
}

#elif defined(CASE_CLEAN_STATIC_APP_WITH_STATIC_BUS)
struct app
{
  fake_i2c bus{ 4 };
  sensor s{ bus };
};
int main()
{
  static app a;  // both in .bss
  return a.s.read() == 4 ? 0 : 1;
}

#elif defined(CASE_MISS_STATIC_SENSOR_ON_LOCAL_OWNER)
// The owned driver lives in reusable storage (.bss), so the target
// looks static.
sensor& get_sensor()
{
  auto l = acquire_i2c1();
  static sensor s(l);
  return s;
}
int main()
{
  (void)get_sensor();
  return 0;
}

#else
#error "define one CASE_*"
#endif

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

// Compile-time checks for hal:lifetime. Each case is built as its own target
// with -DCASE_<name> (see compile_checks.cmake):
//   ERR_*   must be rejected by the compiler
//   WARN_*  must be reported by Clang (built with -Werror -Wlifetime-safety)
//   GAP_*   known misses: must compile cleanly, so a future fix shows up

#include <memory>
#include <memory_resource>
#include <utility>

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

[[maybe_unused]] static fake_i2c g_bus{ 7 };

[[maybe_unused]] static hal::owner<fake_i2c> acquire_i2c1()
{
  static constinit hal::reusable<fake_i2c> storage;
  return emplace(storage, 1);
}

[[maybe_unused]] static hal::owner<fake_i2c> make_bus()
{
  return hal::make_owner<fake_i2c>(std::pmr::new_delete_resource(), 1);
}

class i2c_manager
{
public:
  [[nodiscard]] hal::owner<i2c> acquire(
    std::pmr::polymorphic_allocator<> p_allocator) [[clang::lifetimebound]]
  {
    return hal::make_owner<fake_i2c>(p_allocator, 1);
  }
};

// ---- must not compile -----------------------------------------------------
#if defined(CASE_ERR_STATIC_REF_LOCAL)
int main()
{
  fake_i2c local{ 1 };
  hal::static_ref<fake_i2c> r{ local };
  return r->read();
}
#elif defined(CASE_ERR_STATIC_REF_HEAP)
int main()
{
  auto* p = ::new fake_i2c{ 1 };
  hal::static_ref<fake_i2c> r{ *p };
  return r->read();
}
#elif defined(CASE_ERR_STATIC_REF_RUNTIME_PARAM)
hal::static_ref<fake_i2c> launder(fake_i2c& p_bus)
{
  return hal::static_ref<fake_i2c>{ p_bus };
}
int main()
{
  return launder(g_bus)->read();
}
#elif defined(CASE_ERR_STATIC_REF_THREAD_LOCAL)
thread_local fake_i2c t_bus{ 1 };
int main()
{
  hal::static_ref<fake_i2c> r{ t_bus };
  return r->read();
}
#elif defined(CASE_ERR_STATIC_REF_FROM_REUSABLE)
// An object in a reusable is in static storage but NOT of static lifetime.
int main()
{
  auto l = acquire_i2c1();
  hal::static_ref<fake_i2c> r{ *l };
  return r->read();
}
#elif defined(CASE_ERR_REF_TEMPORARY)
int main()
{
  sensor s(fake_i2c{ 1 });
  return s.read();
}
#elif defined(CASE_ERR_REF_UNIQUE_PTR)
int main()
{
  std::unique_ptr<i2c> p;
  sensor s(p);
  return s.read();
}
#elif defined(CASE_ERR_REF_RAW_PTR)
int main()
{
  i2c* p = &g_bus;
  hal::ref<i2c> r(p);
  return r->read();
}
#elif defined(CASE_ERR_REF_RETURN_LOCAL)
hal::ref<i2c> f()
{
  fake_i2c l{ 1 };
  return l;
}
int main()
{
  return f()->read();
}
#elif defined(CASE_ERR_OWNER_RESET_REMOVED)
// Owners end only by scope: an early reset() is invisible to the analysis
// while a composite still borrows the owned object.
int main()
{
  auto l = acquire_i2c1();
  sensor s(l);
  l.reset();
  return s.read();
}
#elif defined(CASE_ERR_REF_FROM_TEMP_OWNER)
int main()
{
  sensor s(make_bus());  // owner dies at ';' -> driver destroyed
  return s.read();
}
#elif defined(CASE_ERR_DEREF_TEMP_OWNER)
int main()
{
  sensor s(*make_bus());
  return s.read();
}
#elif defined(CASE_ERR_ARROW_TEMP_OWNER)
int main()
{
  return make_bus()->read();
}
#elif defined(CASE_ERR_OWNER_COPY)
int main()
{
  auto a = make_bus();
  auto b = a;
  return b->read();
}
#elif defined(CASE_ERR_OWNER_MOVE_ASSIGN)
int main()
{
  auto a = make_bus();
  hal::owner<fake_i2c> b = std::move(a);
  b = make_bus();
  return 0;
}
#elif defined(CASE_ERR_EMPLACE_LOCAL_STORAGE)
// emplace() takes the storage as a static_ref, so a local is rejected.
int main()
{
  hal::reusable<fake_i2c> storage;  // forgot static
  auto bus = emplace(storage, 1);
  return bus->read();
}
#elif defined(CASE_ERR_EMPLACE_RUNTIME_STORAGE_PARAM)
// A reference parameter is never a constant, even if it names a static.
hal::owner<fake_i2c> fill(hal::reusable<fake_i2c>& p_storage)
{
  return emplace(p_storage, 1);
}
int main()
{
  static constinit hal::reusable<fake_i2c> storage;
  auto bus = fill(storage);
  return bus->read();
}
#elif defined(CASE_ERR_BARE_NEW)
int main()
{
  auto* p = new fake_i2c{ 1 };
  return p->read();
}
#elif defined(CASE_ERR_DELETE)
// Deleting through the class would destroy the object and skip freeing it.
int main()
{
  auto* p = ::new fake_i2c{ 1 };
  int const value = p->read();
  delete p;
  return value;
}
#elif defined(CASE_ERR_MAKE_UNIQUE)
int main()
{
  auto p = std::make_unique<fake_i2c>(1);
  return p->read();
}

// ---- Clang reports these --------------------------------------------------
#elif defined(CASE_WARN_RETURN_SENSOR_LOCAL)
sensor f()
{
  fake_i2c l{ 1 };
  return sensor(l);
}
int main()
{
  return f().read();
}
#elif defined(CASE_WARN_ESCAPE_TO_STATIC)
// The classic "forgot static on the bus" bug.
sensor* g_sensor = nullptr;
void f()
{
  fake_i2c l{ 1 };
  static sensor s(l);
  g_sensor = &s;
}
int main()
{
  f();
  return g_sensor->read();
}
#elif defined(CASE_WARN_INTERMEDIATE_REF)
sensor f()
{
  fake_i2c l{ 1 };
  hal::ref<i2c> r = l;
  return sensor(r);
}
int main()
{
  return f().read();
}

#elif defined(CASE_WARN_RETURN_SENSOR_LOCAL_OWNER)
sensor f()
{
  auto bus = make_bus();
  return sensor(bus);  // owner destroyed at return
}
int main()
{
  return f().read();
}

#elif defined(CASE_WARN_OWNER_OUTLIVES_MANAGER)
// acquire() is lifetimebound, so the owner must not outlive the manager.
hal::owner<i2c> f()
{
  i2c_manager manager;
  return manager.acquire(std::pmr::new_delete_resource());
}
int main()
{
  auto bus = f();
  return bus->read();
}

// ---- known gaps: compile cleanly -------------------------------------------
#elif defined(CASE_GAP_HEAP_DEREF)
// `*::new T` passed to a ref is not caught.
int main()
{
  sensor s(*::new fake_i2c{ 1 });
  return s.read();
}

#else
#error "define one CASE_*"
#endif

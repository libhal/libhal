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

// hal:lifetime misuse that Clang's lifetime safety analysis reports.

#include <memory_resource>

#include "fixtures.hpp"

sensor return_sensor_on_local_bus()
{
  fake_i2c local{ 1 };
  // expected-error@+2 {{is returned}}
  // expected-error@+1 {{address of stack memory}}
  return sensor(local);
}

sensor return_sensor_through_ref()
{
  fake_i2c local{ 1 };
  // expected-error@+1 {{is returned}}
  hal::ref<i2c> r = local;
  return sensor(r);
}

sensor return_sensor_on_local_owner()
{
  auto bus = make_bus();
  // expected-error@+2 {{is returned}}
  // expected-error@+1 {{address of stack memory}}
  return sensor(bus);
}

// acquire() is lifetimebound, so the owner must not outlive the manager.
hal::owner<i2c> return_owner_past_manager()
{
  i2c_manager manager;
  // expected-error@+2 {{is returned}}
  // expected-error@+1 {{address of stack memory}}
  return manager.acquire(std::pmr::new_delete_resource());
}

// The factory is lifetimebound to the manager, so a temporary one is caught.
hal::owner<pinned_i2c> heap_resource_from_temporary_manager()
{
  return hal::make_owner_from(std::pmr::new_delete_resource(), [] {
    // expected-error@+2 {{is returned}}
    // expected-error@+1 {{returning address of local temporary object}}
    return pin_manager{}.acquire();
  });
}

// The classic "forgot static on the bus" bug.
sensor const* escape_to_static()
{
  fake_i2c local{ 1 };
  // expected-error@+1 {{which will dangle}}
  static sensor s(local);
  return &s;
}

// Known gap: `*::new T` passed to a ref is not caught.
int heap_dereference()
{
  sensor s(*::new fake_i2c{ 1 });
  return s.read();
}

// Known gap: the owner from make_owner_from() outliving a local manager is not
// caught, because the lambda hides the manager from the analysis.
hal::owner<pinned_i2c> heap_resource_past_manager()
{
  pin_manager manager;
  return hal::make_owner_from(std::pmr::new_delete_resource(),
                              [&] { return manager.acquire(); });
}

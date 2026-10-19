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
#include <optional>
#include <utility>

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

// The factory is taken by value, so the analysis sees what it captures: the
// owner can't outlive a local manager.
hal::owner<pinned_i2c> heap_resource_past_manager()
{
  pin_manager manager;
  return hal::make_owner_from(std::pmr::new_delete_resource(),
                              // expected-error@+2 {{is returned}}
                              // expected-error@+1 {{address of stack memory}}
                              [&] { return manager.acquire(); });
}

// A static manager is fine: no false positive from the temporary lambda.
hal::owner<pinned_i2c> heap_resource_from_static_manager()
{
  static pin_manager manager;
  return hal::make_owner_from(std::pmr::new_delete_resource(),
                              [&] { return manager.acquire(); });
}

// ---- composing owners: an IMU owner that borrows a bus owner ---------------

// The IMU's owner can't outlive the bus owner it borrows.
hal::owner<fake_imu> imu_owner_past_local_bus()
{
  auto bus = make_bus();
  return hal::make_owner_from(std::pmr::new_delete_resource(),
                              // expected-error@+2 {{is returned}}
                              // expected-error@+1 {{address of stack memory}}
                              [&] { return fake_imu(bus); });
}

// Naming the interface as the target type keeps the borrow.
hal::owner<imu> imu_owner_as_interface_past_local_bus()
{
  auto bus = make_bus();
  return hal::make_owner_from<imu>(
    std::pmr::new_delete_resource(),
    // expected-error@+2 {{is returned}}
    // expected-error@+1 {{address of stack memory}}
    [&] { return fake_imu(bus); });
}

// Known gap: converting the returned owner (owner<fake_imu> to owner<imu>)
// drops the borrow, so the same bug is not caught. Use make_owner_from<imu>
// instead. Making the converting constructor lifetimebound would flag every
// ordinary upcast.
hal::owner<imu> imu_owner_upcast_past_local_bus()
{
  auto bus = make_bus();
  return hal::make_owner_from(std::pmr::new_delete_resource(),
                              [&] { return fake_imu(bus); });
}

// Known gap: make_owner() forwards its arguments without lifetimebound, so
// a borrowed argument is not tracked.
hal::owner<fake_imu> imu_make_owner_past_local_bus()
{
  auto bus = make_bus();
  return hal::make_owner<fake_imu>(std::pmr::new_delete_resource(), bus);
}

// Known gap: an owner stored in an outer optional outliving the bus.
int imu_outlives_bus_in_scope()
{
  std::optional<hal::owner<imu>> device;
  {
    auto bus = make_bus();
    device.emplace(hal::make_owner_from(std::pmr::new_delete_resource(),
                                        [&] { return fake_imu(bus); }));
  }
  return (*device)->acceleration();
}

// No false positives below.

hal::owner<imu> imu_owner_on_static_bus()
{
  static auto bus = make_bus();
  return hal::make_owner_from(std::pmr::new_delete_resource(),
                              [&] { return fake_imu(bus); });
}

struct imu_stack
{
  hal::owner<i2c> bus;
  hal::owner<imu> device;
};

// Both owners move out together; the objects stay where they were allocated.
imu_stack imu_stack_moved_out()
{
  hal::owner<i2c> bus = make_bus();
  hal::owner<imu> device = hal::make_owner_from(std::pmr::new_delete_resource(),
                                                [&] { return fake_imu(bus); });
  return { .bus = std::move(bus), .device = std::move(device) };
}

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

// hal::spi_bus_manager misuse that Clang's lifetime safety analysis reports.

#include "fixtures.hpp"

namespace {
// Static, so it can't be what the checks below report.
hal::no_op_lock single_context;
}  // namespace

hal::spi_bus_manager<>::channel channel_outlives_manager(fake_spi_bus& p_bus,
                                                         fake_output_pin& p_pin)
{
  hal::spi_bus_manager local(p_bus, single_context);
  // Only Clang's returned-stack-address warning reports this one. The lifetime
  // safety analysis doesn't follow the call through the conversion to the
  // manager's base class, where acquire_channel() is declared.
  // expected-error@+1 {{address of stack memory}}
  return local.acquire_channel(p_pin);
}

hal::spi_bus_manager<>::channel channel_outlives_pin(
  hal::spi_bus_manager<>& p_manager)
{
  fake_output_pin local;
  // expected-error@+2 {{is returned}}
  // expected-error@+1 {{address of stack memory}}
  return p_manager.acquire_channel(local);
}

hal::spi_bus_manager<> const* static_manager_on_local_bus()
{
  fake_spi_bus bus;  // forgot static
  // expected-error@+1 {{which will dangle}}
  static hal::spi_bus_manager manager(bus, single_context);
  return &manager;
}

// An owning manager that is static, built by a factory with no captures, is
// fine: nothing here may warn.
hal::spi_bus_manager<fake_spi_bus>& static_owning_manager()
{
  static hal::spi_bus_manager manager([] { return fake_spi_bus{}; },
                                      single_context);
  return manager;
}

// A factory that captures a local by reference ties the manager to it.
hal::spi_bus_manager<fake_spi_bus> const* static_owning_manager_on_local()
{
  int local = 0;  // stands in for anything the bus would borrow
  // Reported at the capture.
  // expected-error@+2 {{which will dangle}}
  static hal::spi_bus_manager manager(
    [&local] {
      static_cast<void>(local);
      return fake_spi_bus{};
    },
    single_context);
  return &manager;
}

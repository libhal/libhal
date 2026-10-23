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

hal::spi_bus_manager::channel channel_outlives_manager(fake_spi_bus& p_bus,
                                                       fake_output_pin& p_pin)
{
  hal::spi_bus_manager local(p_bus);
  // expected-error@+2 {{is returned}}
  // expected-error@+1 {{address of stack memory}}
  return local.acquire_channel(p_pin);
}

hal::spi_bus_manager::channel channel_outlives_pin(
  hal::spi_bus_manager& p_manager)
{
  fake_output_pin local;
  // expected-error@+2 {{is returned}}
  // expected-error@+1 {{address of stack memory}}
  return p_manager.acquire_channel(local);
}

hal::spi_bus_manager const* static_manager_on_local_bus()
{
  fake_spi_bus bus;  // forgot static
  // expected-error@+1 {{which will dangle}}
  static hal::spi_bus_manager manager(bus);
  return &manager;
}

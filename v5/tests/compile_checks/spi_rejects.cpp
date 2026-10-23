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

// hal::spi_bus_manager misuse that the type system rejects.

#include "fixtures.hpp"

// The bus must outlive the manager.
void manager_on_temporary_bus()
{
  // expected-error@+1 {{invokes a deleted function}}
  hal::spi_bus_manager manager(fake_spi_bus{});
}

// The chip select pin must outlive the channel.
void channel_on_temporary_pin(hal::spi_bus_manager<>& p_manager)
{
  // expected-error@+1 {{invokes a deleted function}}
  auto channel = p_manager.acquire_channel(fake_output_pin{});
}

// The manager tracks channels by address.
void copy_channel(hal::spi_bus_manager<>& p_manager, fake_output_pin& p_pin)
{
  auto channel = p_manager.acquire_channel(p_pin);
  // expected-error@+1 {{call to deleted constructor}}
  auto copy = channel;
}

// Only the manager creates channels.
void construct_channel_directly(hal::spi_bus_manager<>& p_manager,
                                fake_output_pin& p_pin)
{
  // expected-error@+1 {{calling a private constructor}}
  hal::spi_bus_manager<>::channel channel(p_manager, p_pin, {});
}

// The factory has to return a bus.
void factory_returns_something_else()
{
  // expected-error@*:* {{The bus type must implement hal::raw_spi_bus}}
  hal::spi_bus_manager manager([] { return 5; });
}

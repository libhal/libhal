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

// hal:lifetime misuse that the type system rejects.

#include <memory>
#include <utility>

#include "fixtures.hpp"

int static_ref_to_local()
{
  fake_i2c local{ 1 };
  // expected-error@+1 {{is not a constant expression}}
  hal::static_ref<fake_i2c> r{ local };
  return r->read();
}

int static_ref_to_heap()
{
  auto* p = ::new fake_i2c{ 1 };
  // expected-error@+1 {{is not a constant expression}}
  hal::static_ref<fake_i2c> r{ *p };
  return r->read();
}

hal::static_ref<fake_i2c> static_ref_from_parameter(fake_i2c& p_bus)
{
  // expected-error@+1 {{is not a constant expression}}
  return hal::static_ref<fake_i2c>{ p_bus };
}

thread_local fake_i2c t_bus{ 1 };
int static_ref_to_thread_local()
{
  // expected-error@+1 {{is not a constant expression}}
  hal::static_ref<fake_i2c> r{ t_bus };
  return r->read();
}

// An object in a reusable is in static storage but NOT of static lifetime.
int static_ref_to_reusable_object()
{
  auto bus = acquire_i2c1();
  // expected-error@+1 {{is not a constant expression}}
  hal::static_ref<fake_i2c> r{ *bus };
  return r->read();
}

// emplace() takes the storage as a static_ref, so a local is rejected.
int emplace_into_local_storage()
{
  hal::reusable<fake_i2c> storage;  // forgot static
  // expected-error@+1 {{is not a constant expression}}
  auto bus = emplace(storage, 1);
  return bus->read();
}

// A reference parameter is never a constant, even if it names a static.
hal::owner<fake_i2c> emplace_through_parameter(
  hal::reusable<fake_i2c>& p_storage)
{
  // expected-error@+1 {{is not a constant expression}}
  return emplace(p_storage, 1);
}

int ref_to_temporary()
{
  // expected-error@+1 {{invokes a deleted function}}
  sensor s(fake_i2c{ 1 });
  return s.read();
}

int ref_to_unique_ptr(std::unique_ptr<i2c> const& p_bus)
{
  // expected-error@+1 {{invokes a deleted function}}
  sensor s(p_bus);
  return s.read();
}

int ref_from_raw_pointer()
{
  i2c* p = &g_bus;
  // expected-error@+1 {{call to deleted constructor}}
  hal::ref<i2c> r(p);
  return r->read();
}

hal::ref<i2c> ref_to_returned_local()
{
  fake_i2c local{ 1 };
  // expected-error@+1 {{invokes a deleted function}}
  return local;
}

int ref_to_temporary_owner()
{
  // expected-error@+1 {{invokes a deleted function}}
  sensor s(make_bus());
  return s.read();
}

int dereference_temporary_owner()
{
  // expected-error@+1 {{deleted operator '*'}}
  sensor s(*make_bus());
  return s.read();
}

int arrow_on_temporary_owner()
{
  // expected-error@+1 {{deleted operator '->'}}
  return make_bus()->read();
}

int copy_owner()
{
  auto a = make_bus();
  // expected-error@+1 {{implicitly-deleted copy constructor}}
  auto b = a;
  return b->read();
}

void move_assign_owner()
{
  auto a = make_bus();
  hal::owner<fake_i2c> b = std::move(a);
  // expected-error@+1 {{deleted operator '='}}
  b = make_bus();
}

// Owners end only by scope: an early reset() would be invisible to the
// lifetime analysis while a composite still borrows the owned object.
int reset_owner()
{
  auto bus = acquire_i2c1();
  sensor s(bus);
  // expected-error@+1 {{no member named 'reset'}}
  bus.reset();
  return s.read();
}

int bare_new()
{
  // expected-error@+1 {{deleted function 'operator new'}}
  auto* p = new fake_i2c{ 1 };
  return p->read();
}

// Both errors are reported inside the standard library.
int make_unique()
{
  // expected-error@*:* {{call to deleted function 'operator new'}}
  // expected-error@*:* {{attempt to use a deleted function}}
  auto p = std::make_unique<fake_i2c>(1);
  return p->read();
}

// Deleting through the class would destroy the object and skip freeing it.
int delete_object()
{
  auto* p = ::new fake_i2c{ 1 };
  int const value = p->read();
  // expected-error@+1 {{attempt to use a deleted function}}
  delete p;
  return value;
}

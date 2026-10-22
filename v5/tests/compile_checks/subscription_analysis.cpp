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

// Single-subscriber callback misuse that Clang's lifetime safety analysis
// reports, using edge_triggered_interrupt as the example source.

#include <optional>

#include "fixtures.hpp"

hal::subscription subscription_outlives_callback(fake_pin& p_pin)
{
  counter local;
  // expected-error@+2 {{is returned}}
  // expected-error@+1 {{address of stack memory}}
  return p_pin.on_trigger(local);
}

hal::subscription subscription_outlives_pin()
{
  static counter c;
  fake_pin local;
  // expected-error@+2 {{is returned}}
  // expected-error@+1 {{address of stack memory}}
  return local.on_trigger(c);
}

button_counter return_counter_past_local_pin()
{
  fake_pin pin;
  // expected-error@+2 {{is returned}}
  // expected-error@+1 {{address of stack memory}}
  return button_counter(pin);
}

button_counter const* static_counter_on_local_pin()
{
  fake_pin pin;  // forgot static
  // expected-error@+1 {{which will dangle}}
  static button_counter c(pin);
  return &c;
}

// Known gaps: the analysis does not see through std::optional, so a
// swappable std::optional<hal::subscription> loses the checks above. Assign a
// plain hal::subscription instead (see the end of this file).

std::optional<hal::subscription> optional_outlives_callback(fake_pin& p_pin)
{
  counter local;
  std::optional<hal::subscription> active;
  active.emplace(p_pin.on_trigger(local));
  return active;
}

std::optional<hal::subscription> optional_direct_init_outlives_callback(
  fake_pin& p_pin)
{
  counter local;
  return std::optional<hal::subscription>(p_pin.on_trigger(local));
}

std::optional<hal::subscription> optional_outlives_pin()
{
  static counter c;
  fake_pin local;
  std::optional<hal::subscription> active;
  active.emplace(local.on_trigger(c));
  return active;
}

void static_optional_on_local_callback()
{
  static fake_pin pin;
  static std::optional<hal::subscription> active;
  counter local;  // forgot static
  active.emplace(pin.on_trigger(local));
}

// Assignment is tracked like initialization, so an assignable subscription
// keeps the checks that std::optional<hal::subscription> loses above.

hal::subscription assigned_outlives_callback(fake_pin& p_pin)
{
  counter local;
  hal::subscription active;
  // expected-error@+1 {{is returned}}
  active = p_pin.on_trigger(local);
  return active;
}

hal::subscription assigned_outlives_pin()
{
  static counter c;
  fake_pin local;
  hal::subscription active;
  // expected-error@+1 {{is returned}}
  active = local.on_trigger(c);
  return active;
}

late_button_counter return_late_counter_past_local_pin()
{
  fake_pin pin;
  // expected-error@+2 {{is returned}}
  // expected-error@+1 {{address of stack memory}}
  return late_button_counter(pin);
}

void static_assigned_on_local_callback()
{
  static fake_pin pin;
  static hal::subscription active;
  counter local;  // forgot static
  // expected-error@+1 {{which will dangle}}
  active = pin.on_trigger(local);
}

hal::subscription reassigned_to_local_callback(fake_pin& p_pin)
{
  static counter c;
  counter local;
  hal::subscription active = p_pin.on_trigger(c);
  active.reset();
  // expected-error@+1 {{is returned}}
  active = p_pin.on_trigger(local);
  return active;
}

// No false positive: the local callback's subscription was reset before the
// static one was assigned.
hal::subscription reassigned_to_static_callback(fake_pin& p_pin)
{
  static counter c;
  counter local;
  hal::subscription active = p_pin.on_trigger(local);
  active.reset();
  active = p_pin.on_trigger(c);
  return active;
}

// Known false positive: the analysis doesn't see reset() end the borrow, so a
// reset subscription still counts as pointing at the local callback, even with
// [[clang::reinitializes]]. Only returning one triggers it, and returning an
// empty subscription has no practical use. Recorded so we notice if Clang
// starts treating reset() as ending the borrow.
hal::subscription reset_then_returned(fake_pin& p_pin)
{
  counter local;
  // expected-error@+1 {{is returned}}
  hal::subscription active = p_pin.on_trigger(local);
  active.reset();
  return active;
}

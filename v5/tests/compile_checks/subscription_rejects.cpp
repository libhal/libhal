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

// Single-subscriber callback misuse that the type system rejects, using
// edge_triggered_interrupt as the example source.

#include "fixtures.hpp"

// Permanent registration needs a callback with static storage.
void static_ref_to_local_callback()
{
  static fake_pin pin;
  counter local;
  // expected-error@+1 {{is not a constant expression}}
  pin.on_trigger(hal::static_ref<hal::edge_triggered_callback>{ local });
}

// A discarded subscription unregisters immediately.
void discard_subscription()
{
  fake_pin pin;
  counter c;
  // expected-error@+1 {{ignoring return value}}
  pin.on_trigger(c);
}

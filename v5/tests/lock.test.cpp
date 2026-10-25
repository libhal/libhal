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

#include <chrono>
#include <mutex>

#include <boost/ut.hpp>

import hal;

namespace {
// Records every call, the way an RTOS mutex adapter would forward them.
class fake_lock final : public hal::timed_lock
{
public:
  bool locked = false;
  bool next_try_succeeds = true;
  int lock_count = 0;
  int unlock_count = 0;
  hal::time_duration last_duration{};

private:
  void driver_lock() override
  {
    locked = true;
    ++lock_count;
  }

  void driver_unlock() override
  {
    locked = false;
    ++unlock_count;
  }

  bool driver_try_lock() override
  {
    locked = next_try_succeeds;
    return locked;
  }

  bool driver_try_lock_for(hal::time_duration p_duration) override
  {
    last_duration = p_duration;
    locked = next_try_succeeds;
    return locked;
  }
};

// The concepts describe the standard traits, so standard locks satisfy them.
static_assert(hal::basic_lockable<std::mutex>);
static_assert(hal::lockable<std::mutex>);
static_assert(hal::timed_lockable<std::timed_mutex>);
static_assert(not hal::timed_lockable<std::mutex>);
static_assert(hal::timed_lockable<fake_lock>);
static_assert(hal::timed_lockable<hal::no_op_lock>);

void lock_test()
{
  using namespace boost::ut;
  using namespace std::chrono_literals;

  "scoped_lock locks and unlocks through the interface"_test = []() {
    // Setup
    fake_lock lock;
    hal::basic_lock& basic = lock;
    bool locked_inside = false;

    // Exercise
    {
      std::scoped_lock guard(basic);
      locked_inside = lock.locked;
    }

    // Verify
    expect(locked_inside);
    expect(not lock.locked);
    expect(that % 1 == lock.lock_count);
    expect(that % 1 == lock.unlock_count);
  };

  "lock_guard locks and unlocks through the interface"_test = []() {
    // Setup
    fake_lock lock;
    hal::basic_lock& basic = lock;
    bool locked_inside = false;

    // Exercise
    {
      std::lock_guard guard(basic);
      locked_inside = lock.locked;
    }

    // Verify
    expect(locked_inside);
    expect(not lock.locked);
    expect(that % 1 == lock.lock_count);
    expect(that % 1 == lock.unlock_count);
  };

  "try_lock reports whether the lock was taken"_test = []() {
    // Setup
    fake_lock lock;
    hal::pollable_lock& pollable = lock;

    // Exercise
    lock.next_try_succeeds = false;
    bool const first = pollable.try_lock();
    lock.next_try_succeeds = true;
    bool const second = pollable.try_lock();

    // Verify
    expect(not first);
    expect(second);
  };

  "unique_lock with try_to_lock uses try_lock"_test = []() {
    // Setup
    fake_lock lock;
    hal::pollable_lock& pollable = lock;
    lock.next_try_succeeds = false;

    // Exercise
    std::unique_lock guard(pollable, std::try_to_lock);

    // Verify
    expect(not guard.owns_lock());
    expect(that % 0 == lock.lock_count);
  };

  "try_lock_for passes the duration to the implementation"_test = []() {
    // Setup
    fake_lock lock;
    hal::timed_lock& timed = lock;

    // Exercise
    bool const acquired = timed.try_lock_for(5ms);

    // Verify
    expect(acquired);
    expect(lock.last_duration == hal::time_duration(5ms));
  };

  "a no-op lock never blocks and always succeeds"_test = []() {
    // Setup
    hal::no_op_lock lock;
    hal::timed_lock& timed = lock;

    // Exercise
    timed.lock();
    // Taking it again doesn't block, since it doesn't lock.
    timed.lock();
    timed.unlock();
    timed.unlock();
    bool const tried = timed.try_lock();
    bool const tried_for = timed.try_lock_for(5ms);

    // Verify
    expect(tried);
    expect(tried_for);
  };
}
}  // namespace

int main()
{
  lock_test();
}

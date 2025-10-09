// Copyright 2024 - 2025 Khalil Estell and the libhal contributors
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

#pragma once

#include "units.hpp"

namespace hal {
/**
 * @brief Interface for a basic lock satisfying C++'s BasicLockable trait.
 *
 */
class basic_lock
{
public:
  /**
   * @brief Acquire a lock
   *
   * Block the current thread of execution until the lock has been acquired.
   *
   */
  void lock()
  {
    return os_lock();
  }

  /**
   * @brief Release a lock
   *
   */
  void unlock()
  {
    return os_unlock();
  }

  virtual ~basic_lock() = default;

private:
  virtual void os_lock() = 0;
  virtual void os_unlock() = 0;
};

/**
 * @brief Interface for a lock satisfying C++'s Lockable trait.
 *
 */
class pollable_lock : public basic_lock
{
public:
  /**
   * @brief Attempt to acquire a lock
   *
   * This call will not block the current thread of execution.
   *
   * @return true - if the lock has been acquired
   * @return false - if the lock could not be acquired
   */
  bool try_lock()
  {
    return os_try_lock();
  }

  ~pollable_lock() override = default;

private:
  virtual bool os_try_lock() = 0;
};

/**
 * @brief Interface for a timed lock satisfying C++'s TimedLockable trait.
 *
 */
class timed_lock : public pollable_lock
{
public:
  /**
   * @brief Attempt to acquire a lock and block for a duration of time.
   *
   * This call will block for the supplied amount of time and then return a
   * status on whether or not the lock was acquired.
   *
   * @param p_duration - the amount of time to block the current thread waiting
   * for the lock to be acquired. Note that the precision of the lock's timing
   * mechanism may not be as precise as the time duration. Expect the time
   * duration that the thread blocks for to be the rounded down to the nearest
   * precision unit of time. For example, if the precision of the system that
   * manages the lock is 1ms, then providing 2.5ms will result in 2ms waited.
   * @return true - if the lock was acquired in the time provided.
   * @return false - if the lock was not acquired in the time provided.
   */
  bool try_lock_for(hal::time_duration p_duration)
  {
    return os_try_lock_for(p_duration);
  }

  ~timed_lock() override = default;

private:
  virtual bool os_try_lock_for(hal::time_duration p_duration) = 0;
};

/**
 * @brief concept for basic_lockable
 *
 * @tparam Lock - lock type
 */
template<class Lock>
concept basic_lockable = requires(Lock lockable) {
  { lockable.lock() } -> std::same_as<void>;
  { lockable.unlock() } -> std::same_as<void>;
};

/**
 * @brief concept for lockable which extends basic_lockable
 *
 * @tparam Lock - lock type
 */
template<class Lock>
concept lockable = basic_lockable<Lock> && requires(Lock lockable) {
  { lockable.try_lock() } -> std::same_as<bool>;
};

/**
 * @brief concept for timed_lockable which extends lockable
 *
 * @tparam Lock - lock type
 */
template<class Lock>
concept timed_lockable =
  lockable<Lock> && requires(Lock lockable, hal::time_duration duration) {
    { lockable.try_lock_for(duration) } -> std::same_as<bool>;
  };

// static asserts to check conformance to the concepts
static_assert(basic_lockable<basic_lock>,
              "basic_lock must satisfy basic_lockable");
static_assert(lockable<pollable_lock>, "pollable_lock must satisfy lockable");
static_assert(timed_lockable<timed_lock>,
              "timed_lock must satisfy timed_lockable");
}  // namespace hal

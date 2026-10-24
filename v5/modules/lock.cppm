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

module;

#include <concepts>

export module hal:lock;

import :units;

namespace hal::inline v5 {
/**
 * @brief Interface for a basic lock satisfying C++'s BasicLockable trait.
 *
 * Implemented by whatever runs more than one context, such as an RTOS, a
 * fiber scheduler, or a host OS, so that drivers and utilities can serialize
 * access to shared hardware without depending on any one of them. Works with
 * `std::scoped_lock` and `std::unique_lock`.
 *
 * A program with a single context passes a `hal::no_op_lock` where an API
 * asks for one.
 */
export class basic_lock
{
public:
  /**
   * @brief Acquire a lock
   *
   * Block the current context until the lock has been acquired.
   *
   */
  void lock()
  {
    driver_lock();
  }

  /**
   * @brief Release a lock
   *
   */
  void unlock()
  {
    driver_unlock();
  }

protected:
  ~basic_lock() = default;

private:
  virtual void driver_lock() = 0;
  virtual void driver_unlock() = 0;
};

/**
 * @brief Interface for a lock satisfying C++'s Lockable trait.
 *
 */
export class pollable_lock : public basic_lock
{
public:
  /**
   * @brief Attempt to acquire a lock
   *
   * This call will not block the current context.
   *
   * @return true - if the lock has been acquired
   * @return false - if the lock could not be acquired
   */
  [[nodiscard]] bool try_lock()
  {
    return driver_try_lock();
  }

protected:
  ~pollable_lock() = default;

private:
  virtual bool driver_try_lock() = 0;
};

/**
 * @brief Interface for a timed lock satisfying C++'s TimedLockable trait.
 *
 */
export class timed_lock : public pollable_lock
{
public:
  /**
   * @brief Attempt to acquire a lock and block for a duration of time.
   *
   * This call will block for the supplied amount of time and then return a
   * status on whether or not the lock was acquired.
   *
   * @param p_duration - the amount of time to block the current context
   * waiting for the lock to be acquired. Note that the precision of the lock's
   * timing mechanism may not be as precise as the time duration. Expect the
   * time duration that the context blocks for to be the rounded down to the
   * nearest precision unit of time. For example, if the precision of the
   * system that manages the lock is 1ms, then providing 2.5ms will result in
   * 2ms waited.
   * @return true - if the lock was acquired in the time provided.
   * @return false - if the lock was not acquired in the time provided.
   */
  [[nodiscard]] bool try_lock_for(time_duration p_duration)
  {
    return driver_try_lock_for(p_duration);
  }

protected:
  ~timed_lock() = default;

private:
  virtual bool driver_try_lock_for(time_duration p_duration) = 0;
};

/**
 * @brief A lock that doesn't lock, for code that runs in one context
 *
 * Every operation returns at once, and `try_lock()` and `try_lock_for()`
 * always succeed. Pass it to an API that takes a lock when everything that
 * uses the locked resource runs in one context: one thread, with no
 * interrupt or other thread touching that resource.
 *
 * It provides no mutual exclusion. Shared between threads, or between a
 * thread and an interrupt, it lets both use the resource at the same time.
 *
 * It has no state, so one can serve any number of resources.
 */
export class no_op_lock final : public timed_lock
{
private:
  void driver_lock() override
  {
  }

  void driver_unlock() override
  {
  }

  bool driver_try_lock() override
  {
    return true;
  }

  bool driver_try_lock_for(time_duration) override
  {
    return true;
  }
};

/**
 * @brief concept for basic_lockable
 *
 * @tparam Lock - lock type
 */
export template<class Lock>
concept basic_lockable = requires(Lock& p_lockable) {
  { p_lockable.lock() } -> std::same_as<void>;
  { p_lockable.unlock() } -> std::same_as<void>;
};

/**
 * @brief concept for lockable which extends basic_lockable
 *
 * @tparam Lock - lock type
 */
export template<class Lock>
concept lockable = basic_lockable<Lock> && requires(Lock& p_lockable) {
  { p_lockable.try_lock() } -> std::same_as<bool>;
};

/**
 * @brief concept for timed_lockable which extends lockable
 *
 * @tparam Lock - lock type
 */
export template<class Lock>
concept timed_lockable =
  lockable<Lock> && requires(Lock& p_lockable, time_duration p_duration) {
    { p_lockable.try_lock_for(p_duration) } -> std::same_as<bool>;
  };

// static asserts to check conformance to the concepts
static_assert(basic_lockable<basic_lock>,
              "basic_lock must satisfy basic_lockable");
static_assert(lockable<pollable_lock>, "pollable_lock must satisfy lockable");
static_assert(timed_lockable<timed_lock>,
              "timed_lock must satisfy timed_lockable");
}  // namespace hal::inline v5

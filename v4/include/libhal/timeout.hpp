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

/**
 * @defgroup TimeoutCore Timeout Core
 * @file timeout.hpp
 * @deprecated will be removed by libhal 5.0.0 and replaced with "worker.hpp"
 * @brief Provides the hal::timeout type and utility functions that use that
 * type.
 *
 */
#pragma once

#include "functional.hpp"
#include "units.hpp"

namespace hal {
/**
 * @ingroup TimeoutCore
 * @brief Represents the state of a coroutine or resumable callable
 *
 */
enum class work_state  // NOLINT(performance-enum-size): would break ABI
{
  // Callable is in progress and has not yet finished performing its work.
  in_progress,
  // Callable was able to determine that it failed to do what it was tasked to
  // do and has terminated.
  failed,
  // Callable finished the work it needed to perform and has terminated.
  finished,
};

/**
 * @ingroup TimeoutCore
 * @brief Signature of a function that throws hal::timed_out
 * @deprecated will be removed by libhal 5.0.0
 *
 * A function that expires after a certain amount of time or after a certain
 * amount of events, or after a specific event occurs. When called, this
 * function checks if the expiration event has occurred and if so, throws the
 * exception hal::timed_out.
 *
 * @throws hal::timed_out - to indicate that it has expired
 */
using timeout_function = void() noexcept(false);

/**
 * @brief Concept for a timeout function
 * @deprecated will be removed by libhal 5.0.0
 *
 * @tparam F - type of function
 */
template<class F>
concept timeout = std::convertible_to<F, hal::function_ref<timeout_function>>;

/**
 * @ingroup TimeoutCore
 * @brief A non-blocking callable that performs work with each call
 *
 * Each call to a work_function will perform a set of work. The worker will
 * return a work_state to indicate its current state. Once the worker reaches a
 * terminal state, it MUST perform no additional work and return the terminal
 * state. For example, if a work function failed, it must always return failure
 * and not interact with hardware or other software from that point on. Same
 * will occur for the "finished" state.
 *
 * This function can be repeatedly tried until it has reached a terminal state
 * with the try_until() function.
 *
 * @returns work_state - indicates what the state of the worker function is.
 */
using work_function = work_state();

template<class T>
concept worker = std::convertible_to<T, hal::function_ref<work_function>>;

/**
 * @ingroup TimeoutCore
 * @brief Create a timeout that will never time out
 * @deprecated will be removed by libhal 5.0.0

 * @return auto - callable that will never timeout
 */
inline auto never_timeout()
{
  return []() {};
}
}  // namespace hal

namespace hal::v5 {
/**
 * @ingroup TimeoutCore
 * @brief Represents the state of a coroutine or resumable callable
 *
 */
enum class work_state : u8
{
  // Callable is in progress and has not yet finished performing its work.
  in_progress,
  // Callable was able to determine that it failed to do what it was tasked to
  // do and has terminated.
  failed,
  // Callable finished the work it needed to perform and has terminated.
  finished,
};

/**
 * @ingroup TimeoutCore
 * @brief A non-blocking callable that performs work with each call
 *
 * Each call to a work_function will perform a set of work. The worker will
 * return a work_state to indicate its current state. Once the worker reaches a
 * terminal state, it MUST perform no additional work and return the terminal
 * state. For example, if a work function failed, it must always return failure
 * and not interact with hardware or other software from that point on. Same
 * will occur for the "finished" state.
 *
 * This function can be repeatedly tried until it has reached a terminal state
 * with the try_until() function.
 *
 * @returns work_state - indicates what the state of the worker function is.
 */
using work_function = work_state();

template<class T>
concept worker = std::convertible_to<T, hal::function_ref<work_function>>;

}  // namespace hal::v5

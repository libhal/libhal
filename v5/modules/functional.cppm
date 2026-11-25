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
#include <type_traits>

export module hal:functional;

namespace hal::inline v5 {
// `callable`'s check. It's a concept so that `and` stops at a call that isn't
// valid, before `std::invoke_result_t` would fail to compile.
template<class Callable, class Result, class... Args>
concept invocable_returning =
  std::invocable<Callable&, Args...> and
  std::same_as<std::invoke_result_t<Callable&, Args...>, Result>;

// A primary template that is false keeps a `Signature` that isn't a function
// type from satisfying `callable`.
template<class Callable, class Signature>
constexpr bool matches_signature = false;

template<class Callable, class Result, class... Args>
constexpr bool matches_signature<Callable, Result(Args...)> =
  invocable_returning<Callable, Result, Args...>;

template<class Callable, class Result, class... Args>
constexpr bool matches_signature<Callable, Result(Args...) noexcept> =
  invocable_returning<Callable, Result, Args...> and
  std::is_nothrow_invocable_v<Callable&, Args...>;

/**
 * @brief Satisfied when `Callable` can be called the way `Signature` says
 *
 * `Signature` is a function type, written like the one `hal::callback` or
 * `std::function` takes:
 *
 * - `bool()` takes no arguments and returns exactly `bool`.
 * - `void(hal::u32)` takes a `hal::u32` and returns nothing.
 * - `void() noexcept` also must not throw, for example to be safe to call
 *   from an interrupt.
 *
 * The return type must match exactly: a callable returning `int` or
 * `hal::u32` doesn't satisfy `callable<F, bool()>`. The arguments only have to
 * be passable, so the usual conversions of a function call apply to them.
 *
 * Exact means exact across targets too. `hal::u32` is `unsigned int` on some
 * targets and `unsigned long` on others (arm-none-eabi GCC), so a lambda that
 * returns the literal `5U` doesn't satisfy `callable<F, hal::u32()>`
 * everywhere. Return `hal::u32{ 5 }`, or give the lambda `-> hal::u32`.
 *
 * It's checked as a call through an lvalue (`Callable&`), the way a function
 * calls a callable parameter by name.
 *
 * ```cpp
 * void each_tick(hal::callable<void(hal::u32)> auto&& p_handler);
 *
 * hal::u32 total(hal::callable<bool()> auto&& p_finished);
 * ```
 *
 * @tparam Callable - the type to check, such as a lambda's type
 * @tparam Signature - the function type it must be callable as
 */
export template<class Callable, class Signature>
concept callable = matches_signature<Callable, Signature>;
}  // namespace hal::inline v5

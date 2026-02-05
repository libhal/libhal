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

#include <array>
#include <span>

#include "units.hpp"

namespace hal::v5 {
/**
 * @brief A view over several non‑contiguous memory blocks.
 *
 * `scatter_span<T>` is a alias for `std::span<std::span<T> const>`.  Each
 * element of the outer span points to a distinct block of objects of type `T`.
 * The blocks may come from any memory pool (stack, heap, ROM, etc.) and need
 * not be contiguous with respect to one another. This type is meant for
 * facilitating the reading and writing of multiple non-contiguous blocks of
 * memory to and from peripheral or device memory. The order is considered
 * constant from the view's point of view since interface APIs taking this type
 * should not change the order of the passed span.
 *
 * @tparam T The type of the objects stored in each sub‑span.
 *
 * Typical use case:
 *
 *   ```cpp
 *   std::array<hal::byte, 5>  buf1{...};
 *   std::array<hal::byte, 3>  buf2{...};
 *
 *   // `sp` is `std::array<std::span<hal::byte>, 2>`.
 *   auto sp = hal::make_scatter_array(buf1, buf2);
 *   ```
 *
 * @note **Lifetime** – `scatter_span` does **not** own its data.  All
 * spans it contains must outlive the `scatter_span` object. It is, therefor,
 * recommended to only use this as an input parameter to functions. Such
 * functions SHOULD NOT capture the information outside of their scope, (e.g.
 * class member variable, global/static memory, heap memory).
 *
 * @see make_scatter_array()  // helper that constructs the array
 * @see to_scatter_byte_array()  // convenient overload for `hal::byte`
 */
template<class T>
using scatter_span = std::span<std::span<T> const>;

/**
 * @brief Create an `std::array` of `std::span<T>` from a variable‑length list
 * of arguments.
 *
 * The function accepts any number of arguments that can be converted to
 * `std::span<T>`.  Commonly this means:
 *
 *   * a `std::span<T>`,
 *   * a raw pointer and a size (`T const*`, `size_t`),
 *   * an array (`std::array<T, N>`, `T[N]`, `std::vector<T>`, etc.).
 *
 * The return type is an `std::array<std::span<T>, N>` where
 * `N == sizeof...(Args)`. The returned array is a compile‑time fixed‑size
 * container, making it ideal for APIs that accept a scatter view with a known
 * number of elements.
 *
 * @tparam T    The element type of the sub‑spans.
 * @tparam Args Variadic list of arguments convertible to `std::span<T>`.
 * @param args  The source data blocks.
 * @return constexpr auto  An `std::array` of `std::span<T>`.
 *
 * @par Example
 *
 * ```cpp
 * std::array<hal::byte, 4> a{1,2,3,4};
 * std::vector<hal::byte> v{5,6,7};
 *
 * auto arr = hal::make_scatter_array<hal::byte>(a, v);
 * // arr[0] → span over a
 * // arr[1] → span over v
 * ```
 *
 * @note **Thread‑safety** – The function itself is thread‑safe.
 * The resulting spans simply reference the original data, so
 * thread‑safety depends on the callers’ use of the underlying data.
 *
 * @note **Lifetime** – The returned `std::array` holds spans that
 * reference the caller‑supplied data. The caller must guarantee that
 * those data outlive the array’s use.
 */
template<class T, class... Args>
constexpr auto make_scatter_array(Args&&... args)
{
  return std::array<std::span<T>, sizeof...(Args)>{ std::span<T>(
    std::forward<Args>(args))... };
}

/**
 * @brief Compare two scatter spans of type T are equal regardless of their
 * underlying topologies and sizes.
 *
 * @tparam T - The element type of the sub‑spans.
 * @param lhs - The scatter_span on the left hand side of the expression to be
 * compared against the right hand side's scatter_span.
 *
 * @param rhs - The scatter_span on the right hand side of the expression to be
 * compared against the left hand side's scatter_span.
 *
 * @returns true if the two scatter_spans are equal false if not.
 */
template<typename T>
constexpr bool operator==(scatter_span<T> const& lhs,
                          scatter_span<T> const& rhs)
{
  if (lhs.size() < 1 || rhs.size() < 1) {
    return false;
  }

  std::pair<size_t, size_t> lhs_pos = { 0, 0 };
  std::pair<size_t, size_t> rhs_pos = { 0, 0 };

  std::span<T const> l = lhs[lhs_pos.first];
  std::span<T const> r = rhs[rhs_pos.first];

  bool lhs_finished = false;
  bool rhs_finished = false;

  while (true) {

    if (l[lhs_pos.second] != r[rhs_pos.second]) {
      return false;
    }

    if (lhs_pos.second >= l.size() - 1) {
      lhs_pos.first++;
      lhs_pos.second = 0;
      if (lhs_pos.first > lhs.size() - 1) {
        lhs_finished = true;
      } else {
        l = lhs[lhs_pos.first];
      }

    } else {
      lhs_pos.second++;
    }

    if (rhs_pos.second >= r.size() - 1) {
      rhs_pos.first++;
      rhs_pos.second = 0;
      if (rhs_pos.first > rhs.size() - 1) {
        rhs_finished = true;
      } else {
        r = rhs[rhs_pos.first];
      }
    } else {
      rhs_pos.second++;
    }

    if (lhs_finished || rhs_finished) {
      if (lhs_finished && rhs_finished) {
        return true;
      } else {
        return false;
      }
    }
  }
}

/**
 * @brief Compare two scatter spans of type T are not equal regardless of their
 * underlying topologies and sizes.
 *
 * @tparam T - The element type of the sub‑spans.
 * @param lhs - The scatter_span on the left hand side of the expression to be
 * compared against the right hand side's scatter_span.
 *
 * @param rhs - The scatter_span on the right hand side of the expression to be
 * compared against the left hand side's scatter_span.
 *
 * @returns true if the two scatter_spans are not equal false if they are.
 */
template<typename T>
constexpr bool operator!=(scatter_span<T> const& lhs,
                          scatter_span<T> const& rhs)
{
  return !(lhs == rhs);
}

/**
 * @brief Convenience overload that creates a scatter array for
 * `hal::byte` (i.e. raw bytes).
 *
 * This helper is useful when the API deals with byte buffers,
 * such as USB or serial transmission functions.
 *
 * @tparam Args Variadic list of arguments convertible to
 *              `std::span<hal::byte const>`.
 * @param args  The source byte blocks.
 * @return constexpr auto  An `std::array<std::span<hal::byte const>, N>`.
 *
 * @par Example
 *
 * ```cpp
 * std::array<hal::byte, 4> a{1,2,3,4};
 * std::span<const hal::byte> s{a.data(), a.size()};
 *
 * auto arr = hal::make_scatter_bytes(a, s);
 * // arr[0] → span over a
 * // arr[1] → span over s
 * ```
 *
 * @see make_scatter_array()  // generic form
 */
template<class... Args>
constexpr auto make_scatter_bytes(Args&&... args)
{
  return make_scatter_array<hal::byte const>(std::forward<Args>(args)...);
}

/**
 * @brief Convenience overload that creates a readable scatter array for
 * `hal::byte` (i.e. raw bytes).
 *
 * This helper is useful when the API deals with byte buffers, such as USB or
 * serial transmission functions.
 *
 * @tparam Args - Variadic list of arguments convertible to
 *               `std::span<hal::byte>`.
 * @param args - The source byte blocks.
 * @return constexpr auto  An `std::array<std::span<hal::byte>, N>`.
 *
 * @par Example
 *
 * ```cpp
 * std::array<hal::byte, 4> a{1,2,3,4};
 * std::span<hal::byte> s{a.data(), a.size()};
 *
 * auto arr = hal::make_writable_scatter_bytes(a, s);
 * // arr[0] → span over a
 * // arr[1] → span over s
 *
 * std::span<hal::byte const> const_data{a.data(), a.size()};
 *
 * // ERROR: ❌ one of the arrays is const and not writeable.
 * auto arr = hal::make_writable_scatter_bytes(a, s, const_data);
 * ```
 *
 * @see make_scatter_array()  // generic form
 */
template<class... Args>
constexpr auto make_writable_scatter_bytes(Args&&... args)
{
  return make_scatter_array<hal::byte>(std::forward<Args>(args)...);
}

template<typename T>
concept spanable = requires(T const& t) {
  { std::span(t) };
};

template<typename T>
concept spanable_bytes = requires(T& t) {
  { std::span<hal::byte const>(t) };
};

template<typename T>
concept spanable_writable_bytes = requires(T& t) {
  { std::span<hal::byte>(t) };
};
}  // namespace hal::v5

namespace hal {
using v5::make_scatter_array;
using v5::make_scatter_bytes;
using v5::make_writable_scatter_bytes;
using v5::scatter_span;
using v5::spanable;
using v5::spanable_bytes;
using v5::spanable_writable_bytes;
using v5::operator==;
using v5::operator!=;
}  // namespace hal

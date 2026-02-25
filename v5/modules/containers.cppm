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
#include <span>

export module hal:containers;

export import :units;

namespace hal::inline v5 {

/**
 * @brief Concept that checks whether `std::span<T>` can be constructed from
 * `U`.
 *
 * This is the reverse of `std::constructible_from<U, std::span<T>>`: it asks
 * "can I build a `std::span<T>` out of a `U`?" rather than "can I build a `U`
 * out of a `std::span<T>`?".
 *
 * Satisfied by any type that `std::span<T>` accepts as a constructor argument,
 * including raw arrays, `std::array`, `std::vector`, and other contiguous
 * ranges, as well as `std::span<std::remove_const_t<T>>` when `T` is const.
 *
 * @tparam U The candidate source type.
 * @tparam T The element type of the target `std::span`.
 */
export template<typename U, typename T>
concept span_constructible = std::constructible_from<std::span<T>, U>;

/**
 * @brief A non-owning view over a contiguous sequence that wraps indices
 * modulo the sequence length.
 *
 * `circular_span<T>` wraps a `std::span<T>` and overloads `operator[]` so
 * that any index is reduced modulo the span's size before the underlying
 * element is accessed. This provides a convenient ring-buffer view without
 * allocating or copying data.
 *
 * @tparam T The element type. May be `const`-qualified (e.g.
 *           `circular_span<hal::byte const>`) to obtain a read-only view.
 *
 * @par Example
 *
 * ```cpp
 * std::array<int, 4> buf{ 10, 20, 30, 40 };
 * hal::circular_span<int> cs(buf);
 *
 * cs[0]  // 10
 * cs[3]  // 40
 * cs[4]  // 10  — wraps around
 * cs[7]  // 40  — wraps around
 * ```
 *
 * @note **Ownership** – `circular_span` does **not** own its data. The
 * underlying storage must outlive the `circular_span` object.
 *
 * @note **Empty span** – Calling `operator[]` on a `circular_span` built
 * from an empty span is undefined behaviour (division by zero in the modulo
 * operation).
 */
export template<class T>
class circular_span
{
public:
  /**
   * @brief Construct from an existing `std::span<T>`.
   *
   * @param p_span The span to wrap.
   */
  constexpr circular_span(std::span<T> p_span)
    : m_span(p_span)
  {
  }

  /**
   * @brief Construct from any type that `std::span<T>` can be built from.
   *
   * Accepts raw arrays, `std::array`, `std::vector`, and other contiguous
   * ranges, as well as `std::span<std::remove_const_t<T>>` when `T` is
   * const-qualified.
   *
   * @param p_span A value convertible to `std::span<T>`.
   */
  constexpr circular_span(span_constructible<T> auto& p_span)
    : m_span(p_span)
  {
  }

  /**
   * @brief Access an element at a wrapped index.
   *
   * The index is reduced modulo `size()` so it always refers to a valid
   * element regardless of magnitude.
   *
   * @param p_index Logical index (any non-negative value).
   * @return Reference to the element at `p_index % size()`.
   */
  constexpr T& operator[](hal::usize p_index) const
  {
    return m_span[p_index % m_span.size()];
  }

  /**
   * @brief Return the number of elements in the underlying span.
   *
   * @return Element count.
   */
  constexpr auto size() const
  {
    return m_span.size();
  }

  /**
   * @brief Return the underlying `std::span<T>`.
   *
   * @return A copy of the wrapped span.
   */
  constexpr auto span()
  {
    return m_span;
  }

private:
  std::span<T> m_span{};
};
}  // namespace hal::inline v5
